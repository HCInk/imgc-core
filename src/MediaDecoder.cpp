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
#include <lodepng.h>

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
	memcpy(buf, reader->data + reader->pos, read);
	reader->pos += read;
	if (read > 0) {
		return read;
	} else {
		return -1;
	}
}

int64_t SeekFunc(void* ptr, int64_t pos, int whence) {
	imgc::MediaDecoder::FileReader* reader = reinterpret_cast<imgc::MediaDecoder::FileReader*>(ptr);
	switch (whence) {
	case SEEK_SET:
		reader->pos = pos;
		break;
	case SEEK_CUR:
		reader->pos += pos;
		break;
	case SEEK_END:
		reader->pos = reader->size + pos;
		break;
	case AVSEEK_SIZE:
		return reader->size;
	default:
		break;
	}

	// Return the new position:
	return reader->pos;
}

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
	prepare();
}

void MediaDecoder::prepare() {
	uint8_t* alloc_buf = (uint8_t*) av_malloc(32 * 1024);
	av_format_ctx = avformat_alloc_context();
	if (!av_format_ctx) {
		throw runtime_error("Failed allocating av_format_ctx!");
	}
	av_format_ctx->pb = avio_alloc_context(alloc_buf, 32 * 1024, false, &in_stream, ReadFunc, NULL, SeekFunc);

	if (!av_format_ctx->pb) {
		av_free(alloc_buf);
		throw runtime_error("Failed allocating avio_context!");
	}
	av_format_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
	if (avformat_open_input(&av_format_ctx, "", NULL, NULL) != 0) {
		throw runtime_error("Failed to open input audio_out_stream");
	}

	// Get infromation about streams
	if (avformat_find_stream_info(av_format_ctx, NULL) < 0) {
		throw runtime_error("Failed to find audio_out_stream info");
	}
	av_packet = av_packet_alloc();

	for (unsigned int stream_index = 0; stream_index < av_format_ctx->nb_streams; stream_index++) {
		AVStream* stream = av_format_ctx->streams[stream_index];
		StreamEntry* entry = new StreamEntry();
		entry->index = stream->index;
		entry->codecType = stream->codecpar->codec_type;
		entry->codec = avcodec_find_decoder(stream->codecpar->codec_id);
		entry->ctx = avcodec_alloc_context3(entry->codec);
		entry->ctx_params = stream->codecpar;
		entry->base_time = stream->time_base;
		entry->duration = stream->duration;

		if (entry->codec) {
			if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
				video_stream_index = entry->index;
				entry->vid_delay = stream->codecpar->video_delay;
				entry->vid_fps = stream->r_frame_rate;
			} else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
				audio_stream_index = entry->index;
			}

		} else {
			cerr << "CANT FIND CODEC FOR STREAM " << stream->id << " (TYPE " << stream->codecpar->codec_type << ")" << endl;
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
	delete[] in_stream.data;
	av_packet_free(&av_packet);
	avformat_close_input(&av_format_ctx);
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
}

void MediaDecoder::extractVideoFrame(StreamEntry* stream, uint8_t* outPt) {
	int32_t rWidth = stream->frame->width;
	int32_t rHeight = stream->frame->height;
	if (sws_scaler_ctx == NULL) {
		sws_scaler_ctx = sws_getContext(stream->frame->width, stream->frame->height, stream->ctx->pix_fmt,
										rWidth, rHeight, AV_PIX_FMT_RGB0,
										SWS_FAST_BILINEAR, NULL, NULL, NULL);
	}

	if (!sws_scaler_ctx) {
		throw runtime_error("Failed to create sws_scaler_ctx!");
	}
	uint8_t* dst[4] = {outPt, NULL, NULL, NULL};
	int dest_linesize[4] = {rWidth * 4, 0, 0, 0};
	sws_scale(sws_scaler_ctx, stream->frame->data, stream->frame->linesize, 0, stream->frame->height, dst,
			  dest_linesize);


}

int64_t MediaDecoder::toBaseTime(double value, AVRational base) {
	return round((value * base.den) / base.num);
}

