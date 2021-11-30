//
// Created by Yann Holme Nielsen on 14/06/2020.
//

#include "../include/torasu/mod/imgc/MediaDecoder.hpp"

#include <math.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <lodepng.h>

#include <torasu/log_tools.hpp>
#include <torasu/std/LIcore_logger.hpp>

#define DECODER_SANITY_CHECKS true
#define DEBUG_BOUND_HITS false
#define DEBUG_GENERAL_INFO false
#define DEBUG_CACHE_EVENTS false
#define DEBUG_SEEKS false

using namespace std;
namespace {

int ReadFunc(void* ptr, uint8_t* buf, int buf_size) {
	imgc::MediaDecoder::FileReader* reader = reinterpret_cast<imgc::MediaDecoder::FileReader*>(ptr);

	size_t nextPos = reader->pos + buf_size;
	size_t read;

	if (nextPos <= reader->size) {
		read = buf_size;
	} else {
		nextPos = reader->size;
		read = nextPos - reader->pos;
	}
	// std::cout << "READ TO " << ((void*)buf) << "-" << ((void*)(buf+read)) << " (SIZE " << read << ")" << std::endl;
	memcpy(buf, reader->data + reader->pos, read);
	reader->pos += read;
	if (read > 0) {
		return read;
	} else {
		return AVERROR_EOF;
	}
}

int64_t SeekFunc(void* ptr, int64_t pos, int whence) {
	imgc::MediaDecoder::FileReader* reader = reinterpret_cast<imgc::MediaDecoder::FileReader*>(ptr);
	switch (whence) {
	case SEEK_SET:
		// std::cout << "SEEK SET " << pos << std::endl;
		reader->pos = pos;
		break;
	case SEEK_CUR:
		// std::cout << "SEEK CUR " << reader->pos << " + " << pos << " = " << (reader->pos+pos) << std::endl;
		reader->pos += pos;
		break;
	case SEEK_END:
		// std::cout << "SEEK END " << reader->size << " + " << pos << " = " << (reader->size+pos) << std::endl;
		reader->pos = reader->size + pos;
		break;
	case AVSEEK_SIZE:
		// std::cout << "SEEK SIZE -> " << reader->size << std::endl;
		return reader->size;
	default:
		break;
	}

	// Return the new position:
	return reader->pos;
}

#if DEBUG_CACHE_EVENTS
void debug_print_cache_event(std::string text, imgc::StreamEntry* entry) {
	std::cout << text << " - Current Cache Entries [";
	bool first = true;
	for (auto cachedFrame : entry->cachedFrames) {
		if (first) {
			first = false;
		} else {
			std::cout << ", ";
		}
		std::cout << cachedFrame.startTime;
	}
	std::cout << "]" << std::endl;
}
#endif

#if DEBUG_BOUND_HITS
void debug_print_bound_match_event(std::string text, imgc::StreamEntry* stream, int64_t targetPosition, int64_t targetPositionEnd) {
	std::cout << text << " - PPOS " << stream->frame->pkt_pos
			  << " FP " << stream->frame->pts << " FE " << (stream->frame->pts+stream->frame->pkt_duration)
			  << " TP " << targetPosition << " TE " << targetPositionEnd << std::endl;
}
#endif

}

