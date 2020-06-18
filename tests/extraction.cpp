//
// Created by Yann Holme Nielsen on 18/06/2020.
//

#include <lodepng.h>
#include <string>
#include <fstream>
#include "../src/VideoFileDeserializer.hpp"

void writeFrames(std::vector<VideoFrame> frames, std::string base_path, int w, int h) {
    for (int i = 0; i < frames.size(); ++i) {
        auto curr = frames[i];
        unsigned error = lodepng::encode(base_path + "file-" + std::to_string(i + 1) + ".png",
                                         curr.data, w, h);

    }
}

void writeAudio(std::string path, std::vector<AudioFrame> frames) {
    std::ofstream out(path);
    for (int j = 0; j < frames.size(); ++j) {
        auto part = frames[j];
        size_t size = 4;
        uint8_t *l = part.data[0];
        uint8_t *r = part.data[1];

        for (int i = 0; i < part.numSamples; i++) {
            out.write(reinterpret_cast<const char *>(l), size);
            out.write(reinterpret_cast<const char *>(r), size);
            l += size;
            r += size;
        }

    }
    out.close();
}

int main() {
    VideoFileDeserializer des("143386147_Superstar W.mp4");
  //  auto totalLength = des.streams[0]->duration * av_q2d(des.streams[0]->base_time);
    auto result = des.getSegment(0.05, 3);
    auto result2 = des.getSegment(0.04, 0.12);
    auto result3 = des.getSegment(3.04, 4.08);
    auto result4 = des.getSegment(4.08, 5);
    auto result5 = des.getSegment(5.04, 5.33);


    writeFrames(result->vidFrames, std::string("test_files/one/"), result->frameWidth, result->frameHeight);
    writeAudio(std::string("test_files/one/audio.pcm"), result->audioParts);

    writeFrames(result2->vidFrames, std::string("test_files/two/"), result2->frameWidth, result2->frameHeight);
    writeAudio(std::string("test_files/two/audio.pcm"), result2->audioParts);


    writeFrames(result3->vidFrames, std::string("test_files/three/"), result3->frameWidth, result3->frameHeight);
    writeAudio(std::string("test_files/three/audio.pcm"), result3->audioParts);

    writeFrames(result4->vidFrames, std::string("test_files/four/"), result4->frameWidth, result4->frameHeight);
    writeAudio(std::string("test_files/four/audio.pcm"), result4->audioParts);

    writeFrames(result5->vidFrames, std::string("test_files/five/"), result5->frameWidth, result5->frameHeight);
    writeAudio(std::string("test_files/five/audio.pcm"), result5->audioParts);


}