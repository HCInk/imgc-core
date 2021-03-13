#include "../include/torasu/mod/imgc/MediaEncoder.hpp"

#include <math.h>

#include <iostream>
#include <fstream>
#include <list>
#include <queue>

#define ENCODER_SANITY_CHECKS true

namespace {

typedef torasu::tstd::Dfile::FileBuilder FileBuilder;

inline std::string avErrStr(int errNum) {
	char error[AV_ERROR_MAX_STRING_SIZE];
	return std::string(av_make_error_string(error, AV_ERROR_MAX_STRING_SIZE, errNum));
}

struct IOOpaque {
	// std::ofstream* ofstream;
	FileBuilder fb;
};

int WriteFunc(void* opaque, uint8_t* buf, int buf_size) {
	// std::cout << "WriteOp..." << std::endl;

	// ((IOOpaque*) opaque)->ofstream->write(const_cast<const char*>(reinterpret_cast<char*>(buf)), buf_size);
	((IOOpaque*) opaque)->fb.write(buf, buf_size);
	return 0;
}

int64_t SeekFunc(void* opaque, int64_t offset, int whence) {
	// TODO Log this message w/ logger
	// std::cout << "SeekOp O " << offset << " W " << whence << std::endl;

	auto* ioop = reinterpret_cast<IOOpaque*>(opaque);
	// std::ofstream* stream = ioop->ofstream;
	auto& fb = ioop->fb;
	switch (whence) {
	case SEEK_SET:
		// stream->seekp(offset);
		fb.pos = offset;
		break;
	case SEEK_CUR:
		// stream->seekp(stream->tellp()+offset);
		fb.pos += offset;
		break;
	case SEEK_END:
		std::cerr << "SeekOp END unsupported!" << std::endl;
		return AVERROR_INVALIDDATA;
	case AVSEEK_SIZE:
		std::cerr << "SeekOp AVSEEK_SIZE unsupported!" << std::endl;
		return AVERROR_INVALIDDATA;
	default:
		std::cerr << "Unkown SeekOp!" << std::endl;
		return AVERROR_INVALIDDATA;
		break;
	}

	// Return the (new) position:
	// return stream->tellp();
	return fb.pos;

}

class StreamContainer {
public:
	// Status
	enum State {
		CREATED,     // Has just been created, no initialisations have been done
		INITIALIZED, // Stream has been initialsed, but not yet configured/opened for encoding
		OPEN,        // Stream is now open for encoding
		FLUSHING,    // Stream is currently flushing thier codecs
		FLUSHED,     // Stream has been been flushed and is now done with encoding
		CLOSED,      // Stream has been closed
		ERROR        // An error occurrend and the stream is now in a bad state
	} state = CREATED;

	// Contexts
	AVCodec* codec = nullptr;
	AVCodecContext* ctx = nullptr;
	AVCodecParameters* params = nullptr;
	AVStream* stream = nullptr;

	// Data
	AVRational time_base = {0, -1};
	int64_t playhead = 0;
	int frame_size = 0;

	// Working-Data
	bool hasQueuedPacket = false;
	AVPacket* queuedPacket = nullptr;
	AVFrame* frame = nullptr;
	SwsContext* swsCtx = nullptr;

	// Fetch data
	int64_t fetchPlayhead = 0;
	std::queue<imgc::MediaEncoder::FrameRequest*> fetchQueue;
	imgc::MediaEncoder::FrameRequest* pendingRequest = nullptr;

	// Logging
	torasu::LogInstruction li = torasu::LogInstruction(nullptr);

	StreamContainer() {}

	StreamContainer(const StreamContainer& src) {
		if (src.state != CREATED)
			throw std::logic_error("Copying of an initalized StreamContainer is not supported!");
	}

	StreamContainer& operator=( const StreamContainer& src) {
		if (src.state != CREATED)
			throw std::logic_error("Copying of an initalized StreamContainer is not supported!");
	}


	StreamContainer(AVCodecID codecId, AVFormatContext* formatCtx, torasu::LogInstruction li) {
		init(codecId, formatCtx, li);
	}

