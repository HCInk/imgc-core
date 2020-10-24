#ifndef INCLUDE_TORASU_MOD_IMGC_MEDIAENCODER_HPP_
#define INCLUDE_TORASU_MOD_IMGC_MEDIAENCODER_HPP_

#include <vector>
#include <utility>
#include <functional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
}

#include <torasu/std/Dfile.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Daudio_buffer.hpp>

namespace imgc {

class MediaEncoder {
public:
	class FrameRequest {
	protected:
		FrameRequest() {}
		virtual ~FrameRequest() {}
	};

	class VideoFrameRequest : FrameRequest {
	private:
		double time;
		torasu::tstd::Dbimg* result;
		torasu::tstd::Dbimg_FORMAT* format;

	protected:
		VideoFrameRequest(double time, torasu::tstd::Dbimg_FORMAT* format) 
			: time(time), format(format) {}
		~VideoFrameRequest() {}

		inline torasu::tstd::Dbimg* getResult() {return result;}
	
	public:
		inline double getTime() {return time;}

		inline void setResult(torasu::tstd::Dbimg* result) {this->result = result;}
		inline torasu::tstd::Dbimg_FORMAT* getFormat() {return format;}
	};

	class AudioFrameRequest : FrameRequest {
	private:
		double start, duration;
		torasu::tstd::Daudio_buffer* result;
	
	protected:
		AudioFrameRequest(double start, double duration) 
			: start(start), duration(duration) {}
		~AudioFrameRequest() {}

		inline torasu::tstd::Daudio_buffer* getResult() {return result;}
	
	public:
		inline double getStart() {return start;}
		inline double getDuration() {return duration;}

		inline void setResult(torasu::tstd::Daudio_buffer* result) {this->result = result;}
	};

	struct EncodeRequest {

	};

	typedef std::function<int(FrameRequest*)> FrameCallbackFunc;

private:
	FrameCallbackFunc frameCallbackFunc;	

public:
	MediaEncoder(FrameCallbackFunc frameCallbackFunc) 
		: frameCallbackFunc(frameCallbackFunc) {}
	~MediaEncoder() {}

	torasu::tstd::Dfile* encode(EncodeRequest request);
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_MEDIAENCODER_HPP_