namespace imgc {

MediaDecoder::MediaDecoder(std::string path) {
	ifstream is = ifstream(path.c_str());
	is.seekg(0, is.end);
	size_t length = is.tellg();
	is.seekg(0, is.beg);
	uint8_t* data = new uint8_t[length];
	is.read(reinterpret_cast<char*>(data), length);
	is.close();
	in_stream.data = data;
	in_stream.pos = 0;
	in_stream.size = length;
	freeInput = true; // delete[] in_stream.data
	std::unique_ptr<torasu::tstd::LIcore_logger> logger(new torasu::tstd::LIcore_logger());
	torasu::LogInstruction li(logger.get());
	LogInstructionSession logSess(this, &li);
	prepare();
}

MediaDecoder::MediaDecoder(uint8_t* dataP, size_t len, torasu::LogInstruction li) {
	in_stream.data = dataP;
	in_stream.pos = 0;
	in_stream.size = len;
	freeInput = false;
	LogInstructionSession logSess(this, &li);
	prepare();
}

void MediaDecoder::prepare() {
	auto* instance = this;
	FFmpegCallbackFunc logCallback = [instance] (void* avcl, int level, const char* msg, va_list vl) {
		auto* li = instance->li;
		if (li != nullptr) {
			FFmpegLogger::logMessageChecked(*li, avcl, level, msg, vl, 500);
		} else {
			std::cout << "MediaDecoder: (Unexpected message) "
					  << FFmpegLogger::formatLine(avcl, level, msg, vl, 500) << std::endl;
		}
	};

	uint8_t* alloc_buf = (uint8_t*) av_malloc(32 * 1024);
	// std::cout << "IOB-ALOC: " << ((void*)alloc_buf) << std::endl;
	av_format_ctx = avformat_alloc_context();
	if (!av_format_ctx) {
		throw runtime_error("Failed allocating av_format_ctx!");
	}

	loggers.emplace_back().attach(av_format_ctx, logCallback);
	av_format_ctx->pb = avio_alloc_context(alloc_buf, 32 * 1024, false, &in_stream, ReadFunc, NULL, SeekFunc);

	if (!av_format_ctx->pb) {
		av_free(alloc_buf);
		throw runtime_error("Failed allocating avio_context!");
	}
	av_format_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
	auto openRet = avformat_open_input(&av_format_ctx, "", NULL, NULL);
	if (openRet != 0) {
		throw runtime_error("Failed to open input - avformat_open_input: ERR#" + std::to_string(openRet) + " " + getErrorMessage(openRet));
	}

	// Get infromation about streams (Somehow doesn't seem necessary)
	// if (avformat_find_stream_info(av_format_ctx, NULL) < 0) {
	// 	throw runtime_error("Failed to find audio_out_stream info");
	// }

	av_packet = av_packet_alloc();
	// std::cout << "PKT-ALOC: " << av_packet << std::endl;

	for (unsigned int stream_index = 0; stream_index < av_format_ctx->nb_streams; stream_index++) {
		AVStream* stream = av_format_ctx->streams[stream_index];
		StreamEntry* entry = new StreamEntry();
		entry->index = stream->index;
		entry->codecType = stream->codecpar->codec_type;
		entry->codec = avcodec_find_decoder(stream->codecpar->codec_id);
		entry->ctx = avcodec_alloc_context3(entry->codec);
		entry->ctx_params = stream->codecpar;
		entry->base_time = stream->time_base;

		loggers.emplace_back().attach(entry->ctx, logCallback);

		// Currently disabled since av_format_ctx->duration is not valid because stream-scanning was disabled
		// if (stream->duration == AV_NOPTS_VALUE) {
		// 	entry->duration = av_format_ctx->duration * stream->time_base.den / stream->time_base.num / AV_TIME_BASE;
		// } else {
		// 	entry->duration = stream->duration;
		// }

		entry->duration = stream->duration;

		if (stream->duration != AV_NOPTS_VALUE
				&& ((double) this->duration.num / this->duration.den) < ((double) stream->duration*stream->time_base.num / stream->time_base.den)) {
			this->duration = {stream->duration* stream->time_base.num, stream->time_base.den};
		}

		// Sets pkt_timebase so timestamps can be corrected (anyone knows why this isnt set by default?)
		// Needs this to update timestamps for skipped samples - fixes "Could not update timestamps for skipped samples."-message
		entry->ctx->pkt_timebase = entry->base_time;

		if (entry->codec) {
			if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
				video_stream_index = entry->index;
				entry->vid_delay = stream->codecpar->video_delay;
				entry->vid_fps = stream->r_frame_rate;
			} else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
				audio_stream_index = entry->index;
			}

		} else {
			torasu::tools::log_checked(*li, torasu::WARN, "CANT FIND CODEC FOR STREAM "
									   + std::to_string(stream->id)
									   + " (TYPE " + std::to_string(stream->codecpar->codec_type) + ")");
		}

		if (!entry->ctx) {
			throw runtime_error("Failed to allocate av_codec_ctx!");
		}
		if (avcodec_parameters_to_context(entry->ctx, entry->ctx_params) < 0) {
			throw runtime_error("Failed to link parameters to context!");
		}
		if (avcodec_open2(entry->ctx, entry->codec, NULL) < 0) {
			throw runtime_error("Failed to initialize av_codec_ctx with av_codec!");
		}
		entry->frame = av_frame_alloc();
		if (!entry->frame) {
			throw runtime_error("Failed to allocate av_frame");
		}
		streams.push_back(entry);
	}
}

MediaDecoder::~MediaDecoder() {

	if (sws_scaler_ctx != nullptr) {
		sws_freeContext(sws_scaler_ctx);
	}

	if (scaled_frame != nullptr) {
		av_frame_free(&scaled_frame);
	}

	for (unsigned long i = 0; i < streams.size(); ++i) {
		auto stream = streams[i];
		if (stream->frame != nullptr) {
			av_frame_free(&stream->frame);
		}
		if (stream->ctx != nullptr) {
			avcodec_free_context(&stream->ctx);
		}
		delete stream;
	}

	// std::cout << "PKT-FREE: " << av_packet << std::endl;
	av_packet_free(&av_packet);
	if (av_format_ctx->pb) {
		av_free(av_format_ctx->pb->buffer);
		avio_context_free(&av_format_ctx->pb);
	}
	avformat_close_input(&av_format_ctx);

	if (freeInput) delete[] in_stream.data;

}

