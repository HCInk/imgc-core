#include "../include/torasu/mod/imgc/MediaEncoder.hpp"

#include <math.h>

#include <iostream>
#include <fstream>
#include <list>

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
	std::cout << "SeekOp O " << offset << " W " << whence << std::endl;

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

	StreamContainer() {}

	StreamContainer(const StreamContainer& src) {
		if (src.state != CREATED)
			throw std::logic_error("Copying of an initalized StreamContainer is not supported!");
	}

	StreamContainer& operator=( const StreamContainer& src) {
		if (src.state != CREATED)
			throw std::logic_error("Copying of an initalized StreamContainer is not supported!");
	}


	StreamContainer(AVCodecID codecId, AVFormatContext* formatCtx) {
		init(codecId, formatCtx);
	}

	~StreamContainer() {
		std::cout << " FREE STREAM " << this << std::endl;
		if (stream != nullptr) {
			avcodec_free_context(&stream->codec);
		}
		if (queuedPacket != nullptr) {
			av_packet_free(&queuedPacket);
		}
		if (frame != nullptr) {
			av_frame_free(&frame);
		}
	}

	inline void init(AVCodecID codecId, AVFormatContext* formatCtx) {

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

		std::cout << " OPEN STREAM " << this << std::endl;

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

	inline int64_t writeFrame(imgc::MediaEncoder::FrameCallbackFunc callback, double offset, double maxDuration) {
		if (state != OPEN) {
			throw std::logic_error("Can only write frame in OPEN-state!");
		}

		// Timestamp to be requested via callback
		double reqTs = (double) playhead*time_base.num/time_base.den + offset;
		
		switch (ctx->codec_type) {
		case AVMEDIA_TYPE_VIDEO:
		{
			int width = frame->width;
			int height = frame->height;
			if (!swsCtx) {
				swsCtx = sws_getContext(width, height, AV_PIX_FMT_RGB0,
											width, height, ctx->pix_fmt,
											SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
				if (!swsCtx)
					throw std::runtime_error("Failed to create sws_scaler_ctx!");
			}

			torasu::tstd::Dbimg_FORMAT bimgFmt(width, height);
			imgc::MediaEncoder::VideoFrameRequest req(reqTs, &bimgFmt);

			int callbackStat = callback(&req);

			if (callbackStat != 0) {
				std::cerr << "Frame callback exited with non-zero return code (" << callbackStat << ")!" << std::endl;
			} else {
				torasu::tstd::Dbimg* bimg = req.getResult();

				uint8_t* dst[4] = {bimg->getImageData(), nullptr, nullptr, nullptr};
				int dest_linesize[4] = {width * 4, 0, 0, 0};
				sws_scale(swsCtx, dst, dest_linesize, 0, frame->height, 
					frame->data, frame->linesize);
			}

			return 1;
		}

		case AVMEDIA_TYPE_AUDIO:
		{	
			bool crop = frame_size > maxDuration*frame->sample_rate;
			if (crop) {
				frame->nb_samples = maxDuration*frame->sample_rate;
			} else {
				frame->nb_samples = frame_size;
			}
			torasu::tstd::Daudio_buffer_FORMAT fmt(frame->sample_rate, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32);
			imgc::MediaEncoder::AudioFrameRequest req(reqTs, (double) frame->nb_samples/frame->sample_rate, &fmt);
			int callbackStat = callback(&req);

			if (callbackStat != 0) {
				std::cerr << "Frame callback exited with non-zero return code (" << callbackStat << ")!" << std::endl;
			} else {
				torasu::tstd::Daudio_buffer* audioSeq = req.getResult();
				
				size_t chCount = audioSeq->getChannelCount();
				auto* channels = audioSeq->getChannels();
				for (size_t ci = 0; ci < chCount; ci++) {
					uint8_t* data = channels[ci].data;
					std::copy(data, data+channels[ci].dataSize, frame->data[ci]);
					if (crop) {
						std::fill(data+frame->nb_samples, data+frame_size, 0x00);
					}
				}
			}

			return frame_size;
		}

		default:
			throw std::runtime_error("Can't write frame for codec-type: " + std::to_string(ctx->codec_type));
		}
	}

};

} // namespace

