#include "../include/torasu/mod/imgc/MediaEncoder.hpp"

#include <iostream>
#include <fstream>
#include <cmath>
#include <list>

namespace {

inline std::string avErrStr(int errNum) {
	char error[AV_ERROR_MAX_STRING_SIZE];
	return std::string(av_make_error_string(error, AV_ERROR_MAX_STRING_SIZE, errNum));
}

struct IOOpaque {
	std::ofstream* ofstream;
};

int ReadFunc(void* opaque, uint8_t* buf, int buf_size) {
	std::cerr << "Unsupported ReadOp!" << std::endl;
	return 0;
}

int WriteFunc(void* opaque, uint8_t* buf, int buf_size) {
	// std::cout << "WriteOp..." << std::endl;
	((IOOpaque*) opaque)->ofstream->write(const_cast<const char*>(reinterpret_cast<char*>(buf)), buf_size);
	return 0;
}

int64_t SeekFunc(void* opaque, int64_t offset, int whence) {
	std::cout << "SeekOp O " << offset << " W " << whence << std::endl;

	auto* ioop = reinterpret_cast<IOOpaque*>(opaque);
	std::ofstream* stream = ioop->ofstream;
	switch (whence) {
	case SEEK_SET:
		stream->seekp(offset);
		break;
	case SEEK_CUR:
		std::cerr << "SeekOp CUR unsupported!" << std::endl;
		stream->seekp(stream->tellp()+offset);
		break;
	case SEEK_END:
		std::cerr << "SeekOp END unsupported!" << std::endl;
		// stream->end();
		break;
	case AVSEEK_SIZE:

		std::cerr << "SeekOp END unsupported!" << std::endl;
		// return reader->size;
		break;
	default:
		break;
	}

	// Return the new position:
	return stream->tellp();

}

class StreamContainer {
public:

	enum State {
		CREATED,
		INITIALIZED,
		OPEN,
		FLUSHING,
		FLUSHED,
		CLOSED,
		ERROR
	};

public:
	State state = CREATED;
	AVCodec* codec = nullptr;
	AVCodecContext* ctx = nullptr;
	AVCodecParameters* params = nullptr;
	AVStream* stream = nullptr;
	AVRational time_base = {0, -1};
	int64_t playhead = 0;
	int frame_size = 0;

	
	bool hasQueuedPacket = false;
	AVPacket* queuedPacket = nullptr;

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
	}

	inline void init(AVCodecID codecId, AVFormatContext* formatCtx) {

		if (state != CREATED) {
			state = ERROR;
			throw std::logic_error("StreamContainer seems to have been initialized twice!");
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

		state = OPEN;

	}
};

} // namespace

namespace imgc {

/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame* pict, int frame_index,
						   int width, int height) {
	int x, y, i;

	i = frame_index;

	/* Y */
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

	/* Cb and Cr */
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}
}
/* Prepare a dummy audio. */
static void fill_fltp_audio(AVFrame* frame, int playhead) {
	int samples = frame->nb_samples;
	// std::cout << "SAMPLES: " << samples << std::endl;
	for (int i = 0; i < samples; i++) {
		float sampleNo = playhead+i;
		float freq = sin(sampleNo / 44100 / 1)*1000+1000 + 100;

		// std::cout << "FREQ: " << freq << std::endl;
		float val = sin(sampleNo * freq / 44100)*2;

		// std::cout << " S: " << val << std::endl;
		uint32_t d = *(reinterpret_cast<uint32_t*>(&val));
		for (int c = 0; c < 2; c++) {
			for (int b = 0; b < 4; b++) {
				frame->data[c][i*4+b] = d >> b*8 & 0xFF;
			}
		}

	}
}