	~StreamContainer() {
		// TODO Log this message w/ logger
		// std::cout << " FREE STREAM " << this << std::endl;
		if (stream != nullptr) {
			avcodec_free_context(&stream->codec);
		}
		if (queuedPacket != nullptr) {
			av_packet_free(&queuedPacket);
		}
		if (frame != nullptr) {
			av_frame_free(&frame);
		}
		if (swsCtx != nullptr) {
			sws_freeContext(swsCtx);
		}
	}

	inline void init(AVCodecID codecId, AVFormatContext* formatCtx, torasu::LogInstruction li) {
		this->li = li;
		if (state != CREATED) {
			throw std::logic_error("Can only be initialze in CREATED-state!");
		}

		codec = avcodec_find_encoder(codecId);
		stream = avformat_new_stream(formatCtx, codec);
		if (!stream) {
			state = ERROR;
			throw std::runtime_error("Cannot create stream!");
		}
		ctx = stream->codec;
		params = stream->codecpar;
		avcodec_parameters_from_context(params, ctx);

		queuedPacket = av_packet_alloc();
		if (!queuedPacket) {
			state = ERROR;
			throw std::runtime_error("Cannot create stream-packet!");
		}
		state = INITIALIZED;
	}

	inline void open() {

		if (state != INITIALIZED) {
			throw std::logic_error("Can only be open in INITIALIZED-state!");
		}

		// TODO Log this message w/ logger
		// std::cout << " OPEN STREAM " << this << std::endl;

		stream->time_base = time_base;
		ctx->time_base = time_base;
		params->frame_size = frame_size;

		avcodec_parameters_to_context(ctx, params);

		int openStat = avcodec_open2(ctx, codec, 0);
		if (openStat != 0) {
			state = ERROR;
			throw std::runtime_error("Failed to open codec: " + avErrStr(openStat));
		}

		frame_size = ctx->frame_size;

		initFrame();

		state = OPEN;

	}

	inline void initFrame() {
		frame = av_frame_alloc();

		if (ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
			frame->format = ctx->pix_fmt;
			frame->width = ctx->width;
			frame->height = ctx->height;
		} else if (ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			frame->format = ctx->sample_fmt;
			frame->nb_samples = frame_size;
			frame->sample_rate = ctx->sample_rate;
			frame->channel_layout = ctx->channel_layout;
		} else {
			state = ERROR;
			throw std::runtime_error("Can't configure frame for codec-type: " + std::to_string(ctx->codec_type));
		}

		int callStat;

		callStat = av_frame_get_buffer(frame, 0);
		if (callStat != 0) {
			state = ERROR;
			throw std::runtime_error("Error while creating buffer for frame: " + avErrStr(callStat));
		}

		callStat = av_frame_make_writable(frame);
		if (callStat < 0) {
			state = ERROR;
			throw std::runtime_error("Error while making frame writable: " + avErrStr(callStat));
		}

	}

private:

	/**
	 * @brief  Will generate next frame request for the given stream
	 * @param  offset: The offset of the stream to the source
	 * @param  streamDuration: Duration the stream should have
	 * @retval The created FrameRequest (has to be cleaned up by caller)
	 */
	inline imgc::MediaEncoder::FrameRequest* nextFrameRequest(double offset, double streamDuration) {

		double reqTs = (double) fetchPlayhead*time_base.num/time_base.den + offset;

		switch (ctx->codec_type) {
		case AVMEDIA_TYPE_VIDEO: {
				fetchPlayhead += 1;

				auto* bimgFmt = new torasu::tstd::Dbimg_FORMAT(frame->width, frame->height);
				return new imgc::MediaEncoder::VideoFrameRequest(reqTs, bimgFmt);
			}

		case AVMEDIA_TYPE_AUDIO: {
				int64_t maxDuration = streamDuration*frame->sample_rate - fetchPlayhead;
				int64_t nb_samples;
				if (frame_size > maxDuration) {
					nb_samples = maxDuration;
				} else {
					nb_samples = frame_size;
				}
				fetchPlayhead += nb_samples;

				auto* fmt = new torasu::tstd::Daudio_buffer_FORMAT(frame->sample_rate, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32);
				return new imgc::MediaEncoder::AudioFrameRequest(reqTs, (double) nb_samples/frame->sample_rate, fmt);
			}

		default:
			throw std::runtime_error("Can't write frame for codec-type: " + std::to_string(ctx->codec_type));
		}
	}