namespace imgc {

torasu::tstd::Dfile* MediaEncoder::encode(EncodeRequest request) {
	/*
	FileBuilder fb;
	{
		uint8_t a[] = {1, 30, 10};
		fb.write(a, 3);
	}
	fb.printMap();

	{
		fb.pos = 1;
		uint8_t a[] = {2, 6, 40};
		fb.write(a, 3);
	}
	fb.printMap();

	{
		uint8_t a[] = {2, 6, 40};
		fb.write(a, 3);
	}
	fb.printMap();

	{
		auto* file = fb.compile();
		uint8_t* data = file->getFileData();
		size_t size = file->getFileSize();


		for (size_t i = 0; i < size; i++) {
			std::cout << " - " << (int)data[i] << std::endl;
		}
	}*/


	//
	// Settings
	//

	std::string format_name = "mp4";
	int width = 1920;
	int height = 1080;
	int framerate = 25;
	int samplerate = 44100;
	double begin = 0;
	double end = 10;

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
			throw std::runtime_error("Format name " + format_name + " couldn't be found!");
		}
		// std::cout << " FMT-IT " << oformat->name << std::endl;
		if (format_name == oformatCurr->name) {
			oformat = *oformatCurr;
			break;
		}
	}

	std::cout << "FMT SELECTED:" << std::endl
			  << "	name: " << oformat.name << std::endl
			  << "	video_codec: " << avcodec_get_name(oformat.video_codec) << std::endl
			  << "	audio_codec: " << avcodec_get_name(oformat.audio_codec) << std::endl;


	formatCtx->oformat = &oformat;

	//
	// Stream Setup
	//

	std::list<StreamContainer> streams;

	// Video Stream
	{
		auto& vidStream = streams.emplace_back();
		vidStream.init(oformat.video_codec, formatCtx);

		AVCodecParameters* codecParams = vidStream.params;
		vidStream.time_base = {1, framerate};
		codecParams->width = width;
		codecParams->height = height;
		codecParams->format = AV_PIX_FMT_YUV420P;
		codecParams->bit_rate = 4000 * 1000;

		vidStream.open();
	}

	// Audio Stream
	{
		auto& audStream = streams.emplace_back();
		audStream.init(oformat.audio_codec, formatCtx);

		AVCodecParameters* codecParams = audStream.params;
		audStream.time_base = {1, samplerate};
		audStream.frame_size = samplerate/framerate;
		codecParams->format = AV_SAMPLE_FMT_FLTP;
		codecParams->sample_rate = samplerate;
		codecParams->channel_layout = AV_CH_LAYOUT_STEREO;

		audStream.open();
	}

	for (auto& current : streams) {
		std::cout << " STR " << &current << std::endl;
	}

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
					std::cout << " DIRECT INIT QUEUE PACK" << std::endl;
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
			std::cout << "Enocding finished." << std::endl;
			break;
		}
		StreamContainer& stream = *selectedStream;

		double translatedPh = ((double)stream.playhead)*stream.time_base.num/stream.time_base.den;

		AVFrame* frame = nullptr;
		double duationLeft = duration - translatedPh;
		if (duationLeft > 0) {
			auto written = stream.writeFrame(frameCallbackFunc, begin, duationLeft);
			frame = stream.frame;
			frame->pts = stream.playhead;
			stream.playhead += written;
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
				std::cout << "codec-recieve: EGAIN" << std::endl;
			} else if (callStat == AVERROR_EOF) {
				std::cout << "codec-recieve: EOF" << std::endl;
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

	avformat_free_context(formatCtx);
	avio_context_free(&avioCtx);
	av_free(alloc_buf);

	return opaque.fb.compile();
}

} // namespace imgc