torasu::tstd::Dfile* MediaEncoder::encode(EncodeRequest request) {

	//
	// Debugging
	//

	std::ofstream myfile("test.mp4");

	//
	// Settings
	//

	std::string format_name = "mp4";
	int width = 1920;
	int height = 1080;

	//
	// Create Context
	//

	uint8_t* alloc_buf = (uint8_t*) av_malloc(32 * 1024);
	AVFormatContext* formatCtx = avformat_alloc_context();
	if (!formatCtx) {
		throw std::runtime_error("Failed allocating formatCtx!");
	}

	IOOpaque opaque = {
		&myfile
	};

	AVIOContext* avioCtx = avio_alloc_context(alloc_buf, 32 * 1024, true, &opaque, ReadFunc, WriteFunc, SeekFunc);

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

	// oformat.audio_codec = AVCodecID::AV_CODEC_ID_NONE;

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
		vidStream.time_base = {1, 25};
		codecParams->width = width;
		codecParams->height = height;
		codecParams->format = AV_PIX_FMT_YUV420P;
		codecParams->bit_rate = 4000 * 1000;

		vidStream.open();
	}

	// Audio Stream
	// StreamContainer audStream;
	size_t audioFrameSize = 0;
 	{
		auto& audStream = streams.emplace_back();
		audStream.init(oformat.audio_codec, formatCtx);

		AVCodecParameters* codecParams = audStream.params;
		audStream.time_base = {1, 44100};
		audStream.frame_size = 44100/25;
		codecParams->format = AV_SAMPLE_FMT_FLTP;
		codecParams->sample_rate = 44100;
		codecParams->channel_layout = AV_CH_LAYOUT_STEREO;

		audStream.open();

		audioFrameSize = audStream.frame_size;
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

	// Video Dummy

	AVFrame* workingFrame = av_frame_alloc();

	workingFrame->format = AV_PIX_FMT_YUV420P;
	workingFrame->width = width;
	workingFrame->height = height;

	callStat = av_frame_get_buffer(workingFrame, 0);
	if (callStat != 0)
		throw std::runtime_error("Error while creating buffer for frame: " + avErrStr(callStat));

	callStat = av_frame_make_writable(workingFrame);
	if (callStat < 0)
		throw std::runtime_error("Error while making frame writable: " + avErrStr(callStat));

	// Audio Dummy

	AVFrame* audioDummyFrame = av_frame_alloc();

	audioDummyFrame->format = AV_SAMPLE_FMT_FLTP;
	audioDummyFrame->nb_samples = audioFrameSize;
	audioDummyFrame->channel_layout = AV_CH_LAYOUT_STEREO;

	callStat = av_frame_get_buffer(audioDummyFrame, 0);
	if (callStat != 0)
		throw std::runtime_error("Error while creating buffer for audio-frame: " + avErrStr(callStat));

	callStat = av_frame_make_writable(audioDummyFrame);
	if (callStat < 0)
		throw std::runtime_error("Error while making audio-frame writable: " + avErrStr(callStat));

	// Encoding loop

	int framecount = 0;
	for (;;) {
		
		StreamContainer* selectedStream = nullptr;
		{
			int64_t minDts = INT64_MAX;
			for (auto& current : streams) {
				if (current.state == StreamContainer::FLUSHED) {
					continue;
				}
				if (!current.hasQueuedPacket) {
					selectedStream = &current;
					std::cout << " DIRECT INIT QUEUE PACK" << std::endl;
					break;
				}
				if (minDts > current.queuedPacket->dts) {
					selectedStream = &current;
					minDts = current.queuedPacket->dts;
				}
			}
		}
		if (selectedStream == nullptr) {
			std::cout << "Enocding finished." << std::endl;
			break;
		}
		StreamContainer& stream = *selectedStream;
		
		double translatedPh = ((double)stream.playhead)*stream.time_base.num/stream.time_base.den;

		AVFrame* frame = nullptr;
		if (translatedPh < 10) {
			if (stream.ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
				std::cout << " T_VID@" << translatedPh << std::endl;
				workingFrame->pts = stream.playhead;
				fill_yuv_image(workingFrame, stream.playhead, width, height);
				frame = workingFrame;
				stream.playhead++;
			} else if (stream.ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
				std::cout << " T_AUD@" << translatedPh << std::endl;
				audioDummyFrame->pts = stream.playhead;
				fill_fltp_audio(audioDummyFrame, stream.playhead);
				frame = audioDummyFrame;
				stream.playhead += stream.frame_size;
			}
		}

		if (stream.hasQueuedPacket) {
			std::cout << " WRITE PACK " << stream.queuedPacket->dts << " P-ST " << stream.queuedPacket->stream_index << " I-ST " << stream.stream->index << std::endl;
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


	av_frame_free(&workingFrame);

	avformat_free_context(formatCtx);
	avio_context_free(&avioCtx);
	av_free(alloc_buf);


	myfile.close();

	return nullptr;
}

} // namespace imgc