std::pair<int32_t, int32_t> MediaDecoder::getDimensions() {
	StreamEntry* stream = getStreamEntryByIndex(video_stream_index);
	if (stream == nullptr) {
		return std::pair<int32_t, int32_t>(-1, -1);
	}
	return std::pair<int32_t, int32_t>(stream->ctx->width, stream->ctx->height);
}

double MediaDecoder::getDuration() {
	return (double) this->duration.num / this->duration.den;
}

void MediaDecoder::flushBuffers(StreamEntry* entry) {
	avcodec_flush_buffers(entry->ctx);
	entry->cachedFrames.clear();
	entry->flushCount = 0;
}

StreamEntry* MediaDecoder::getStreamEntryByIndex(int index) {
	for (unsigned long i = 0; i < streams.size(); ++i) {
		auto entry = streams[i];
		if (entry->index == index) {
			return entry;
		}
	}
	return nullptr;
}

void MediaDecoder::removeCacheFrame(int64_t pos, std::vector<BufferedFrame>* list) {
	for (auto it = list->begin(); it != list->end(); ++it) {
		BufferedFrame entry = *it;
		if (entry.pos == pos) {
			list->erase(it);
			return;
		}

	}
	std::cerr << "Missing frame that should've been added" << std::endl;
}

// Use AVFrane for scaling, instead of using manual parameters
#define VID_FRAME_SCALE_AVFRAME true
// First convert frame into libav-buffer (with a padding for swsscale) and then into the destination-buffer, instead doing a direct conversion into
#define VID_FRAME_SCALE_INDIRECT true

void MediaDecoder::extractVideoFrame(StreamEntry* stream, uint8_t* outPt) {
	int32_t rWidth = stream->frame->width;
	int32_t rHeight = stream->frame->height;
	if (sws_scaler_ctx == nullptr) {
		sws_scaler_ctx = sws_getContext(stream->frame->width, stream->frame->height, stream->ctx->pix_fmt,
										rWidth, rHeight, AV_PIX_FMT_RGB0,
										SWS_FAST_BILINEAR, NULL, NULL, NULL);

		if (!sws_scaler_ctx) throw runtime_error("Failed to create sws_scaler_ctx!");
	}

#if VID_FRAME_SCALE_AVFRAME
	int status;
	if (scaled_frame == nullptr) {
		scaled_frame = av_frame_alloc();
		if (!scaled_frame) throw std::runtime_error("Failed to allocate scaled-frame!");

		scaled_frame->format = AV_PIX_FMT_RGB0;
		scaled_frame->width  = rWidth;
		scaled_frame->height = rHeight;
		status = av_frame_get_buffer(scaled_frame, 0);
		if (status != 0) throw std::runtime_error("Error allocating destination frame on scale! (" + std::to_string(status) + ")");
	}

	status = sws_scale(sws_scaler_ctx, stream->frame->data, stream->frame->linesize, 0, rHeight, scaled_frame->data,
					   scaled_frame->linesize);
	if (status < 0) throw std::runtime_error("Error scaling frame! (" + std::to_string(status) + ")");

	uint8_t* linePtr = scaled_frame->data[0];
	uint8_t* outPtr = outPt;
	for (int32_t y = 0; y < rHeight; y++) {
		std::copy(linePtr, linePtr + rWidth*4, outPtr);
		linePtr += scaled_frame->linesize[0];
		outPtr += rWidth*4;
	}

#else
#if VID_FRAME_SCALE_INDIRECT
	size_t bufferSize = rWidth*rHeight*4;
	// A padding for sws scale, since it seems to write outside of the designated are
	// Issue was found on a conversion from 1000x1200 AV_PIX_FMT_YUV420P -> 1000x1200 AV_PIX_FMT_RGB0
	constexpr int SWS_BOUND = 1;
	uint8_t* writeBuffer = reinterpret_cast<uint8_t*>( av_malloc(bufferSize + rWidth*SWS_BOUND*4 + (rHeight+SWS_BOUND)*SWS_BOUND*4) );
	uint8_t* dst[4] = {writeBuffer, NULL, NULL, NULL};
#else
	uint8_t* dst[4] = {outPt, NULL, NULL, NULL};
#endif
	int dest_linesize[4] = {rWidth * 4, 0, 0, 0};
	sws_scale(sws_scaler_ctx, stream->frame->data, stream->frame->linesize, 0, stream->frame->height, dst,
			  dest_linesize);


#if VID_FRAME_SCALE_INDIRECT
	std::copy(writeBuffer, writeBuffer+bufferSize, outPt);
	av_free(writeBuffer);
#endif
#endif

}

