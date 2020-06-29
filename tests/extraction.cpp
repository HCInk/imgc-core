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

void writeAudio(std::string path, torasu::tstd::Daudio_buffer* audioBuff) {
	std::ofstream out(path);
	size_t size = 4;
	uint8_t* l = audioBuff->getChannels()[0].data;
	uint8_t* r = audioBuff->getChannels()[1].data;

	for (int i = 0; i < audioBuff->getChannels()[0].dataSize/size; i++) {
		out.write(reinterpret_cast<const char*>(l), size);
		out.write(reinterpret_cast<const char*>(r), size);
		l += size;
		r += size;
	}
	out.close();
}

int main() {
	VideoFileDeserializer des("143386147_Superstar W.mp4");
	//  auto totalLength = des.streams[0]->duration * av_q2d(des.streams[0]->base_time);
	torasu::tstd::Dbimg_FORMAT vidFormat(-1, -1);
	torasu::tstd::Daudio_buffer_FORMAT audFormat(44100, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32);

	std::vector<torasu::tstd::Dbimg*>* videoResult1;
	torasu::tstd::Daudio_buffer* audioResult1;
	auto result = des.getSegment((SegmentRequest) {
		.start = 0.05,
		.end = 3,
		.videoBuffer = &videoResult1, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult1, .audioFormat = &audFormat
	});

	std::vector<torasu::tstd::Dbimg*>* videoResult2;
	torasu::tstd::Daudio_buffer* audioResult2;
	auto result2 = des.getSegment((SegmentRequest) {
		.start = 0.04,
		.end = 0.12,
		.videoBuffer = &videoResult2, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult2, .audioFormat = &audFormat
	});

	std::vector<torasu::tstd::Dbimg*>* videoResult3;
	torasu::tstd::Daudio_buffer* audioResult3;
	auto result3 = des.getSegment((SegmentRequest) {
		.start = 3.04,
		.end = 4.08,
		.videoBuffer = &videoResult3, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult3, .audioFormat = &audFormat
	});

	std::vector<torasu::tstd::Dbimg*>* videoResult4;
	torasu::tstd::Daudio_buffer* audioResult4;
	auto result4 = des.getSegment((SegmentRequest) {
		.start = 4.08,
		.end = 5,
		.videoBuffer = &videoResult4, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult4, .audioFormat = &audFormat
	});

	std::vector<torasu::tstd::Dbimg*>* videoResult5;
	torasu::tstd::Daudio_buffer* audioResult5;
	auto result5 = des.getSegment((SegmentRequest) {
		.start = 5.04,
		.end = 5.33,
		.videoBuffer = &videoResult5, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult5, .audioFormat = &audFormat
	});

	std::vector<torasu::tstd::Dbimg*>* videoResult6;
	torasu::tstd::Daudio_buffer* audioResult6;
	auto result6 = des.getSegment((SegmentRequest) {
		.start = 5.33,
		.end = 6,
		.videoBuffer = &videoResult6, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult6, .audioFormat = &audFormat
	});

	std::vector<torasu::tstd::Dbimg*>* videoResultB;
	torasu::tstd::Dbimg_FORMAT vidFmtB(200, 300);
	auto resultB1 = des.getSegment((SegmentRequest) {
		.start = 1,
		.end = 1.5,
		.videoBuffer = &videoResultB,
		.videoFormat = &vidFmtB,
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

//	std::cout << "B1 Video-Size: " << resultB1->vidFrames.size() << " AudioSize: " << resultB1->audFrames.size() << std::endl;
//
//	std::cout << "B2 Video-Size: " << resultB2->vidFrames.size() << " AudioSize: " << resultB2->audFrames.size() << std::endl;

	std::thread write1([&result, audioResult1]() {

		writeAudio(std::string("test_files/one/audio.pcm"), audioResult1);
		writeFrames(result->vidFrames, std::string("test_files/one/"), result->frameWidth, result->frameHeight);

	});
	std::thread write2([&result2, audioResult2]() {

		writeAudio(std::string("test_files/two/audio.pcm"), audioResult2);
		writeFrames(result2->vidFrames, std::string("test_files/two/"), result2->frameWidth, result2->frameHeight);

	});
	std::thread write3([&result3, audioResult3]() {

		writeAudio(std::string("test_files/three/audio.pcm"), audioResult3);
		writeFrames(result3->vidFrames, std::string("test_files/three/"), result3->frameWidth, result3->frameHeight);

	});
	std::thread write4([&result4, audioResult4]() {

		writeAudio(std::string("test_files/four/audio.pcm"), audioResult4);
		writeFrames(result4->vidFrames, std::string("test_files/four/"), result4->frameWidth, result4->frameHeight);

	});
	std::thread write5([&result5, audioResult5]() {

		writeAudio(std::string("test_files/five/audio.pcm"), audioResult5);
		writeFrames(result5->vidFrames, std::string("test_files/five/"), result5->frameWidth, result5->frameHeight);

	});
	std::thread write6([&result6, audioResult6]() {

		writeAudio(std::string("test_files/six/audio.pcm"), audioResult6);
		writeFrames(result6->vidFrames, std::string("test_files/six/"), result6->frameWidth, result6->frameHeight);

	});

	write1.join();
	write2.join();
	write3.join();
	write4.join();
	write5.join();
	write6.join();

}