void MediaDecoder::getSegment(SegmentRequest request) {

	DecodingState* decodingState = createDecoderState(request);


	fetchBuffered(decodingState);
	initializePosition(decodingState);

	while (true) {
		if ((decodingState->videoDone||!decodingState->videoAvailable) && (decodingState->audioDone || !decodingState->audioAvailable)) {
			break;
		}
		av_packet_unref(av_packet);
		int nextFrameStat = av_read_frame(av_format_ctx, av_packet);
		auto stream = getStreamEntryByIndex(av_packet->stream_index);
		if (nextFrameStat == AVERROR_EOF) {
			for(unsigned long i = 0; i < streams.size(); i++) {
				drainStream(streams[i], decodingState);
			}
			break;
		}
		int response = avcodec_send_packet(stream->ctx, av_packet);
		if (response == AVERROR(EAGAIN)) {
			continue;
		}
		if(response == AVERROR(ENOMEM)) {
			throw runtime_error("Send packet returned ENOMEM");
		}
		auto fr = BufferedFrame{av_packet->pts, av_packet->pos, av_packet->duration};
		stream->cachedFrames.push_back(fr);
		response = avcodec_receive_frame(stream->ctx, stream->frame);
		if (response == AVERROR(EAGAIN)) {
			if (stream->flushCount > 0) {
				stream->flushCount--;
			}
			continue;
		}
		stream->frameIsPresent = true;
		removeCacheFrame(stream->frame->pkt_pos, &stream->cachedFrames);

		if (stream->flushCount > 0) {
			stream->flushCount--;
			continue;
		}
		handleFrame(stream, decodingState);
	}

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
	return ((frame->pts < end) && (frame->pts >= start || frame->pts + frame->pkt_duration > start));
}

void MediaDecoder::handleFrame(StreamEntry* stream, DecodingState* decodingState) {

	stream->nextFramePts = stream->frame->pts + stream->frame->pkt_duration;

	int64_t targetPosition = toBaseTime(decodingState->requestStart, stream->base_time);
	int64_t targetPositionEnd = toBaseTime(decodingState->requestEnd, stream->base_time);

	if(targetPositionEnd > stream->duration) {
		// FAIL safe, forced the wanted end pos to the start of last available frame of the stream
		targetPositionEnd = stream->duration - stream->frame->pkt_duration;
	}

	if (!decodingState->videoDone && stream->index == video_stream_index) {
		if (checkFrameTargetBound(stream->frame, targetPosition, targetPositionEnd)) {
			if (decodingState->videoReadUntil > stream->frame->pts) {
				return;
			}
			int32_t rWidth = stream->frame->width;
			int32_t rHeight = stream->frame->height;

			uint8_t* target = (**decodingState->segmentRequest.videoBuffer).addFrame(
								  ((double)(stream->frame->pts * stream->base_time.num)) / stream->base_time.den,
								  torasu::tstd::Dbimg_FORMAT(rWidth, rHeight))->getImageData();
			extractVideoFrame(stream, target);

			decodingState->videoReadUntil = stream->frame->pts + stream->frame->pkt_duration;
		}

		if (targetPositionEnd <= stream->nextFramePts) {
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
		av_seek_frame(av_format_ctx, -1, decodingState->requestStart * AV_TIME_BASE,
					  AVSEEK_FLAG_BACKWARD); //Assuming the -1 includes all streams
		for (auto& stream : streams) {
			stream->flushCount = stream->cachedFrames.size();
			stream->nextFramePts = -1;
		}
	}

}


void MediaDecoder::drainStream(StreamEntry* stream, DecodingState* decodingState) {
	int drainingRequest = avcodec_send_packet(stream->ctx, NULL);
	if(drainingRequest != 0) return;
	stream->draining = true;
	while(true) {
		int response = avcodec_receive_frame(stream->ctx, stream->frame);
		if (response == AVERROR(EAGAIN)) {
			continue;
		}

		if (response == AVERROR_EOF) {
			break;
		}

		if (response != 0) {
			throw runtime_error("non 0 response code from receive frame while draining");
		}

		removeCacheFrame(stream->frame->pkt_pos, &stream->cachedFrames);
		handleFrame(stream, decodingState);
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
	int channelSize = sampleRate * (decodingState->requestEnd - decodingState->requestStart);
	*decodingState->segmentRequest.audioBuffer = new torasu::tstd::Daudio_buffer(channelCount, sampleRate, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32, sampleSize, channelSize * sampleSize);

	auto* channels = (*decodingState->segmentRequest.audioBuffer)->getChannels();

	// Requested start in base-time
	int64_t requestStartBased = toBaseTime(decodingState->requestStart, audioBaseTime);

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

		for (int i = 0; i < channelCount; ++i) {
			std::copy(frame.data[i]+(copySrcStart * sampleSize), frame.data[i] + (copySrcEnd * sampleSize), channels[i].data + (copyDestPos * sampleSize) );
			delete[] frame.data[i];
		}

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

} // namespace imgc