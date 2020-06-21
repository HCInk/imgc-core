//
// Created by Yann Holme Nielsen on 14/06/2020.
//

#include <cstdio>
#include <cstdint>
#include <string>
#include <fstream>
#include <iostream>
#include <math.h>
#include "torasu/mod/imgc/VideoFileDeserializer.hpp"
#include <lodepng.h>

using namespace std;
namespace {

int ReadFunc(void* ptr, uint8_t* buf, int buf_size) {
	VideoFileDeserializer::FileReader* reader = reinterpret_cast<VideoFileDeserializer::FileReader*>(ptr);

	size_t nextPos = reader->pos + buf_size;
	size_t read;

	if (nextPos <= reader->size) {
		read = buf_size;
	} else {
		nextPos = reader->size;
		read = nextPos - reader->pos;
	}

	memcpy(buf, reader->data + reader->pos, read);
	//cout << "R " << read << " OF " << buf_size << endl;
	reader->pos += read;
	if (read > 0) {
		return read;
	} else {
		return -1;
	}
}

int64_t SeekFunc(void* ptr, int64_t pos, int whence) {
	VideoFileDeserializer::FileReader* reader = reinterpret_cast<VideoFileDeserializer::FileReader*>(ptr);

	//cout << "SEEK " << pos << " WHENCE " << whence << endl;
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
		//cout << "SEEK SIZE" << endl;
		return reader->size;
	default:
		//cout << "UNSUPP-SEEK " << pos << " WHENCE " << whence << endl;
		break;
	}

	// Return the new position:
	return reader->pos;
}
}

