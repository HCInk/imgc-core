//
// Created by Yann Holme Nielsen on 18/06/2020.
//

#include <lodepng.h>
#include <string>
#include <fstream>
#include <thread>
#include <iostream>
#include <torasu/mod/imgc/MediaDecoder.hpp>

void writeFrames(torasu::tstd::Dbimg_sequence* sequence, std::string base_path) {
	auto& frames = sequence->getFrames();
	int i = 0;
	for (auto& frame : frames) {
		std::string path = base_path + "file-" + std::to_string(i + 1) + ".png";
		unsigned error = lodepng::encode(path,
										 frame.second->getImageData(), frame.second->getWidth(), frame.second->getHeight());
		if (error) {
			std::cerr << "LODEPNG ERROR " << error << ": " << lodepng_error_text(error) << " - while writing " << path << std::endl;
		} 
		++i;
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
	MediaDecoder des("143386147_Superstar W.mp4");
	//  auto totalLength = des.streams[0]->duration * av_q2d(des.streams[0]->base_time);
	torasu::tstd::Dbimg_FORMAT vidFormat(-1, -1);
	torasu::tstd::Daudio_buffer_FORMAT audFormat(44100, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32);

	torasu::tstd::Dbimg_sequence* videoResult1;
	torasu::tstd::Daudio_buffer* audioResult1;
	des.getSegment((SegmentRequest) {
		.start = 0.05,
		.end = 3,
		.videoBuffer = &videoResult1, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult1, .audioFormat = &audFormat
	});

	torasu::tstd::Dbimg_sequence* videoResult2;
	torasu::tstd::Daudio_buffer* audioResult2;
	des.getSegment((SegmentRequest) {
		.start = 0.04,
		.end = 0.12,
		.videoBuffer = &videoResult2, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult2, .audioFormat = &audFormat
	});

	torasu::tstd::Dbimg_sequence* videoResult3;
	torasu::tstd::Daudio_buffer* audioResult3;
	des.getSegment((SegmentRequest) {
		.start = 3.04,
		.end = 4.08,
		.videoBuffer = &videoResult3, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult3, .audioFormat = &audFormat
	});

	torasu::tstd::Dbimg_sequence* videoResult4;
	torasu::tstd::Daudio_buffer* audioResult4;
	des.getSegment((SegmentRequest) {
		.start = 4.08,
		.end = 5,
		.videoBuffer = &videoResult4, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult4, .audioFormat = &audFormat
	});

	torasu::tstd::Dbimg_sequence* videoResult5;
	torasu::tstd::Daudio_buffer* audioResult5;
	des.getSegment((SegmentRequest) {
		.start = 5.04,
		.end = 5.33,
		.videoBuffer = &videoResult5, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult5, .audioFormat = &audFormat
	});

	torasu::tstd::Dbimg_sequence* videoResult6;
	torasu::tstd::Daudio_buffer* audioResult6;
	des.getSegment((SegmentRequest) {
		.start = 5.33,
		.end = 6,
		.videoBuffer = &videoResult6, .videoFormat = &vidFormat,
		.audioBuffer = &audioResult6, .audioFormat = &audFormat
	});

	torasu::tstd::Dbimg_sequence* videoResultB;
	torasu::tstd::Dbimg_FORMAT vidFmtB(200, 300);
	des.getSegment((SegmentRequest) {
		.start = 1,
		.end = 1.5,
		.videoBuffer = &videoResultB,
		.videoFormat = &vidFmtB,
		.audioBuffer = NULL,
		.audioFormat = NULL
	});


	torasu::tstd::Daudio_buffer* audioBuffer;
	torasu::tstd::Daudio_buffer_FORMAT audioFmt(44100, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32);
	des.getSegment((SegmentRequest) {
		.start = 1,
		.end = 1.5,
		.videoBuffer = NULL,
		.videoFormat = NULL,
		.audioBuffer = &audioBuffer,
		.audioFormat = &audioFmt
	});


	MediaDecoder desC("104188354_In Memory of Firestarter.mp4");
	torasu::tstd::Dbimg_sequence* videoResultC1;
	torasu::tstd::Daudio_buffer* audioResultC1;
	desC.getSegment((SegmentRequest) {
		.start = 0,
		.end = 10,
		.videoBuffer = &videoResultC1, .videoFormat = &vidFormat,
		.audioBuffer = &audioResultC1, .audioFormat = &audFormat
	});

	

//	std::cout << "B1 Video-Size: " << resultB1->videoFrames.size() << " AudioSize: " << resultB1->audioFrames.size() << std::endl;
//
//	std::cout << "B2 Video-Size: " << resultB2->videoFrames.size() << " AudioSize: " << resultB2->audioFrames.size() << std::endl;

	std::thread write1([videoResult1, audioResult1]() {

		writeAudio(std::string("test_files/one/audio.pcm"), audioResult1);
		writeFrames(videoResult1, std::string("test_files/one/"));

	});
	std::thread write2([videoResult2, audioResult2]() {

		writeAudio(std::string("test_files/two/audio.pcm"), audioResult2);
		writeFrames(videoResult2, std::string("test_files/two/"));

	});
	std::thread write3([videoResult3, audioResult3]() {

		writeAudio(std::string("test_files/three/audio.pcm"), audioResult3);
		writeFrames(videoResult3, std::string("test_files/three/"));

	});
	std::thread write4([videoResult4, audioResult4]() {

		writeAudio(std::string("test_files/four/audio.pcm"), audioResult4);
		writeFrames(videoResult4, std::string("test_files/four/"));

	});
	std::thread write5([videoResult5, audioResult5]() {

		writeAudio(std::string("test_files/five/audio.pcm"), audioResult5);
		writeFrames(videoResult5, std::string("test_files/five/"));

	});
	std::thread write6([videoResult6, audioResult6]() {

		writeAudio(std::string("test_files/six/audio.pcm"), audioResult6);
		writeFrames(videoResult6, std::string("test_files/six/"));

	});
	std::thread write7([videoResultC1, audioResultC1]() {

		writeAudio(std::string("test_files/seven/audio.pcm"), audioResultC1);
		writeFrames(videoResultC1, std::string("test_files/seven/"));

	});

	write1.join();
	write2.join();
	write3.join();
	write4.join();
	write5.join();
	write6.join();
	write7.join();

}