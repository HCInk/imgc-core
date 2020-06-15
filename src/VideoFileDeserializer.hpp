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
};
struct StreamEntry {
    int id;
    AVMediaType codecType;
    AVCodec* codec = nullptr;
    AVCodecContext * ctx = nullptr;
    AVCodecParameters * ctx_params = nullptr;
    AVFrame * frame;
    AVRational base_time;
    bool frameIsPresent = false;
    std::vector<BufferedFrame> cachedFrames;
    //video specific
    int vid_delay = 0;
    AVRational vid_fps;
    int flushCount = 0;

};
class VideoFileDeserializer {
public:
    ~VideoFileDeserializer();
    struct FileReader {
        uint8_t* data;
        size_t size;
        size_t pos;
    };
private:
    FileReader in_stream;
    void prepare();
    std::vector<StreamEntry*> streams;
    AVFormatContext* av_format_ctx;

    int vid_stream_id = -1;
    int audio_stream_id = -1;

    double decoderPosition = -1;
    SwsContext* sws_scaler_ctx = nullptr;

    bool draining;
    int drainingIndex;
    AVPacket* av_packet;
    StreamEntry* getEntryById(int index);
    void removeCacheFrame(int64_t pos, std::vector<BufferedFrame>* list);
    void extractVideoFrame(StreamEntry* stream, uint8_t* outPt);
    int32_t* getRealBounds(StreamEntry* stream);
    int f = 0;
    int64_t lastPos = 0;
    int64_t toBaseTime(double value, AVRational base);
    static bool checkFrameTargetBound(AVFrame* frame, int64_t start, int64_t end);
    void handleFrame(StreamEntry* stream, DecodingState* decodingState, int64_t targetPosition, int64_t targetPositionEnd);
public:

    VideoFileDeserializer();
    void flushBuffers(StreamEntry* entry);
    void getFrame(double targetPosition);
    DecodingState * getSegment(double start, double end);
    void getAudioSegment(double start, double end);
    void getVideoSegment(double start, double end);

};


#endif //IMGC_VIDEOFILEDESERIALIZER_HPP
