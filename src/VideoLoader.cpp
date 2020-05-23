#include "../include/torasu/mod/imgc/VideoLoader.hpp"

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <istream>
#include <fstream>

#include <torasu/render_tools.hpp>
#include <torasu/std/Dfile.hpp>
#include <torasu/std/pipeline_names.hpp>

#include <lodepng.h>

using namespace std;
using namespace torasu;
using namespace torasu::tstd;

namespace imgc {


int ReadFunc(void* ptr, uint8_t* buf, int buf_size) {
    FileReader* reader = reinterpret_cast<FileReader*>(ptr);

    size_t nextPos = reader->pos+buf_size;
	size_t read;

	if (nextPos <= reader->size) {
		read = buf_size;
	} else {
		nextPos = reader->size;
		read = nextPos-reader->pos;
	}

	memcpy(buf, reader->data + reader->pos, read);
	cout << "R " << read << " OF " << buf_size << endl;
	reader->pos+=read;
	if (read > 0) {
		return read;
	} else {
		return -1;
	}
}
// whence: SEEK_SET, SEEK_CUR, SEEK_END (like fseek) and AVSEEK_SIZE, which is meant to return the video's size without changing the position
int64_t SeekFunc(void* ptr, int64_t pos, int whence) {
    FileReader* reader = reinterpret_cast<FileReader*>(ptr);
	
	cerr << "SEEK " << pos << " WHENCE " << whence << endl;
	switch (whence) {
	case SEEK_SET:
	 	reader->pos = pos;
	 	break;
	case SEEK_CUR:
	 	reader->pos += pos;
	 	break;
	case SEEK_END:
	 	reader->pos = reader->size+pos;
	 	break;
	case AVSEEK_SIZE:
		cerr << "SEEK SIZE" << endl;
    	return reader->size;
	default:
	 	cerr << "UNSUPP-SEEK " << pos << " WHENCE " << whence << endl;
	 	break;
	}

    // Return the new position:
    return reader->pos;
}

VideoLoader::VideoLoader(torasu::Renderable* source) {
	this->source = source;
	av_format_ctx = avformat_alloc_context();

	if (!av_format_ctx) {
		throw runtime_error("Failed allocating av_format_ctx!");
	}


}

VideoLoader::~VideoLoader() {

	if (sws_scaler_ctx != NULL) {
		sws_freeContext(sws_scaler_ctx);
	}
	if (av_frame != NULL) {
		av_frame_free(&av_frame);
	}
	if (av_packet != NULL) {
		av_packet_free(&av_packet);
	}
	if (av_codec_ctx != NULL) {
		avcodec_free_context(&av_codec_ctx);
	}

	if (input_laoded) {
		avformat_close_input(&av_format_ctx);
	}

	avformat_free_context(av_format_ctx);

	if (sourceFetchResult != NULL) {
		delete sourceFetchResult;
	}

}

void VideoLoader::load(torasu::ExecutionInterface* ei) {

	if (sourceFetchResult != NULL) {
		delete sourceFetchResult;
	}

	tools::RenderInstructionBuilder rib;

	auto handle = rib.addSegmentWithHandle<Dfile>(TORASU_STD_PL_FILE, NULL);

	sourceFetchResult = rib.runRender(source, NULL, ei);

	auto castedResultSeg = handle.getFrom(sourceFetchResult);

	if (castedResultSeg.getResult() == NULL) {
		throw runtime_error("Sub-render of file failed");
	}

	auto data = castedResultSeg.getResult()->getFileData();
	
	in_stream.data = data->data();
	in_stream.size = data->size();
	in_stream.pos = 0;

	// Open file
    /*if (avformat_open_input(&av_format_ctx, filename.c_str(), NULL, NULL) != 0) {
		throw runtime_error("Failed to open input file");
    }*/
	
	uint8_t* alloc_buf = (uint8_t*) av_malloc(alloc_buf_len);

	av_format_ctx->pb = avio_alloc_context(alloc_buf, alloc_buf_len, false, &in_stream, ReadFunc, NULL, SeekFunc);

	if(!av_format_ctx->pb) {
		av_free(alloc_buf);
		throw runtime_error("Failed allocating avio_context!");
	}
	av_format_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
	av_format_ctx->probesize = 1200000;

	// Need to probe buffer for input format unless you already know it
	AVProbeData probeData;
	probeData.buf = alloc_buf;
	probeData.buf_size = alloc_buf_len;
	probeData.filename = "";

	AVInputFormat* av_input_format;// = av_probe_input_format(&probeData, 1);

	//if(!pAVInputFormat)
	av_input_format = av_probe_input_format(&probeData, 1);

	if(!av_input_format) {
		throw runtime_error("Failed to create input-format!");
	}

	av_input_format->flags |= AVFMT_NOFILE;
	
	if (avformat_open_input(&av_format_ctx, "", NULL, NULL) != 0) {
		throw runtime_error("Failed to open input stream");
    }


    // Get infromation about streams
    if (avformat_find_stream_info(av_format_ctx, NULL) < 0) {
		throw runtime_error("Failed to find stream info");
    }

	for (unsigned int i = 0; i < av_format_ctx->nb_streams; i++) {
		AVStream* stream = av_format_ctx->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			av_codec_params = stream->codecpar;
			av_codec = avcodec_find_decoder(av_codec_params->codec_id);

			if (!av_codec) {
				continue;
			}
			video_stream_index = i;

			break;
		}
	}