int64_t MediaDecoder::toBaseTime(double value, AVRational base) {
	return round((value * base.den) / base.num);
}

void MediaDecoder::getSegment(SegmentRequest request) {
	std::unique_ptr<torasu::tstd::LIcore_logger> logger(new torasu::tstd::LIcore_logger());
	torasu::LogInstruction li(logger.get());
	getSegment(request, li);
}

void MediaDecoder::getSegment(SegmentRequest request, torasu::LogInstruction li) {
	LogInstructionSession logSess(this, &li);

	DecodingState* decodingState = createDecoderState(request);
#if DEBUG_GENERAL_INFO
	std::cout << "DECODE-TASK: start " << decodingState->requestStart << " end " << decodingState->requestEnd
			  << " TYPES: " << (!decodingState->videoDone ? "V" : "") << (!decodingState->audioDone ? "A" : "")
			  << std::endl << std::flush;
#endif

	fetchBuffered(decodingState);
	initializePosition(decodingState);

	while (true) { // DECODE-LOOP
		if ((decodingState->videoDone||!decodingState->videoAvailable) && (decodingState->audioDone || !decodingState->audioAvailable)) {
			break;
		}
		// std::cout << "PKT-UREF: " << av_packet << " DAT: " << ((void*)av_packet->data) << std::endl;
		av_packet_unref(av_packet);
		int nextFrameStat = av_read_frame(av_format_ctx, av_packet);
		// std::cout << "PKT-READ: " << av_packet << " DAT: " << ((void*)av_packet->data) << std::endl;
		auto stream = getStreamEntryByIndex(av_packet->stream_index);
		if (nextFrameStat == AVERROR_EOF) {
			for(unsigned long i = 0; i < streams.size(); i++) {
				drainStream(streams[i], decodingState, true);
			}
			break;
		} else if (nextFrameStat < 0) {
			throw runtime_error(std::string("Error ") + std::to_string(nextFrameStat) + std::string(" (") + getErrorMessage(nextFrameStat) + std::string(") occurred while reading frame!") );
		}
		bool packetIsPostponed = false;
		int response = avcodec_send_packet(stream->ctx, av_packet);
		if (response == AVERROR(EAGAIN)) {
			packetIsPostponed = true;
		} else if (response == AVERROR(ENOMEM)) {
			throw runtime_error("Send packet returned ENOMEM");
		} else if (response < 0) {
			throw runtime_error(std::string("Error ") + std::to_string(nextFrameStat) + std::string(" (") + getErrorMessage(nextFrameStat) + std::string(") occurred while sending frame to decoder!") );
		}


		if (!stream->pushy && av_packet->duration == 0) {
			// Enable pushy if there are errors retrieving a duration
			stream->pushy = true;
		}

		// Dont save packets that with negative pts to cache, since they wont come out (just observed, no proofed rule)
		if (av_packet->pts >= 0) {
			auto fr = BufferedFrame{av_packet->pts, av_packet->pos, av_packet->duration};
			stream->cachedFrames.push_back(fr);
#if DEBUG_CACHE_EVENTS
			debug_print_cache_event("Added Cached frame " + std::to_string(av_packet->pts), stream);
#endif
		}

		// Pushy Behavior: Fill codec until EGAIN
		if (stream->pushy && !packetIsPostponed) continue;

		for (;;) { // RECIEVE-LOOP
			// Read next frame (if available)
			response = avcodec_receive_frame(stream->ctx, stream->frame);
			if (response == AVERROR(EAGAIN)) {
				if (stream->flushCount > 0) {
					stream->flushCount--;
				}
				break;
			} else if (response < 0) {
				throw runtime_error(std::string("Error ") + std::to_string(nextFrameStat) + std::string(" (") + getErrorMessage(nextFrameStat) + std::string(") occurred while recieving frame from decoder!") );
			}
			stream->frameIsPresent = true;
			removeCacheFrame(stream->frame->pkt_pos, &stream->cachedFrames);
#if DEBUG_CACHE_EVENTS
			debug_print_cache_event("Removed cached frame for " + std::to_string(stream->frame->pts), stream);
#endif

			if (stream->flushCount > 0) {
				stream->flushCount--;
			} else {
				handleFrame(stream, decodingState);
			}

			if (!packetIsPostponed) break; // Done reading once postponed has been read

			// Try sending postponed packet
			response = avcodec_send_packet(stream->ctx, av_packet);
			if (response == AVERROR(EAGAIN)) {
				continue; // Continue reading until packet can be sent
			} else if (response == AVERROR(ENOMEM)) {
				throw runtime_error("Send packet returned ENOMEM");
			} else if (response < 0) {
				throw runtime_error(std::string("Error ") + std::to_string(nextFrameStat) + std::string(" (") + getErrorMessage(nextFrameStat) + std::string(") occurred while sending frame to decoder!") );
			}
			packetIsPostponed = false;
			break;
		} // END RECIEVE-LOOP

	} // END DECODE-LOOP

	if (!decodingState->audioFrames.empty()) {
		concatAudio(decodingState);
	} else if (request.audioBuffer != NULL) {
		std::vector<uint8_t*> data;

		if (!decodingState->audioAvailable) {
			*request.audioBuffer = new torasu::tstd::Daudio_buffer(1, 1, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32, 4, 0);
		} else {
			auto audioStream = getStreamEntryByIndex(audio_stream_index);
			*request.audioBuffer = new torasu::tstd::Daudio_buffer(audioStream->ctx->channels, audioStream->ctx->sample_rate, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32, determineSampleSize(audioStream), 0);
		}
	}
	// TODO Distruct decoding state correctly

	delete decodingState;
}

