#ifndef INCLUDE_TORASU_MOD_IMGC_VIDEOLOADER_HPP_
#define INCLUDE_TORASU_MOD_IMGC_VIDEOLOADER_HPP_

#include <string>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/avutil.h>
	#include <libswscale/swscale.h>
	#include <libavutil/error.h>
	#include <libavutil/frame.h>
}

namespace imgc {

class VideoLoader {
private:
	std::string filename;
	AVFormatContext* av_format_ctx;
	
	int video_stream_index = -1;
	int width, height;
	AVCodecContext* av_codec_ctx;
	AVCodec* av_codec;
	AVCodecParameters* av_codec_params;
	
	SwsContext* sws_scaler_ctx = NULL;
	
	double video_framees_per_second;
	double video_base_time;

	AVFrame* av_frame;
	AVPacket* av_packet;
public:
	VideoLoader(std::string filename);
	~VideoLoader();

	bool video_decode_example();
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_VIDEOLOADER_HPP_