	if (video_stream_index < 0) {
		throw runtime_error("Failed to find suitable video stream!");
	}

	av_codec_ctx = avcodec_alloc_context3(av_codec);
	if (!av_codec_ctx) {
		throw runtime_error("Failed to allocate av_codec_ctx!");
	}

	if (avcodec_parameters_to_context(av_codec_ctx, av_codec_params) < 0) {
		throw runtime_error("Failed to link parameters to context!");
	}

	if (avcodec_open2(av_codec_ctx, av_codec, NULL) < 0) {
		throw runtime_error("Failed to initialize av_codec_ctx with av_codec!");
	}

	video_framees_per_second = av_q2d(av_format_ctx->streams[video_stream_index]->r_frame_rate);
	video_base_time = av_q2d(av_format_ctx->streams[video_stream_index]->time_base); 
	// FIXME video_base_time doesnt seem to make any sesnse
	
	width   = av_codec_ctx->width;
	height  = av_codec_ctx->height;

	cout << "GOT VIDEO:" << endl
			<< "FPS	" << video_framees_per_second << endl
			<< "TIME	" << video_base_time << endl
			<< "RES " << width << "x" << height << endl;
	


	av_frame = av_frame_alloc();
	if (!av_frame) {
		throw runtime_error("Failed to allocate av_frame");
	}

	av_packet = av_packet_alloc();
	if (!av_packet) {
		throw runtime_error("Failed to allocate av_packet");
	}
}

void VideoLoader::video_decode_example() {

	int response;
	int frameNum = 0;
	
	while (true) {
		int nextFrameStat = av_read_frame(av_format_ctx, av_packet);

		if (nextFrameStat < 0) {
			break;
		}

		if (av_packet->stream_index != video_stream_index) {
			continue;
		}

		response = avcodec_send_packet(av_codec_ctx, av_packet);
		if (response < 0) {
			char errStr[100];
			av_strerror(response, errStr, 100);
			std::string msgExcept = "Failed to decode packet: ";
			msgExcept += errStr;
			throw runtime_error(msgExcept);
		}

		int response = avcodec_receive_frame(av_codec_ctx, av_frame);
		
		if (response == AVERROR(EAGAIN)) {
			continue;
		} else if (response < 0) {
			char errStr[100];
			av_strerror(response, errStr, 100);
			std::string msgExcept = "Failed to recieve frame: ";
			msgExcept += errStr;
			throw runtime_error(msgExcept);
		}

		stringstream out_name;
		out_name << "test-res/out";
		out_name << std::setfill('0') << std::setw(5) << frameNum;
		out_name << ".png";
		
		if (!sws_scaler_ctx) {

			sws_scaler_ctx = sws_getContext(av_frame->width, av_frame->height, av_codec_ctx->pix_fmt,
											av_frame->width, av_frame->height, AV_PIX_FMT_RGB0,
											SWS_FAST_BILINEAR, NULL, NULL, NULL);
			if (!sws_scaler_ctx) {
				throw runtime_error("Failed to create sws_scaler_ctx!");
			}

		}
		
		vector<uint8_t> rgbaData(av_frame->width * av_frame->height * 4);
		uint8_t* dst[4] = {rgbaData.data(), NULL, NULL, NULL};
		int dest_linesize[4] = { av_frame->width*4, 0, 0, 0 };
		sws_scale(sws_scaler_ctx, av_frame->data, av_frame->linesize, 0, av_frame->height,dst, dest_linesize);

		unsigned error = lodepng::encode(out_name.str(), rgbaData, av_frame->width, av_frame->height);
		if (error) {
			cerr << "Failed encoding png: " << lodepng_error_text(error) << endl;
		} else {
			cout << "Saved " << out_name.str() << endl;
		}

		av_packet_unref(av_packet);

		frameNum++;
	}
}

} // namespace imgc