bool MediaDecoder::checkFrameTargetBound(AVFrame* frame, int64_t start, int64_t end) {
	return ( (frame->pts < end) && (frame->pts >= start || frame->pts + frame->pkt_duration > start) ) || (frame->pts == start);
}

void MediaDecoder::handleFrame(StreamEntry* stream, DecodingState* decodingState) {
	if (stream->frame->pkt_duration <= 0) {

		// Get start of next frame
		int64_t nextFrameTime = INT64_MAX;
		for (auto cachedFrame : stream->cachedFrames) {
			if (nextFrameTime > cachedFrame.startTime && cachedFrame.startTime > stream->frame->pts) {
				nextFrameTime = cachedFrame.startTime;
			}
		}
		if (nextFrameTime != INT64_MAX) {
			// Calculate duration from distance between own start and start of other frame
			stream->frame->pkt_duration = nextFrameTime - stream->frame->pts;
#if DEBUG_CACHE_EVENTS
			debug_print_cache_event("Found cached frame after " + std::to_string(stream->frame->pts)
									+ " setting duration " + std::to_string(stream->frame->pkt_duration), stream);
#endif
		} else {
#if DEBUG_CACHE_EVENTS
			debug_print_cache_event("Found no next frame after " + std::to_string(stream->frame->pts), stream);
#endif
		}
		// Add mechanism to wait on the next frame-packet from the file
		// if this may be reached without the next frame-packet already read
	}
	stream->nextFramePts = stream->frame->pts + stream->frame->pkt_duration;

	int64_t targetPosition = toBaseTime(decodingState->requestStart, stream->base_time);
	int64_t targetPositionEnd = toBaseTime(decodingState->requestEnd, stream->base_time);

	/* if(targetPositionEnd > stream->duration) {
		// FAIL safe, forced the wanted end pos to the start of last available frame of the stream
		targetPositionEnd = stream->duration - stream->frame->pkt_duration;
	} */

	if (!decodingState->videoDone && stream->index == video_stream_index) {
		// Check if frame is inside of range or in the case of a requesting contents after the stream-limit, if the frame is the last one
		if (checkFrameTargetBound(stream->frame, targetPosition, targetPositionEnd)
				|| ( (targetPosition >= stream->duration) && (stream->frame->pts+stream->frame->pkt_duration) == stream->duration) ) {
			if (decodingState->videoReadUntil > stream->frame->pts) {
#if DEBUG_BOUND_HITS
				debug_print_bound_match_event("VID BOUND HIT BUT ALREADY READ", stream, targetPosition, targetPositionEnd);
#endif
				return;
			}
#if DEBUG_BOUND_HITS
			debug_print_bound_match_event("VID BOUND HIT", stream, targetPosition, targetPositionEnd);
#endif
			int32_t rWidth = stream->frame->width;
			int32_t rHeight = stream->frame->height;

			uint8_t* target = (**decodingState->segmentRequest.videoBuffer).addFrame(
								  ((double)(stream->frame->pts * stream->base_time.num)) / stream->base_time.den,
								  torasu::tstd::Dbimg_FORMAT(rWidth, rHeight))->getImageData();
			// For testing purposes
			// std::fill(target, target+rWidth*rHeight*4, 0x00);
			// std::cout << "extracting video frame to " << ((void*)target) << " - " << ((void*)(target+rWidth*rHeight*4-1)) << std::endl;
			extractVideoFrame(stream, target);

			decodingState->videoReadUntil = stream->frame->pts + stream->frame->pkt_duration;
		}
#if DEBUG_BOUND_HITS
		else {
			debug_print_bound_match_event("VID BOUND MISS", stream, targetPosition, targetPositionEnd);
		}
#endif

		if (targetPositionEnd <= stream->nextFramePts && targetPosition != stream->nextFramePts) {
			decodingState->videoDone = true;
		}

	}

	if (!decodingState->audioDone && stream->index == audio_stream_index) {
		if (checkFrameTargetBound(stream->frame, targetPosition, targetPositionEnd)) {
			if (!decodingState->audioFrames.empty() && decodingState->audioFrames.rbegin()->end > stream->frame->pts) {
				return;
			}
			std::vector<uint8_t*> data;
			static const size_t sampleSize = 4;

			// How big the audio-frame should be
			size_t memSize = stream->frame->pkt_duration * stream->ctx->sample_rate
							 * stream->base_time.num / stream->base_time.den * sampleSize;
			// How big the audio-frame actually is
			size_t dataSize = stream->frame->nb_samples * sampleSize;

			for (int i = 0; i < stream->frame->channels; ++i) {
				auto* cp = new uint8_t[memSize];
				int dataGap = memSize-dataSize;
				if (dataGap < 0) {
					// Too much data inside the packet: Crop at the end
					dataGap = 0;
					dataSize = memSize;
				}
				std::copy(stream->frame->extended_data[i], stream->frame->extended_data[i]+dataSize, cp+dataGap);
				std::fill(cp, cp+dataGap, 0x00);
				data.push_back(cp);
			}
			auto part = AudioFrame{stream->frame->pts, stream->frame->pts + stream->frame->pkt_duration,
								   stream->frame->nb_samples, memSize, data};
			decodingState->audioFrames.push_back(part);
		}

		if (targetPositionEnd <= stream->nextFramePts) {
			decodingState->audioDone = true;
		}

	}
}

