//
// Created by Yann Holme Nielsen on 14/06/2020.
//

#ifndef IMGC_VIDEOFILEDESERIALIZER_HPP
#define IMGC_VIDEOFILEDESERIALIZER_HPP

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
}

struct AudioFrame {
	int64_t start;
	int64_t end;
	int numSamples;
	int size;
	std::vector<uint8_t*> data;
};
struct VideoFrame {
	int64_t start;
	int64_t end;
	uint32_t size;
	uint8_t* data;
};
struct DecodingState {
	double requestStart;
	double requestEnd;

	bool videoPresent = false;
	bool audioPresent = false;

	std::vector<AudioFrame> audioParts;
	std::vector<VideoFrame> vidFrames;

	int frameWidth;
	int frameHeight;
};

struct BufferedFrame {
	int64_t startTime;
	int64_t pos;
	int64_t duration;
};

struct StreamEntry {
	int id;
	AVMediaType codecType;
	AVCodec* codec = nullptr;
	AVCodecContext* ctx = nullptr;
	AVCodecParameters* ctx_params = nullptr;
	AVRational base_time;
	//video specific
	int vid_delay = 0;
	AVRational vid_fps;
	int flushCount = 0;

	// Stream-state
	bool frameIsPresent = false;
	AVFrame* frame;
	std::vector<BufferedFrame> cachedFrames;
	int64_t duration;
	bool draining = false;
	int64_t nextFramePts = 0;
};
class VideoFileDeserializer {
public:
	VideoFileDeserializer(std::string path);

	~VideoFileDeserializer();
	struct FileReader {
		uint8_t* data;
		size_t size;
		size_t pos;
	};
	std::vector<StreamEntry*> streams;
private:
	FileReader in_stream;
	void prepare();
	AVFormatContext* av_format_ctx;

	int vid_stream_id = -1;
	int audio_stream_id = -1;

	double decoderPosition = -1;
	SwsContext* sws_scaler_ctx = nullptr;


	AVPacket* av_packet;
	StreamEntry* getEntryById(int index);
	void removeCacheFrame(int64_t pos, std::vector<BufferedFrame>* list);
	void extractVideoFrame(StreamEntry* stream, uint8_t* outPt);
//    int32_t* getRealBounds(StreamEntry* stream);

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
	 */
	void drainStream(StreamEntry* stream, DecodingState* decodingState);
	/**
	 * @brief Concatenates audio into one single frame, matching the requested size
	 * @param decodingState The current DecodingState to be updated
	 */
	void concatAudio(DecodingState* decodingState);

public:

	VideoFileDeserializer();
	DecodingState* getSegment(double start, double end);

};


#endif //IMGC_VIDEOFILEDESERIALIZER_HPP
