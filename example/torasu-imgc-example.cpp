#include <iostream>
#include <iomanip>

#include <lodepng.h>

#include <torasu/torasu.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/EIcore_runner.hpp>
#include <torasu/std/Dstring.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dfile.hpp>
#include <torasu/std/Rnet_file.hpp>

#include <torasu/mod/imgc/Rimg_file.hpp>


using namespace std;
using namespace torasu;
using namespace torasu::tstd;
using namespace imgc;

inline const char* printCode(u_int8_t value, const char** codeSet) {
	
	if (value > 0xf2) {
		return codeSet[3];
	} else if (value > 0xd0) {
		return codeSet[2];
	} else if (value > 0x80) {
		return codeSet[1];
	} else if (value > 0x40) {
		return codeSet[0];
	} else {
		return " ";
	}
}

void netImageTest() {
	Rnet_file file("https://assets.gitlab-static.net/uploads/-/system/project/avatar/14033279/TorasuLogo2Color.png");

	Rimg_file tree(&file);
	

	EIcore_runner* runner = new EIcore_runner();

	ExecutionInterface* ei = runner->createInterface();

	tools::RenderInstructionBuilder rib;

	Dbimg_FORMAT format(400, 400);

	auto rf = format.asFormat();

	auto handle = rib.addSegmentWithHandle<Dbimg>(TORASU_STD_PL_VIS, &rf);

	cout << "RENDER BEGIN" << endl;
	/*for (int i = 0; i < 120; i++) {
		std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
		auto result = rib.runRender(&tree, NULL, ei);
		cout << "NEXT FRAME " << i << endl;
		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
		delete result;
	}*/

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	auto result = rib.runRender(&tree, NULL, ei);

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
	cout << "RENDER FIN" << endl;

	auto castedRes = handle.getFrom(result);

	ResultSegmentStatus rss = castedRes.getStatus();

	cout << "STATUS " << rss << endl;

	if (rss >= ResultSegmentStatus::ResultSegmentStatus_OK) {
		Dbimg* bimg = castedRes.getResult();

		
		int width = bimg->getWidth();
		int height = bimg->getHeight();
		/*int channels = 4;
		uint8_t* data = &(*bimg->getImageData())[0];

				
		const char* redCodes[] = {"\33[31m|", "\33[31;1m|", "\33[30m\33[101m|", "\33[31;1m\33[101m|"};
		const char* greenCodes[] = {"\33[32m|", "\33[32;1m|", "\33[30m\33[102m|", "\33[32;1m\33[102m|"};
		const char* blueCodes[] = {"\33[34m|", "\33[34;1m|", "\33[30m\33[104m|", "\33[34;1m\33[104m|"};

		int i = 0;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {

				u_int8_t red = data[i];
				u_int8_t green = data[i+1];
				u_int8_t blue = data[i+2];
				
				cout << printCode(red, redCodes) << "\33[0m";
				cout << printCode(green, greenCodes) << "\33[0m";
				cout << printCode(blue, blueCodes)  << "\33[0m";

				i+=channels;
			}
			cout << endl;
		}*/

		unsigned error = lodepng::encode("test-res/out.png", *bimg->getImageData(), width, height);

		cout << "ENCODE STAT " << error;
	}




	delete result;

	delete ei;
	delete runner;
}

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/avutil.h>
	#include <libswscale/swscale.h>
	#include <libavutil/error.h>
	#include <libavutil/frame.h>
}

bool video_decode_example(const char *outfilename, const char *filename) {

	AVFormatContext* av_format_ctx = avformat_alloc_context();

	if (!av_format_ctx) {
		return false;
	}

	// Open file
    if (avformat_open_input(&av_format_ctx, filename, NULL, NULL) != 0) {
       return false;
    }

    // Get infromation about streams
    if (avformat_find_stream_info(av_format_ctx, NULL) < 0) {
       return false;
    }
	
	int video_stream_index = -1;
	int width, height;
	AVCodecContext* av_codec_ctx;
	AVCodec* av_codec;
	AVCodecParameters* av_codec_params;
	
	video_stream_index = -1;

	for (unsigned int i = 0; i < av_format_ctx->nb_streams; i++)
	{
		AVStream* stream = av_format_ctx->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
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
		return false;
	}

	av_codec_ctx = avcodec_alloc_context3(av_codec);
	if (!av_codec_ctx) {
		return false;
	}

	if (avcodec_parameters_to_context(av_codec_ctx, av_codec_params) < 0) {
		return false;
	}

	if(avcodec_open2(av_codec_ctx, av_codec, NULL) < 0) {
		return false;
	}


	double video_framees_per_second = av_q2d(av_format_ctx->streams[video_stream_index]->r_frame_rate);
	double video_base_time = av_q2d(av_format_ctx->streams[video_stream_index]->time_base); 
	// FIXME video_base_time doesnt seem to make any sesnse
	
	width   = av_codec_ctx->width;
	height  = av_codec_ctx->height;

	cout << "GOT VIDEO:" << endl
			<< "FPS	" << video_framees_per_second << endl
			<< "TIME	" << video_base_time << endl
			<< "RES " << width << "x" << height << endl;


	AVFrame* av_frame = av_frame_alloc();
	if (!av_frame) {
		return false;
	}
	AVPacket* av_packet = av_packet_alloc();
	if (!av_packet) {
		return false;
	}

	int response;
	int frameNum = 0;
	SwsContext* sws_scaler_ctx = NULL;
	while (av_read_frame(av_format_ctx, av_packet) >= 0) {

		if (av_packet->stream_index != video_stream_index) {
			continue;
		}
		response = avcodec_send_packet(av_codec_ctx, av_packet);
		if (response < 0) {
			char errStr[100];
			av_strerror(response, errStr, 100);
			printf("Failed to decode packet: %s\n", errStr);
			return false;
		}

		int response = avcodec_receive_frame(av_codec_ctx, av_frame);
		
		if (response == AVERROR(EAGAIN)) {
			cout << "~EAGAIN" << endl;
			continue;
		} else if (response < 0) {
			char errStr[100];
			av_strerror(response, errStr, 100);
			printf("Failed to recieve frame: %s\n", errStr);
			return false;
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
				return false;
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
	
	
	sws_freeContext(sws_scaler_ctx);
	avformat_close_input(&av_format_ctx);
	avformat_free_context(av_format_ctx);
	av_frame_free(&av_frame);
	av_packet_free(&av_packet);
	avcodec_free_context(&av_codec_ctx);

	return true;
}

void avTest() {
	cout << "ello avTest()" << endl;

	bool res = video_decode_example("out.ex", "test-res/in.mp4");
	cout << " >> " << res << endl;
}

int main() {
	//netImageTest();
	avTest();

	return 0;
}