void MediaDecoder::fetchBuffered(DecodingState* decodingState) {

	if (!decodingState->videoDone) {
		auto vidStream = getStreamEntryByIndex(video_stream_index);
		int64_t vidTargetPosition = toBaseTime(decodingState->requestStart, vidStream->base_time);

		if (!vidStream->frameIsPresent) {
			return;
		}

		auto vidFrameStart = vidStream->frame->pts;

		if (vidFrameStart <= vidTargetPosition) {
			handleFrame(vidStream, decodingState);
		}
	}

	if (!decodingState->audioDone) {
		auto audStream = getStreamEntryByIndex(audio_stream_index);
		int64_t audTargetPosition = toBaseTime(decodingState->requestStart, audStream->base_time);

		if (!audStream->frameIsPresent) {
			return;
		}
		auto audFrameStart = audStream->frame->pts;

		if (audFrameStart <= audTargetPosition) {
			handleFrame(audStream, decodingState);
		}

	}

}


void MediaDecoder::initializePosition(DecodingState* decodingState) {

	bool videoSeekBack = false;
	bool videoSeekForward = false;
	if (!decodingState->videoDone && decodingState->videoAvailable) {
		auto vidStream = getStreamEntryByIndex(video_stream_index);
		int64_t vidTargetPosition = decodingState->videoReadUntil == INT64_MIN ?
									toBaseTime(decodingState->requestStart, vidStream->base_time) : decodingState->videoReadUntil;

		int64_t videoPlayHead = vidStream->nextFramePts >= 0 ?
								vidStream->nextFramePts : INT64_MAX;

		videoSeekBack = videoPlayHead > vidTargetPosition;


		videoSeekForward = !videoSeekBack && vidTargetPosition > 0;

		if (videoSeekForward) {

			int64_t cacheEnd = 0;
			for (auto& cachedFrame : vidStream->cachedFrames) {
				int64_t currentFrameEnd = cachedFrame.startTime + cachedFrame.duration;
				if (cacheEnd < currentFrameEnd) {
					cacheEnd = currentFrameEnd;
				}
			}

			if (cacheEnd > vidTargetPosition) {
				// Dont seek forward, since frames are already located in the buffer
				videoSeekForward = false;
			}

		}

	}
	bool audioSeekBack = false;
	if (!decodingState->audioDone && decodingState->audioAvailable) {

		auto audStream = getStreamEntryByIndex(audio_stream_index);

		int64_t audTargetPosition = decodingState->audioFrames.empty() ?
									toBaseTime(decodingState->requestStart, audStream->base_time) : decodingState->audioFrames.rbegin()->end;

		int64_t audPlayHead = audStream->nextFramePts >= 0 ?
							  audStream->nextFramePts : INT64_MAX;

		audioSeekBack = (audPlayHead > audTargetPosition);

	}

	// TODO Seek inidividual streams if required

	if (videoSeekBack || audioSeekBack || videoSeekForward) {
#if DEBUG_SEEKS
		std::cout << "SEEK VB " << videoSeekBack << " AB " << audioSeekBack << " VF " << videoSeekForward << " TO REACH START " << decodingState->requestStart << std::endl;
#endif
		double position = decodingState->requestStart;

		// Seek to
		if (decodingState->audioAvailable) {
			auto audStream = getStreamEntryByIndex(audio_stream_index);
			// For some reason the audStream->ctx_params->seek_preroll was not enough in practice
			// This is we also seek back two frames more, which seemd to fix the issue on testing with a mp3
			// Is audStream->ctx_params->seek_preroll maybe not set correctly? - If someone finds a better solution, help is appreciated!
			double audioSeekPadding = (double)(audStream->ctx->frame_size*2 + audStream->ctx->seek_preroll +1) / audStream->ctx->sample_rate;
			if (position > audioSeekPadding) {
				position -= audioSeekPadding;
			} else {
				position = 0;
			}
		}

		av_seek_frame(av_format_ctx, -1, position * AV_TIME_BASE,
					  AVSEEK_FLAG_BACKWARD); //Assuming the -1 includes all streams
		// Flushing those without handling them is vital,
		// because some systems depend on that one of the cachedFrames to come out of the decoder next
		// for example if a frame does have no duration the next frame in cachedFrames, may be used to calculate the duration
		for (auto* stream : streams) {
			drainStream(stream, decodingState, false);
		}
	}

}


