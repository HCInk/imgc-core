//
// Created by Yann Holme Nielsen on 18/06/2020.
//

#include <lodepng.h>
#include <string>
#include <fstream>
#include <thread>
#include <iostream>
#include <torasu/mod/imgc/VideoFileDeserializer.hpp>

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
    auto result6 = des.getSegment(5.33, 6);
    
	std::vector<torasu::tstd::Dbimg*>* videoBuffer;
	torasu::tstd::Dbimg_FORMAT videoFmt(200, 300);
	auto resultB1 = des.getSegment((SegmentRequest) {
		.start = 1,
		.end = 1.5,
		.videoBuffer = &videoBuffer,
		.videoFormat = &videoFmt,
		.audioBuffer = NULL,
		.audioFormat = NULL
	});


	torasu::tstd::Daudio_buffer* audioBuffer;
	torasu::tstd::Daudio_buffer_FORMAT audioFmt(44100, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32);
	auto resultB2 = des.getSegment((SegmentRequest) {
		.start = 1,
		.end = 1.5,
		.videoBuffer = NULL,
		.videoFormat = NULL,
		.audioBuffer = &audioBuffer,
		.audioFormat = &audioFmt
	});

	std::cout << "B1 Video-Size: " << resultB1->vidFrames.size() << " AudioSize: " << resultB1->audFrames.size() << std::endl;

	std::cout << "B2 Video-Size: " << resultB2->vidFrames.size() << " AudioSize: " << resultB2->audFrames.size() << std::endl;

	std::thread write1([&result](){ 
		
		writeFrames(result->vidFrames, std::string("test_files/one/"), result->frameWidth, result->frameHeight);
		writeAudio(std::string("test_files/one/audio.pcm"), result->audFrames);

	});
	std::thread write2([&result2](){ 
		
		writeFrames(result2->vidFrames, std::string("test_files/two/"), result2->frameWidth, result2->frameHeight);
		writeAudio(std::string("test_files/two/audio.pcm"), result2->audFrames);

	});
	std::thread write3([&result3](){ 
		
		writeFrames(result3->vidFrames, std::string("test_files/three/"), result3->frameWidth, result3->frameHeight);
		writeAudio(std::string("test_files/three/audio.pcm"), result3->audFrames);

	});
	std::thread write4([&result4](){ 
		
		writeFrames(result4->vidFrames, std::string("test_files/four/"), result4->frameWidth, result4->frameHeight);
		writeAudio(std::string("test_files/four/audio.pcm"), result4->audFrames);

	});
	std::thread write5([&result5](){ 
		
		writeFrames(result5->vidFrames, std::string("test_files/five/"), result5->frameWidth, result5->frameHeight);
		writeAudio(std::string("test_files/five/audio.pcm"), result5->audFrames);

	});
	std::thread write6([&result6](){ 
		
		writeFrames(result6->vidFrames, std::string("test_files/six/"), result6->frameWidth, result6->frameHeight);
		writeAudio(std::string("test_files/six/audio.pcm"), result6->audFrames);

	});

	write1.join();
	write2.join();
	write3.join();
	write4.join();
	write5.join();
	write6.join();

}