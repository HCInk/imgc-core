//
// Created by Yann Holme Nielsen on 14/06/2020.
//

#ifndef IMGC_MEDIADECODER_HPP
#define IMGC_MEDIADECODER_HPP

#include <vector>
#include <list>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
}

#include <torasu/torasu.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dbimg_sequence.hpp>
#include <torasu/std/Daudio_buffer.hpp>
#include <torasu/mod/imgc/FFmpegLogger.h>

namespace imgc {

struct LongRational {
	int64_t num; ///< Numerator
	int64_t den; ///< Denominator
};

struct SegmentRequest {
	double start;
	double end = -1;

	torasu::tstd::Dbimg_sequence** videoBuffer = NULL;
	torasu::tstd::Daudio_buffer** audioBuffer = NULL;
};

struct AudioFrame {
	int64_t start;
	int64_t end;
	int numSamples;
	size_t size;
	std::vector<uint8_t*> data;
};

struct DecodingState {
	double requestStart;
	double requestEnd;

	bool videoDone = false;
	bool audioDone = false;

	bool videoAvailable = false;
	bool audioAvailable = false;

	int frameWidth;
	int frameHeight;

	// Video has been read until this point (exclusive) - INT64_MIN = no frame has been read yet
	int64_t videoReadUntil = INT64_MIN;
	std::vector<AudioFrame> audioFrames;

	SegmentRequest segmentRequest;
};

struct BufferedFrame {
	int64_t startTime;
	int64_t pos;
	int64_t duration;
};

struct StreamEntry {
	int index;
	AVMediaType codecType;
	const AVCodec* codec = nullptr;
	AVCodecContext* ctx = nullptr;
	AVCodecParameters* ctx_params = nullptr;
	AVRational base_time;
	//video specific
	int vid_delay = 0;
	AVRational vid_fps;

	// Stream-state
	bool frameIsPresent = false;
	AVFrame* frame;
	std::vector<BufferedFrame> cachedFrames;
	int64_t duration;
	bool draining = false;
	int64_t nextFramePts = 0;
	// state for prebuffering next frame if neccessary
	bool nextFrameIsPresent = false;
	bool enrichFromNextFrame = false;
	AVFrame* nextFrame = nullptr;
};
class MediaDecoder {
private:
	torasu::LogInstruction* li = nullptr;
	std::list<FFmpegLogger> loggers;

	class LogInstructionSession {
	private:
		MediaDecoder* dec;
	public:
		LogInstructionSession(MediaDecoder* dec, torasu::LogInstruction* li)
			: dec(dec) {
			dec->li = li;
		}
		~LogInstructionSession() {
			dec->li = nullptr;
		}
	};
public:
	MediaDecoder(std::string path);
	MediaDecoder(uint8_t* dataP, size_t len, torasu::LogInstruction li);

	~MediaDecoder();

	void getSegment(SegmentRequest request);
	void getSegment(SegmentRequest request, torasu::LogInstruction li);

	std::pair<int32_t, int32_t> getDimensions();
	double getDuration();

	struct FileReader {
		uint8_t* data;
		size_t size;
		size_t pos;
	};

	std::vector<StreamEntry*> streams;

private:
	FileReader in_stream;
	bool freeInput;
	void prepare();
	AVFormatContext* av_format_ctx;

	int video_stream_index = -1;
	int audio_stream_index = -1;
	LongRational duration = {0, -1};

	double decoderPosition = -1;
	SwsContext* sws_scaler_ctx = nullptr;
	AVFrame* scaled_frame = nullptr;


	AVPacket* av_packet;
	StreamEntry* getStreamEntryByIndex(int index);
	static void removeCacheFrame(int64_t pos, std::vector<BufferedFrame>* list);
	void extractVideoFrame(StreamEntry* stream, uint8_t* outPt);

	enum FrameRecieveMode {
		/** @brief Just read the next frame - no end expected */
		FrameRecieveMode_NORMAL,
		/** @brief Set if end is awaited, so it can be handled properly */
		FrameRecieveMode_END,
		/** @brief Flush mode, this will just recieve a frame from the codec (if possible)
		 * but wont create a valid result of the result */
		FrameRecieveMode_FLUSH,
	};

	enum FrameRecieveResult {
		/** @brief sucessfully recieved new frame into stream->frame */
		FrameRecieveResult_RECIEVED,
		/** @brief not enough input yet to give output */
		FrameRecieveResult_NEEDS_INPUT,
		/** @brief stream has ended */
		FrameRecieveResult_END
	};

	/**
	 * @brief  Recieve frame from codec, makes sure some data is enrichted
	 * @param  stream: Stream to recieved from
	 * @param  mode: Mode to recieve
	 * @retval Result of fetch
	 */
	static FrameRecieveResult recieveFrameFromCodec(StreamEntry* stream, FrameRecieveMode mode);

	/**
	 * @brief  Converts float time into an base_time-format, by applying the AVStream->base_time
	 * @param  value: The float value to be converted
	 * @param  base: The AVStream->base_time to be used for the conversion
	 */
	int64_t toBaseTime(double value, AVRational base);
	/**
	 * @brief  Checks if a frame is is partially or completely inside the given boundary
	 * @param  frame: The frame to be checked
	 * @param  start: Start of the area frames should be accepted (in AVStream->base_time)
	 * @param  end: End of the area frames should be accepted (in AVStream->base_time)
	 * @retval true if inside the boundary, otherwise false
	 */
	static bool checkFrameTargetBound(AVFrame* frame, int64_t start, int64_t end);
	/**
	 * @brief  Handles frame on the given stream
	 * @note   Must be called in the correct order, no skipping allowed between calls per session
	 * @param  stream: The stream, which is holding the frame to be evaluated
	 * @param  decodingState: The current DecodingState to be updated
	 */
	void handleFrame(StreamEntry* stream, DecodingState* decodingState);
	/**
	 * @brief  Fetches existing results into current DecodingState
	 * @param  decodingState: The current DecodingState to be updated
	 */
	void fetchBuffered(DecodingState* decodingState);
	/**
	 * @brief  Seeks to a position that is before all packets that are still required to be found
	 * @param  decodingState: The current DecodingState to be updated
	 */
	void initializePosition(DecodingState* decodingState);
	/**
	 * @brief Flushes buffers of stream to be reused after drain
	 * @param stream The stream to be flushed
	 */
	void flushBuffers(StreamEntry* stream);
	/**
	 * @brief Drain a stream
	 * @param stream The stream to be drained
	 * @param decodingState The current DecodingState to be updated
	 * @param record Handle drained frame (true for end of stream situations - false when draining before end in seeking)
	 */
	void drainStream(StreamEntry* stream, DecodingState* decodingState, bool record);
	/**
	 * @brief Concatenates audio into one single frame, matching the requested size
	 * @param decodingState The current DecodingState to be updated
	 */
	void concatAudio(DecodingState* decodingState);

	DecodingState* createDecoderState(SegmentRequest request);

	size_t determineSampleSize(StreamEntry* stream);

public:
	static std::string getErrorMessage(int errorCode);

};

} // namespace imgc

#endif //IMGC_MEDIADECODER_HPP