VideoFileDeserializer::VideoFileDeserializer(std::string path) {
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

void VideoFileDeserializer::prepare() {
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
	av_format_ctx->probesize = 1200000;
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

VideoFileDeserializer::~VideoFileDeserializer() {
	if (sws_scaler_ctx != nullptr) {
		sws_freeContext(sws_scaler_ctx);
	}
	for (int i = 0; i < streams.size(); ++i) {
		auto stream = streams[i];
		if (stream->frame != nullptr)
			av_frame_free(&stream->frame);
		if (stream->ctx != nullptr)
			avcodec_free_context(&stream->ctx);


	}
	av_packet_free(&av_packet);
	avformat_close_input(&av_format_ctx);
}

void VideoFileDeserializer::flushBuffers(StreamEntry* entry) {
	avcodec_flush_buffers(entry->ctx);
	entry->cachedFrames.clear();
	entry->flushCount = 0;
}

StreamEntry* VideoFileDeserializer::getStreamEntryByIndex(int index) {
	for (int i = 0; i < streams.size(); ++i) {
		auto entry = streams[i];
		if (entry->index == index) return entry;
	}
	return nullptr;
}

/*int *VideoFileDeserializer::getRealBounds(StreamEntry *stream) { // Still in use?
    AVFrame *video_frame = stream->frame;
    int32_t video_width = stream->ctx->width;
    int32_t video_height = stream->ctx->height;

    int32_t rWidth, rHeight;
    if (video_width < 0) {
        if (video_height < 0) {
            rWidth = video_frame->width;
            rHeight = video_frame->height;
        } else {
            rWidth = video_height * (static_cast<double>(video_frame->width) / video_frame->height);
            rHeight = video_height;
        }
    } else {
        if (video_height < 0) {
            rWidth = video_width;
            rHeight = video_width * (static_cast<double>(video_frame->height) / video_frame->width);
        } else {
            rWidth = video_width;
            rHeight = video_height;
        }
    }
    int32_t vals[] = {rWidth, rHeight};
    return vals;
}*/

void VideoFileDeserializer::removeCacheFrame(int64_t pos, std::vector<BufferedFrame>* list) {
	for (auto it = list->begin(); it != list->end(); ++it) {
		BufferedFrame entry = *it;
		if (entry.pos == pos) {
			list->erase(it);
			return;
		}

	}
}

void VideoFileDeserializer::extractVideoFrame(StreamEntry* stream, uint8_t* outPt) {
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

int64_t VideoFileDeserializer::toBaseTime(double value, AVRational base) {
	return round((value * base.den) / base.num);
}

DecodingState* VideoFileDeserializer::getSegment(SegmentRequest request) {

	DecodingState* decodingState = new DecodingState();

	decodingState->requestStart = request.start;
	decodingState->requestEnd = request.end;

	if (request.videoBuffer == NULL) {
		decodingState->videoDone = true; // Set this to true since not required
	} else {
		auto vidStream = getStreamEntryByIndex(video_stream_index);
		decodingState->frameWidth = vidStream->ctx->width;
		decodingState->frameHeight = vidStream->ctx->height;
	}

	if (request.audioBuffer == NULL) {
		decodingState->audioDone = true; // Set this to true since not required
	}

	fetchBuffered(decodingState);

	initializePosition(decodingState);

	while (true) {
		if (decodingState->videoDone && decodingState->audioDone) break;
		av_packet_unref(av_packet);
		int nextFrameStat = av_read_frame(av_format_ctx, av_packet);
		auto stream = getStreamEntryByIndex(av_packet->stream_index);
		if (nextFrameStat == AVERROR_EOF) {
			for(int i = 0; i < streams.size(); i++) {
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

	if (!decodingState->audFrames.empty()) {
		concatAudio(decodingState);
	} else if (request.audioBuffer != NULL) { 
		// TODO Better way to handle if the requested stream is not available 
		// Will crash if audio stream is unexistent, but audio is requested
		std::vector<uint8_t*> data;

		int channelCount = getStreamEntryByIndex(audio_stream_index)->ctx->channels;

		for (int i = 0; i < channelCount; i++) {
			data.push_back(new uint8_t[0]);
		}

		decodingState->audFrames.push_back( (AudioFrame) {
			.start = decodingState->requestStart,
			.end = decodingState->requestEnd,
			.numSamples = 0,
			.size = 0,
			.data = data 
		} );
	}

	cout << "Decoding of all components done!\n";
	return decodingState;
}

bool VideoFileDeserializer::checkFrameTargetBound(AVFrame* frame, int64_t start, int64_t end) {

	if (frame->pts < end) {
		if (frame->pts >= start) {
			return true;
		} else {
			return frame->pts + frame->pkt_duration > start;
		}
	} else {
		return false;
	}

}

void VideoFileDeserializer::handleFrame(StreamEntry* stream, DecodingState* decodingState) {

	stream->nextFramePts = stream->frame->pts + stream->frame->pkt_duration;

	int64_t targetPosition = toBaseTime(decodingState->requestStart, stream->base_time);
	int64_t targetPositionEnd = toBaseTime(decodingState->requestEnd, stream->base_time);

	if(targetPositionEnd > stream->duration) {
		// FAIL safe, forced the wanted end pos to the start of last available frame of the stream
		targetPositionEnd = stream->duration - stream->frame->pkt_duration;
	}

	if (!decodingState->videoDone && stream->index == video_stream_index) {
		if (checkFrameTargetBound(stream->frame, targetPosition, targetPositionEnd)) {
			if (!decodingState->vidFrames.empty() && decodingState->vidFrames.rbegin()->end > stream->frame->pts) {
				cout << "[VIDEO-FRAME] OLD FRAME " << stream->frame->pts << endl;
				return;
			}
			cout << "[VIDEO-FRAME] NEW FRAME " << stream->frame->pts << endl;
			int32_t rWidth = stream->frame->width;
			int32_t rHeight = stream->frame->height;
			uint32_t frameSize = rWidth * rHeight * 4;

			auto* target = new uint8_t[frameSize];
			extractVideoFrame(stream, target);

			auto frame = VideoFrame{stream->frame->pts, stream->frame->pts + stream->frame->pkt_duration, frameSize,
									target};
			decodingState->vidFrames.push_back(frame);
		}

		if (targetPositionEnd <= stream->nextFramePts) {
			decodingState->videoDone = true;
		}

	}

	if (!decodingState->audioDone && stream->index == audio_stream_index) {
		if (checkFrameTargetBound(stream->frame, targetPosition, targetPositionEnd)) {
			//   auto frameOffset = stream->frame->pts - targetPosition;
			//   auto frameDuration = stream->frame->pkt_duration;
			if (!decodingState->audFrames.empty() && decodingState->audFrames.rbegin()->end > stream->frame->pts) {
				cout << "[AUDIO-FRAME] OLD FRAME " << stream->frame->pts << endl;
				return;
			}
			cout << "[AUDIO-FRAME] NEW FRAME " << stream->frame->pts << endl;
			// std::cout << "[AUDIO-FRAME] OFFSET " << frameOffset << " DURATION " << frameDuration << std::endl;
			std::vector<uint8_t*> data;
			size_t memSize = stream->frame->nb_samples * 4;
			for (int i = 0; i < stream->frame->channels; ++i) {
				auto* cp = new uint8_t[memSize];
				std::copy(stream->frame->extended_data[i], stream->frame->extended_data[i]+memSize, cp);
				data.push_back(cp);
			}
			auto part = AudioFrame{stream->frame->pts, stream->frame->pts + stream->frame->pkt_duration,
								   stream->frame->nb_samples, memSize, data};
			decodingState->audFrames.push_back(part);
		}

		if (targetPositionEnd <= stream->nextFramePts) {
			decodingState->audioDone = true;
		}

	}
}

void VideoFileDeserializer::fetchBuffered(DecodingState* decodingState) {

	if (!decodingState->videoDone) {
		auto vidStream = getStreamEntryByIndex(video_stream_index);
		int64_t vidTargetPosition = toBaseTime(decodingState->requestStart, vidStream->base_time);
		//	int64_t vidTargetPositionEnd = toBaseTime(decodingState->requestEnd, vidStream->base_time);

		if (!vidStream->frameIsPresent) return;

		auto vidFrameStart = vidStream->frame->pts;
		//	auto vidFrameEnd = vidStream->frame->pts + vidStream->frame->pkt_duration;

		if (vidFrameStart <= vidTargetPosition) {
			handleFrame(vidStream, decodingState);
		}
	}

	if (!decodingState->audioDone) {

		auto audStream = getStreamEntryByIndex(audio_stream_index);
		int64_t audTargetPosition = toBaseTime(decodingState->requestStart, audStream->base_time);
		//	int64_t audTargetPositionEnd = toBaseTime(decodingState->requestEnd, audStream->base_time);

		if (!audStream->frameIsPresent) return;

		auto audFrameStart = audStream->frame->pts;
		//auto audFrameEnd = audStream->frame->pts + audStream->frame->pkt_duration;

		if (audFrameStart <= audTargetPosition) {
			handleFrame(audStream, decodingState);
		}

	}

}


void VideoFileDeserializer::initializePosition(DecodingState* decodingState) {

	bool vidSeekBack;
	bool vidSeekForward;
	if (!decodingState->videoDone) {
		auto vidStream = getStreamEntryByIndex(video_stream_index);
		int64_t vidTargetPosition = decodingState->vidFrames.empty() ?
									toBaseTime(decodingState->requestStart, vidStream->base_time) : decodingState->vidFrames.rbegin()->end;

		int64_t vidPlayhead = vidStream->nextFramePts >= 0 ?
							vidStream->nextFramePts : INT64_MAX;

		vidSeekBack = vidPlayhead > vidTargetPosition;


		vidSeekForward = !vidSeekBack && vidTargetPosition > 0;

		if (vidSeekForward) {

			int64_t cacheEnd = 0;
			for (auto& cachedFrame : vidStream->cachedFrames) {
				int64_t currentFrameEnd = cachedFrame.startTime + cachedFrame.duration;
				if (cacheEnd < currentFrameEnd) {
					cacheEnd = currentFrameEnd;
				}
			}

			if (cacheEnd > vidTargetPosition) {
				// Dont seek forward, since frames are already located in the buffer
				vidSeekForward = false;
			}

		}

	} else {
		vidSeekBack = false;
		vidSeekForward = false;
	}

	bool audSeekBack;
	if (!decodingState->audioDone) {
	
		auto audStream = getStreamEntryByIndex(audio_stream_index);

		int64_t audTargetPosition = decodingState->audFrames.empty() ?
									toBaseTime(decodingState->requestStart, audStream->base_time) : decodingState->audFrames.rbegin()->end;

		int64_t audPlayhead = audStream->nextFramePts >= 0 ?
							audStream->nextFramePts : INT64_MAX;

		audSeekBack = (audPlayhead > audTargetPosition);

	} else {
		audSeekBack = false;
	}

	cout << "SEEK DECISION VB " << vidSeekBack << " AB " << audSeekBack << " VF " << vidSeekForward << endl;

	// TODO Seek inidividual streams if required

	if (vidSeekBack || audSeekBack || vidSeekForward) {
		av_seek_frame(av_format_ctx, -1, decodingState->requestStart * AV_TIME_BASE,
					  AVSEEK_FLAG_BACKWARD); //Assuming the -1 includes all streams
		for (auto& stream : streams) {
			stream->flushCount = stream->cachedFrames.size();
			stream->nextFramePts = -1;
		}
	}

}


void VideoFileDeserializer::drainStream(StreamEntry* stream, DecodingState* decodingState) {
	int drainingRequest = avcodec_send_packet(stream->ctx, NULL);
	if(drainingRequest != 0) return;
	stream->draining = true;
	while(true) {
		int response = avcodec_receive_frame(stream->ctx, stream->frame);
		if (response == AVERROR(EAGAIN))
			continue;

		if (response == AVERROR_EOF)
			break;

		if (response != 0)
			throw runtime_error("non 0 response code from receive frame while draining");

		removeCacheFrame(stream->frame->pkt_pos, &stream->cachedFrames);
		handleFrame(stream, decodingState);
	}
	flushBuffers(stream);
	stream->draining = false;
}

void VideoFileDeserializer::concatAudio(DecodingState* decodingState) {
	auto audioStream = getStreamEntryByIndex(audio_stream_index);

	auto audioCtx = audioStream->ctx;
	auto audioBaseTime = audioStream->base_time;
	int channelCount = audioCtx->channels;
	int sampleRate = audioCtx->sample_rate;

	std::vector<uint8_t*> resultData(channelCount);

	size_t sampleSize = 4;

	// Channel-size in samples
	int channelSize = sampleRate * (decodingState->requestEnd - decodingState->requestStart);
	cout << "CP-ALLOC " << channelSize << endl;
	for (int i = 0; i < channelCount; ++i) {
		resultData[i] = new uint8_t[channelSize*sampleSize];
	}

	// Requested start/end in base-time
	int64_t requestStartBased = toBaseTime(decodingState->requestStart, audioBaseTime);
	int64_t requestEndBased = toBaseTime(decodingState->requestEnd, audioBaseTime);

	for (AudioFrame& frame : decodingState->audFrames) {

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

		cout << "CP " << copyDestPos << "-" << (copyDestPos+(copySrcEnd-copySrcStart)) << endl;

		for (int i = 0; i < channelCount; ++i) {
			std::copy(frame.data[i]+(copySrcStart * sampleSize), frame.data[i] + (copySrcEnd * sampleSize), resultData[i] + (copyDestPos * sampleSize) );
			// TODO free audio data of frames
		}

	}
	decodingState->audFrames.clear();
	decodingState->audFrames.push_back((AudioFrame) {
		requestStartBased, requestEndBased, channelSize, (int) (channelSize*sampleSize), resultData
	});

}