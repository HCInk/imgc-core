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
	torasu::Renderable* source;
	AVFormatContext* av_format_ctx;
	bool input_laoded = false;

	int video_stream_index = -1;
	int audio_stream_index = -1;
	int width, height;
	AVCodecContext* av_codec_ctx = NULL;
	AVCodec* av_codec = NULL;
	AVCodecParameters* av_codec_params = NULL;
	int av_codec_video_delay;

    AVCodecParameters* av_audio_coded_params = NULL;
    AVCodec* av_audio_codec = NULL;
    AVCodecContext* av_audio_codec_ctx = NULL;
    int32_t audio_sample_rate;
    int32_t audio_frame_size;

	VideoLoader::FrameProperties* av_codec_fp_buf = NULL;
	int av_codec_fp_buf_len;
	int av_codec_fp_buf_pos;
	
	SwsContext* sws_scaler_ctx = NULL;
	
	double video_framees_per_second;
	double video_base_time;

	FrameProperties current_fp;
	AVFrame* av_frame = NULL;
    AVFrame* av_audio_frame = NULL;

    AVPacket* av_packet = NULL;

    std::ofstream* stream;


    bool loaded = false;

	const size_t alloc_buf_len = 32 * 1024;

	VideoLoader::FileReader in_stream;
	torasu::RenderResult* sourceFetchResult = NULL;

	int64_t lastReadDts = INT64_MIN;
	bool draining = false;

	void nextPacket();
	torasu::tstd::Dbimg* getFrame(double targetPos, int32_t width, int32_t height);

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