	inline int64_t writeFrame(imgc::MediaEncoder::FrameRequest* frameRequest, double maxDuration) {
		std::unique_ptr<imgc::MediaEncoder::FrameRequest> fr(frameRequest);

		if (state != OPEN) {
			throw std::logic_error("Can only write frame in OPEN-state!");
		}

		{
			int finishStat = fr->finish();
			if (finishStat != 0) {
				std::cerr << "Frame-finish callback exited with non-zero return code (" << finishStat << ")!" << std::endl;
			}
		}

		switch (ctx->codec_type) {
		case AVMEDIA_TYPE_VIDEO: {
				int width = frame->width;
				int height = frame->height;
				if (!swsCtx) {
					swsCtx = sws_getContext(width, height, AV_PIX_FMT_RGB0,
											width, height, ctx->pix_fmt,
											SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
					if (!swsCtx)
						throw std::runtime_error("Failed to create sws_scaler_ctx!");
				}

				torasu::tstd::Dbimg* bimg = ((imgc::MediaEncoder::VideoFrameRequest*) frameRequest)->getResult();

				uint8_t* src[4] = {bimg->getImageData(), nullptr, nullptr, nullptr};
				int src_linesize[4] = {width * 4, 0, 0, 0};
				sws_scale(swsCtx, src, src_linesize, 0, frame->height,
						  frame->data, frame->linesize);

				return 1;
			}

		case AVMEDIA_TYPE_AUDIO: {
				bool crop = frame_size > maxDuration*frame->sample_rate;
				if (crop) {
					frame->nb_samples = maxDuration*frame->sample_rate;
				} else {
					frame->nb_samples = frame_size;
				}

				torasu::tstd::Daudio_buffer* audioSeq = ((imgc::MediaEncoder::AudioFrameRequest*) frameRequest)->getResult();

				size_t chCount = audioSeq->getChannelCount();
				auto* channels = audioSeq->getChannels();
				for (size_t ci = 0; ci < chCount; ci++) {
					uint8_t* srcData = channels[ci].data;
					auto dataSize = channels[ci].dataSize;
					uint8_t* destData = frame->data[ci];
#if ENCODER_SANITY_CHECKS
					if (dataSize > (size_t) frame->nb_samples*4) {
						throw std::runtime_error("Sanity-Panic: Recieved frame is bigger then expected!");
					}
#endif
					std::copy(srcData, srcData+dataSize, destData);
					std::fill(destData+dataSize, destData+(frame_size*4), 0x00);
				}

				return frame_size;
			}

		default:
			throw std::runtime_error("Can't write frame for codec-type: " + std::to_string(ctx->codec_type));
		}
	}

public:

	/**
	 * @brief  Updates to next frame, and fills up the buffer
	 * @param  callback: The callback that should be used to retrive the frames
	 * @param  duration: The duration of the stream
	 * @param  offset: The offset of the stream to the source
	 * @retval true, if there is a frame to fetch; false, if the are no frames to fetch left
	 */
	bool updateFrameBuffer(imgc::MediaEncoder::FrameCallbackFunc callback, double duration, double offset) {
		bool logProgress = li.options & torasu::LogInstruction::OPT_PROGRESS;
		bool needsNewFrame = fetchQueue.empty();
		for (;;) {
			if (pendingRequest != nullptr) {
				if (needsNewFrame) {
					// Signalize it can't be postponed
					pendingRequest->needsNow = true;
				}

				int callbackStat = callback(pendingRequest);
				if (callbackStat == 0) {
					// Callback successful - psuh into queue to be fetched
					fetchQueue.push(pendingRequest);
					needsNewFrame = false;

					if (logProgress) {
						int64_t totalFrames = (duration-offset)*time_base.den/time_base.num;
						int64_t pendingFrames = fetchQueue.size();
						int64_t currentFrame = fetchPlayhead - pendingFrames;
						li.logger->log(new torasu::LogProgress( 
							totalFrames, currentFrame, pendingFrames,
							"Rendering frame " + std::to_string(fetchPlayhead) + "/" + std::to_string(totalFrames)));
					}
				} else if (callbackStat == 1) {
					if (needsNewFrame) throw std::runtime_error("Frame callback exited with postpone-code (1), but the flag needsNow is set");
					// Postponed processing - try again later, exit buffering
					break;
				} else {
					// Callback error - drop request
					delete pendingRequest;
					if (callbackStat < 0) {
						std::cerr << "Frame callback exited with negative return code (" << callbackStat << ")!" << std::endl;
					} else {
						std::cerr << "Frame callback exited with invalid return code (" << callbackStat << ")!" << std::endl;
					}
				}

			}

			if ((int64_t)(duration*time_base.den - fetchPlayhead*time_base.num) > 0) {
				// Go to next request
				pendingRequest = nextFrameRequest(offset, duration);
			} else {
				// Reached end
				pendingRequest = nullptr;
				break;
			}
		}

		// End of stream reached if queue is still empty
		if (needsNewFrame) return false;

		auto timeLeft = duration - (playhead*time_base.num/time_base.den);
		auto written = writeFrame(fetchQueue.front(), timeLeft);
		fetchQueue.pop();
		frame->pts = playhead;
		playhead += written;

		if (logProgress) {
			int64_t totalFrames = (duration-offset)*time_base.den/time_base.num;
			int64_t pendingFrames = fetchQueue.size();
			int64_t currentFrame = fetchPlayhead - pendingFrames;
			if (pendingRequest != nullptr) currentFrame -= ctx->codec_type == AVMEDIA_TYPE_AUDIO ? static_cast<imgc::MediaEncoder::AudioFrameRequest*>(pendingRequest)->getDuration()*time_base.den : 1;
			li.logger->log(new torasu::LogProgress(totalFrames, currentFrame, pendingFrames));
		}

		return true;

	}

};

} // namespace

#define CHECK_PARAM_UNSET(unsetCondition, name, message) \
	if (unsetCondition) throw std::logic_error("Parameter '" name "' is not provided in the request. " message);

namespace imgc {

torasu::tstd::Dfile* MediaEncoder::encode(EncodeRequest request, torasu::LogInstruction li) {

	//
	// Configure
	//

	std::string formatName = request.formatName;
	CHECK_PARAM_UNSET(formatName.empty(), "formatName", "It is required for encoding. (Examples are 'mp4', 'mp3' etc..)")

	double begin = request.begin;
	double end = request.end;

	CHECK_PARAM_UNSET(isnan(end), "end", "It is required for encoding.")

	bool doVideo = request.doVideo;
	bool doAudio = request.doAudio;

	if (!doVideo && !doAudio)
		throw std::logic_error("Nor 'doVideo' or 'doAudio' is set, set one or both to configure.");

	int width, height, framerate, videoBitrate;
	if (doVideo) {
		width = request.width;
		height = request.height;
		framerate = request.framerate;
		videoBitrate = request.videoBitrate;
		CHECK_PARAM_UNSET(width == -1, "width", "It is required for video-encoding.")
		CHECK_PARAM_UNSET(height == -1, "height", "It is required for video-encoding.")
		CHECK_PARAM_UNSET(framerate == -1, "framerate", "It is required for video-encoding.")
	}

	int minSampleRate, audioBitrate;
	if (doAudio) {
		minSampleRate = request.minSampleRate;
		audioBitrate = request.audioBitrate;
		CHECK_PARAM_UNSET(minSampleRate == -1, "minSampleRate", "It is required for audio-encoding. (For minimum quality set to 0, for maximum quality use INT32_MAX)")
	}

	int samplerate = minSampleRate; // TODO Get desired sample-rate

	double duration = end-begin;

	//
	// Create Context
	//

	uint8_t* alloc_buf = (uint8_t*) av_malloc(32 * 1024);
	AVFormatContext* formatCtx = avformat_alloc_context();
	if (!formatCtx) {
		throw std::runtime_error("Failed allocating formatCtx!");
	}

	IOOpaque opaque;

	AVIOContext* avioCtx = avio_alloc_context(alloc_buf, 32 * 1024, true, &opaque, nullptr, WriteFunc, SeekFunc);

	if (!avioCtx) {
		av_free(alloc_buf);
		throw std::runtime_error("Failed allocating avio_context!");
	}

	formatCtx->pb = avioCtx;
	formatCtx->flags = AVFMT_FLAG_CUSTOM_IO;

	//
	// Format Setup
	//

	AVOutputFormat oformat;
	for (void* itHelper = nullptr;;) {
		const AVOutputFormat* oformatCurr = av_muxer_iterate(&itHelper);
		if (oformatCurr == nullptr) {
			throw std::runtime_error("Format name " + formatName + " couldn't be found!");
		}
		// std::cout << " FMT-IT " << oformat->name << std::endl;
		if (formatName == oformatCurr->name) {
			oformat = *oformatCurr;
			break;
		}
	}

	if (request.metadata != nullptr) {
		for (auto entry : *request.metadata) {
			if (av_dict_set(&formatCtx->metadata, entry.first.c_str(), entry.second.c_str(), 0) < 0)
				throw std::runtime_error("Failed to set metadata-field \"" + entry.first + "\"!");
		}
	}

	if (doAudio) {
		if (oformat.audio_codec == AV_CODEC_ID_NONE) {
			throw std::runtime_error("No audio-codec available for the format '" + formatName + "'");
		}
	} else {
		oformat.audio_codec = AV_CODEC_ID_NONE;
	}

	if (doVideo) {
		if (oformat.video_codec == AV_CODEC_ID_NONE) {
			throw std::runtime_error("No video-codec available for the format '" + formatName + "'");
		}
	} else {
		oformat.video_codec = AV_CODEC_ID_NONE;
	}

	// TODO Log this message w/ logger
	std::cout << "FMT SELECTED:" << std::endl
			  << "	name: " << oformat.name << std::endl
			  << "	video_codec: " << avcodec_get_name(oformat.video_codec) << std::endl
			  << "	audio_codec: " << avcodec_get_name(oformat.audio_codec) << std::endl;

	formatCtx->oformat = &oformat;

	//
	// Stream Setup
	//

	std::list<StreamContainer> streams;

	bool doProgress = li.options & torasu::LogInstruction::OPT_PROGRESS;
	// Video Stream
	if (doVideo) {
		torasu::LogInstruction videoLi = li;
		if (doProgress) {
			videoLi.options = videoLi.options | torasu::LogInstruction::OPT_PROGRESS;
			doProgress = false;
		} else {
			videoLi.options = videoLi.options &~ torasu::LogInstruction::OPT_PROGRESS;
		}

		auto& vidStream = streams.emplace_back();
		vidStream.init(oformat.video_codec, formatCtx, videoLi);

		AVCodecParameters* codecParams = vidStream.params;
		vidStream.time_base = {1, framerate};
		codecParams->width = width;
		codecParams->height = height;
		codecParams->format = *vidStream.codec->pix_fmts;
		codecParams->bit_rate = videoBitrate;

		vidStream.open();
	}

	// Audio Stream
	if (doAudio) {
		torasu::LogInstruction audioLi = li;
		if (doProgress) {
			audioLi.options = audioLi.options | torasu::LogInstruction::OPT_PROGRESS;
			doProgress = false;
		} else {
			audioLi.options = audioLi.options &~ torasu::LogInstruction::OPT_PROGRESS;
		}
		auto& audStream = streams.emplace_back();
		audStream.init(oformat.audio_codec, formatCtx, audioLi);

		AVCodecParameters* codecParams = audStream.params;
		audStream.time_base = {1, samplerate};
		if (framerate > 0) {
			audStream.frame_size = samplerate/framerate;
		} else {
			audStream.frame_size = samplerate;
		}
		codecParams->format = AV_SAMPLE_FMT_FLTP;
		codecParams->sample_rate = samplerate;
		codecParams->channel_layout = AV_CH_LAYOUT_STEREO;
		codecParams->bit_rate = audioBitrate;

		audStream.open();
	}

	// for (auto& current : streams) {
	// 	std::cout << " STR " << &current << std::endl;
	// }

	// Writing Header

	int callStat;

	callStat = avformat_write_header(formatCtx, 0);
	if (callStat < 0) {
		throw std::runtime_error("Failed to write header (" + std::to_string(callStat) + ")!");
	}


	//
	// Encoding
	//

	// Encoding loop

	for (;;) {

		StreamContainer* selectedStream = nullptr;
		{
			int64_t minDts = INT64_MAX;
			for (auto& current : streams) {
				// Skip streams, which are already done
				if (current.state == StreamContainer::FLUSHED) {
					continue;
				}
				// Directly process streams which dont have a packet yet
				if (!current.hasQueuedPacket) {
					selectedStream = &current;
					// TODO Log this message w/ logger
					// std::cout << " DIRECT INIT QUEUE PACK" << std::endl;
					break;
				}
				// If all streams already have a packet, then pick the stream which the lowest dts in the queue
				if (minDts > current.queuedPacket->dts) {
					selectedStream = &current;
					minDts = current.queuedPacket->dts;
				}
			}
		}

		// No Streams returned: All streams are done.
		if (selectedStream == nullptr) {
			// TODO Log this message w/ logger
			// std::cout << "Enocding finished." << std::endl;
			break;
		}
		StreamContainer& stream = *selectedStream;

		AVFrame* frame;
		if (stream.updateFrameBuffer(frameCallbackFunc, duration, begin)) {
			frame = stream.frame;
		} else {
			frame = nullptr;
		}

		if (stream.hasQueuedPacket) {
			// std::cout << " WRITE PACK " << stream.queuedPacket->dts << " P-ST " << stream.queuedPacket->stream_index << " I-ST " << stream.stream->index << std::endl;
			callStat = av_write_frame(formatCtx, stream.queuedPacket);
			if (callStat != 0)
				throw std::runtime_error("Error while writing frame to format: " + avErrStr(callStat));
			av_packet_unref(stream.queuedPacket);
			stream.hasQueuedPacket = false;
		}

		if (stream.state == StreamContainer::OPEN) {
			if (frame != nullptr) {

				callStat = avcodec_send_frame(stream.ctx, frame);
				if (callStat != 0) {
					if (callStat == AVERROR(EAGAIN)) {
						std::cerr << "codec-send: EGAIN - not implemented yet!" << std::endl;
					} else if (callStat == AVERROR_EOF) {
						throw std::runtime_error("Error while sending frame to codec: Encoder has already been flushed!");
					} else {
						throw std::runtime_error("Error while sending frame to codec: " + avErrStr(callStat));
					}
				} else {
					// Instead increment to the next frame here in case the frame has to re-presented on an EGAIN
				}

			} else {
				stream.state = StreamContainer::FLUSHING;
				callStat = avcodec_send_frame(stream.ctx, nullptr);
				if (callStat != 0)
					throw std::runtime_error("Error while sending flush-frame to codec: " + avErrStr(callStat));
			}

		}

		callStat = avcodec_receive_packet(stream.ctx, stream.queuedPacket);
		if (callStat != 0) {
			if (callStat == AVERROR(EAGAIN)) {
				// TODO Log this message w/ logger
				// std::cout << "codec-recieve: EGAIN" << std::endl;
			} else if (callStat == AVERROR_EOF) {
				// TODO Log this message w/ logger
				// std::cout << "codec-recieve: EOF" << std::endl;
				stream.state = StreamContainer::FLUSHED;
			} else {
				throw std::runtime_error("Error while recieving packet from codec: " + avErrStr(callStat));
			}
		} else {
			av_packet_rescale_ts(stream.queuedPacket, stream.time_base, stream.stream->time_base);
			stream.queuedPacket->stream_index = stream.stream->index;
			stream.hasQueuedPacket = true;
		}

	}

	// Post-actions

	av_write_trailer(formatCtx);

	//
	// Cleanup
	//

	streams.clear();

	avformat_free_context(formatCtx);
	avio_context_free(&avioCtx);
	av_free(alloc_buf);

	return opaque.fb.compile();
}

} // namespace imgc
