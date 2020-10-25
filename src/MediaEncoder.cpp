#include "../include/torasu/mod/imgc/MediaEncoder.hpp"

#include <iostream>
#include <fstream>

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

	int64_t SeekFunc(void *opaque, int64_t offset, int whence) {
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

} // namespace

namespace imgc {

/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame *pict, int frame_index,
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


torasu::tstd::Dfile* MediaEncoder::encode(EncodeRequest request) {

	//
	// Debugging
	//

	std::ofstream myfile("test.mp4");

	// 
	// Settings
	//

	std::string format_name = "mp4";
	int width = 160;
	int height = 120;

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

	AVCodec* videoCodec = avcodec_find_encoder(oformat.video_codec);

	AVStream* videoStream = avformat_new_stream(formatCtx, videoCodec);
	if (!videoStream) {
		throw std::runtime_error("Cannot create video stream!");
	}

	/*AVStream* videoStream = avformat_new_stream(formatCtx, nullptr);
	if (!videoStream) {
		throw std::runtime_error("Cannot create video stream!");
	}

	videoStream->codec = avcodec_alloc_context3(videoCodec);
	if (!videoStream->codec) {
		throw std::runtime_error("Cannot allocate codec!");
	}*/
	

	AVCodecParameters* videoCodecParams = videoStream->codecpar;
    // avcodec_parameters_from_context(videoStream->codecpar, videoStream->codec);
	videoCodecParams->codec_id = oformat.video_codec;
    videoCodecParams->codec_type = AVMEDIA_TYPE_VIDEO;
    videoCodecParams->width = width;
    videoCodecParams->height = height;
    videoCodecParams->format = AV_PIX_FMT_YUV420P;
    videoCodecParams->bit_rate = 400 * 1000;
    videoStream->time_base = {1, 25};
	videoStream->codec->time_base = videoStream->time_base;

	avcodec_parameters_to_context(videoStream->codec, videoStream->codecpar);

	int callStat;

	callStat = avcodec_open2(videoStream->codec, videoCodec, 0);
	if (callStat != 0)
		throw std::runtime_error("Failed to open codec: " + avErrStr(callStat));

	callStat = avformat_write_header(formatCtx, nullptr);
	if (callStat < 0) {
		throw std::runtime_error("Failed to write header (" + std::to_string(callStat) + ")!");
	}


	//
	// Encoding
	//

	// Preparations

	AVPacket* avPacket = av_packet_alloc();
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

	// Encoding loop

	int framecount = 0;
	bool flushing = false;
	for (;;) {

		if (!flushing) {
			workingFrame->pts = framecount;
			fill_yuv_image(workingFrame, framecount, width, height);
			callStat = avcodec_send_frame(videoStream->codec, workingFrame);
			if (callStat != 0) {
				if (callStat == AVERROR(EAGAIN)) {
					std::cerr << "codec-send: EGAIN - not implemented yet!" << std::endl;
				} else if (callStat == AVERROR_EOF) {
					throw std::runtime_error("Error while sending frame to codec: Encoder has already been flushed!");
				} else {
					throw std::runtime_error("Error while sending frame to codec: " + avErrStr(callStat));
				}
			} else {
				framecount++;
			}
		}

		callStat = avcodec_receive_packet(videoStream->codec, avPacket);
		if (callStat != 0) {
			if (callStat == AVERROR(EAGAIN)) {
				std::cout << "codec-recieve: EGAIN" << std::endl;
			} else if (callStat == AVERROR_EOF) {
				std::cout << "codec-recieve: EOF" << std::endl;
				break;
			} else {
				throw std::runtime_error("Error while recieving packet from codec: " + avErrStr(callStat));
			}
		} else {
			av_packet_rescale_ts(avPacket, videoStream->codec->time_base, videoStream->time_base);
			callStat = av_write_frame(formatCtx, avPacket);
			av_packet_unref(avPacket);
			if (callStat != 0) 
				throw std::runtime_error("Error while writing frame to format: " + avErrStr(callStat));
		}

		if (framecount >= 10000 && !flushing) {

			flushing = true;
			callStat = avcodec_send_frame(videoStream->codec, nullptr);
			if (callStat != 0)
				throw std::runtime_error("Error while sending flush-frame to codec: " + avErrStr(callStat));
			
		} 

	}

	// Post-actions

	av_write_trailer(formatCtx);

/* 	// Flushing

	callStat = avcodec_send_frame(videoStream->codec, nullptr);
	if (callStat != 0)
		throw std::runtime_error("Error while sending flush to codec: " + avErrStr(callStat));

	callStat = avcodec_receive_packet(videoStream->codec, avPacket);
	if (callStat != 0) 
		throw std::runtime_error("Error while recieving packet from codec: " + avErrStr(callStat));

	callStat = av_write_frame(formatCtx, avPacket);
	if (callStat != 0) 
		throw std::runtime_error("Error while writing frame to format: " + avErrStr(callStat));
 */

    //
    // Cleanup
    //

	// delete[] chY;
	// delete[] chU;
	// delete[] chV;
	

	av_frame_free(&workingFrame);
	av_packet_free(&avPacket);

	avcodec_free_context(&videoStream->codec);
	avformat_free_context(formatCtx);
	avio_context_free(&avioCtx);
    av_free(alloc_buf);


	myfile.close();

	return nullptr;
}

} // namespace imgc