void MediaDecoder::drainStream(StreamEntry* stream, DecodingState* decodingState, bool record) {

#if DEBUG_CACHE_EVENTS
	std::cout << "DRAIN STREAM " << stream->index << " RECORD " << record << std::endl;
#endif
	int drainingRequest = avcodec_send_packet(stream->ctx, NULL);
	if(drainingRequest != 0) return;
	stream->draining = true;

	AVFrame* flushedFrame = av_frame_alloc();
	if (flushedFrame == nullptr) throw runtime_error("Failed to allocate av_frame (flush)");
	std::unique_ptr<AVFrame*, decltype(av_frame_free)*> flushedFrameDeleter(&flushedFrame, av_frame_free);

	while(true) {
		int response = avcodec_receive_frame(stream->ctx, flushedFrame);
		if (response == AVERROR(EAGAIN)) {
			continue;
		}

		bool ended = response == AVERROR_EOF;

		if (!ended && response != 0) {
			throw runtime_error("non 0 response code from receive frame while draining");
		}

		if (!ended) {
			{
				// Swap once accepted
				AVFrame* nextBuffer = stream->frame;
				stream->frame = flushedFrame;
				flushedFrame = nextBuffer;
			}

			removeCacheFrame(stream->frame->pkt_pos, &stream->cachedFrames);
#if DEBUG_CACHE_EVENTS
			debug_print_cache_event("[Drain] Removed cached frame for " + std::to_string(stream->frame->pts), stream);
#endif
			if (record) {
				handleFrame(stream, decodingState);
			}
		} else {
			if (record) {
				if (stream->codecType == AVMEDIA_TYPE_VIDEO) {
					// Duplicate last video-frame if last frame is not long enough
					int64_t endOffset = stream->duration-(stream->frame->pts+stream->frame->pkt_duration);
					if (endOffset > 0) {
						stream->frame->pts += stream->frame->pkt_duration;
						stream->frame->pkt_duration = endOffset;
						handleFrame(stream, decodingState);
					}
				}
			}
			stream->frameIsPresent = false;
			break;
		}

	}

	if (!stream->cachedFrames.empty()) {
		// FIXME tracking frames which went into the pipe is really unreliable since they might not come out again!
		// throw std::logic_error("Panic: Cached frames still are in the pipe after drain!");
	}

	flushBuffers(stream);
	stream->draining = false;
}

