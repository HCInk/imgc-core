#ifndef INCLUDE_TORASU_MOD_IMGC_VIDEOLOADER_HPP_
#define INCLUDE_TORASU_MOD_IMGC_VIDEOLOADER_HPP_

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/mathematics.h>
	#include <libavutil/avutil.h>
	#include <libswscale/swscale.h>
	#include <libavutil/error.h>
	#include <libavutil/frame.h>
}

#include <string>
#include <map>
#include <istream>
#include <fstream>

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/std/spoilsD.hpp>

namespace imgc {


class VideoLoader : public torasu::tools::SimpleRenderable {
public: 
	//
	// Helper structs
	//

	struct FileReader {
		uint8_t* data;
		size_t size;
		size_t pos;
	};

	struct FrameProperties {
		bool loaded;
		double start;
		double duration;
	};

private:

	// Input/Reading
	
	const size_t alloc_buf_len = 32 * 1024;

	torasu::Renderable* source;
	AVFormatContext* av_format_ctx;
	bool input_laoded = false;
    bool loaded = false;

    AVPacket* av_packet = NULL;

	VideoLoader::FileReader in_stream;
	torasu::RenderResult* sourceFetchResult = NULL;

    int64_t lastReadDts = INT64_MIN;
    int64_t lastReadPackPos = INT64_MIN;

	bool draining = false;

	// Video Data

	int video_stream_index = -1;
	int video_width, video_height;
	AVCodec* video_codec = NULL;
	AVCodecContext* video_codec_ctx = NULL;
	AVCodecParameters* video_codec_params = NULL;
	int video_codec_delay;
	
	SwsContext* sws_scaler_ctx = NULL;
	double video_framees_per_second;
	double video_base_time;

	AVFrame* video_frame = NULL;

	FrameProperties current_fp;
	VideoLoader::FrameProperties* video_codec_fp_buf = NULL;
	int video_codec_fp_buf_len;
	int video_codec_fp_buf_pos;

	// Audio Data

	int audio_stream_index = -1;
    AVCodec* audio_codec = NULL;
    AVCodecContext* audio_codec_ctx = NULL;
    AVCodecParameters* audio_codec_params = NULL;
    int32_t audio_sample_rate;
    int32_t audio_frame_size;

    AVFrame* audio_frame = NULL;

    std::ofstream* audio_out_stream;


	void nextPacket();
	void getFrame(double targetPos, const torasu::tstd::Dbimg_FORMAT& imageFormat, torasu::tstd::Dbimg** outImageFrame);

protected: 
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri);

public:
	VideoLoader(torasu::Renderable* source);
	~VideoLoader();

	std::map<std::string, torasu::Element*> getElements();
	void setElement(std::string key, torasu::Element* elem);

	void load(torasu::ExecutionInterface* ei);
	void flushBuffers();
	void debugPackets();
	void video_decode_example();
	
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_VIDEOLOADER_HPP_