void MediaDecoder::concatAudio(DecodingState* decodingState) {
	auto* audioStream = getStreamEntryByIndex(audio_stream_index);

	auto* audioCtx = audioStream->ctx;
	auto audioBaseTime = audioStream->base_time;
	int channelCount = audioCtx->channels;
	int sampleRate = audioCtx->sample_rate;

	size_t sampleSize = determineSampleSize(audioStream);

	// Channel-size in samples
	int channelSize = round(decodingState->requestEnd * sampleRate) - round(decodingState->requestStart * sampleRate);
	*decodingState->segmentRequest.audioBuffer = new torasu::tstd::Daudio_buffer(channelCount, sampleRate, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32, sampleSize, channelSize * sampleSize);

	auto* channels = (*decodingState->segmentRequest.audioBuffer)->getChannels();

	// Requested start in base-time
	int64_t requestStartBased = toBaseTime(decodingState->requestStart, audioBaseTime);

	int64_t lastWrittenPos = 0; // Until which position has been written (exclusive)

	for (AudioFrame& frame : decodingState->audioFrames) {
		int64_t copySrcStart, copySrcEnd, copyDestPos;

		copyDestPos = (frame.start-requestStartBased) * audioBaseTime.num * sampleRate / audioBaseTime.den;
		copySrcEnd = (frame.end - frame.start) * audioBaseTime.num * sampleRate / audioBaseTime.den;

		if (copyDestPos < 0) {
			// Crop beginning
			copySrcStart = -copyDestPos;
			copyDestPos = 0;
		} else {
			copySrcStart = 0;
		}

		int64_t copyOverflow = (copyDestPos + (copySrcEnd-copySrcStart)) - channelSize;
		if (copyOverflow > 0) {
			// Crop end
			copySrcEnd -= copyOverflow;
		}

		int64_t fillGap = (copyDestPos * sampleSize)-lastWrittenPos;
		for (int i = 0; i < channelCount; ++i) {
			std::copy(frame.data[i]+(copySrcStart * sampleSize), frame.data[i] + (copySrcEnd * sampleSize), channels[i].data + (copyDestPos * sampleSize) );
			if (fillGap > 0) {
				std::fill(channels[i].data+lastWrittenPos, channels[i].data+lastWrittenPos+fillGap, 0);
			}
#if DECODER_SANITY_CHECKS
			else if (fillGap < 0) {
				throw std::runtime_error("Sanity-Panic: Pre-fill gap negative (" + std::to_string(fillGap) + "<0) - may have written into non-authrized area!");
			}
#endif
			delete[] frame.data[i];
		}
		lastWrittenPos = (copyDestPos + (copySrcEnd-copySrcStart)) * sampleSize;
	}

	for (int i = 0; i < channelCount; ++i) {
		int64_t fillGap = channels[i].dataSize-lastWrittenPos;

		if (fillGap > 0) {
			std::fill(channels[i].data+lastWrittenPos, channels[i].data+lastWrittenPos+fillGap, 0);
		}
#if DECODER_SANITY_CHECKS
		else if (fillGap < 0) {
			throw std::runtime_error("Sanity-Panic: Post-fill gap negative (" + std::to_string(fillGap) + "<0) - may have written into non-authrized area!");
		}
#endif
	}

	decodingState->audioFrames.clear();

}


DecodingState* MediaDecoder::createDecoderState(SegmentRequest request) {
	DecodingState* decodingState = new DecodingState();

	decodingState->segmentRequest = request;
	decodingState->requestStart = request.start;
	decodingState->requestEnd = request.end;
	decodingState->videoAvailable = video_stream_index != -1;
	decodingState->audioAvailable = audio_stream_index != -1;
	decodingState->audioDone = audio_stream_index == -1 || request.audioBuffer == NULL;
	decodingState->videoDone = video_stream_index == -1 || request.videoBuffer == NULL;

	if(decodingState->videoAvailable && request.videoBuffer != NULL) {
		auto vidStream = getStreamEntryByIndex(video_stream_index);
		decodingState->frameWidth = vidStream->ctx->width;
		decodingState->frameHeight = vidStream->ctx->height;

		*request.videoBuffer = new torasu::tstd::Dbimg_sequence();
	}
	return decodingState;
}

size_t MediaDecoder::determineSampleSize(StreamEntry* stream) {
	switch(stream->ctx->sample_fmt) {
	case AV_SAMPLE_FMT_NONE:
		return 0;
	case AV_SAMPLE_FMT_U8:
	case AV_SAMPLE_FMT_U8P:
		return 1;
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S16P:
		return 2;
	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_FLT:
	case AV_SAMPLE_FMT_S32P:
	case AV_SAMPLE_FMT_FLTP:
		return 4;
	case AV_SAMPLE_FMT_DBL:
	case AV_SAMPLE_FMT_DBLP:
	case AV_SAMPLE_FMT_S64:
	case AV_SAMPLE_FMT_S64P:
		return 8;
	default:
		//ambiguous default, but leave it for now
		return 4;
	}
}

std::string MediaDecoder::getErrorMessage(int errorCode) {
	char errBuf[AV_ERROR_MAX_STRING_SIZE];
	if (av_strerror(errorCode, errBuf, AV_ERROR_MAX_STRING_SIZE) < 0) {
		return "Unknown Error";
	}
	return errBuf;
}

} // namespace imgc