#include <iostream>
#include <iomanip>

#include <lodepng.h>
#include <torasu/torasu.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/EIcore_runner.hpp>
#include <torasu/std/Dstring.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>
#include <torasu/std/Dfile.hpp>
#include <torasu/std/Rlocal_file.hpp>
#include <torasu/std/Rnet_file.hpp>

#include <torasu/mod/imgc/Rimg_file.hpp>
#include <torasu/mod/imgc/Rvideo_file.hpp>
#include <torasu/mod/imgc/VideoFileDeserializer.hpp>


using namespace std;
using namespace torasu;
using namespace torasu::tstd;
using namespace imgc;

inline const char* printCode(u_int8_t value, const char** codeSet) {

	if (value > 0xf2) {
		return codeSet[3];
	} else if (value > 0xd0) {
		return codeSet[2];
	} else if (value > 0x80) {
		return codeSet[1];
	} else if (value > 0x40) {
		return codeSet[0];
	} else {
		return " ";
	}
}

void netImageTest() {
	Rnet_file file("https://assets.gitlab-static.net/uploads/-/system/project/avatar/14033279/TorasuLogo2Color.png");

	Rimg_file tree(&file);


	EIcore_runner* runner = new EIcore_runner();

	ExecutionInterface* ei = runner->createInterface();

	tools::RenderInstructionBuilder rib;

	Dbimg_FORMAT format(400, 400);

	auto rf = format.asFormat();

	auto handle = rib.addSegmentWithHandle<Dbimg>(TORASU_STD_PL_VIS, &rf);

	cout << "RENDER BEGIN" << endl;
	/*for (int i = 0; i < 120; i++) {
		std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
		auto result = rib.runRender(&tree, NULL, ei);
		cout << "NEXT FRAME " << i << endl;
		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
		delete result;
	}*/

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	auto result = rib.runRender(&tree, NULL, ei);

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
	cout << "RENDER FIN" << endl;

	auto castedRes = handle.getFrom(result);

	ResultSegmentStatus rss = castedRes.getStatus();

	cout << "STATUS " << rss << endl;

	if (rss >= ResultSegmentStatus::ResultSegmentStatus_OK) {
		Dbimg* bimg = castedRes.getResult();


		int width = bimg->getWidth();
		int height = bimg->getHeight();
		/*int channels = 4;
		uint8_t* data = &(*bimg->getImageData())[0];


		const char* redCodes[] = {"\33[31m|", "\33[31;1m|", "\33[30m\33[101m|", "\33[31;1m\33[101m|"};
		const char* greenCodes[] = {"\33[32m|", "\33[32;1m|", "\33[30m\33[102m|", "\33[32;1m\33[102m|"};
		const char* blueCodes[] = {"\33[34m|", "\33[34;1m|", "\33[30m\33[104m|", "\33[34;1m\33[104m|"};

		int i = 0;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {

				u_int8_t red = data[i];
				u_int8_t green = data[i+1];
				u_int8_t blue = data[i+2];

				cout << printCode(red, redCodes) << "\33[0m";
				cout << printCode(green, greenCodes) << "\33[0m";
				cout << printCode(blue, blueCodes)  << "\33[0m";

				i+=channels;
			}
			cout << endl;
		}*/

		unsigned error = lodepng::encode("test-res/out.png", bimg->getImageData(), width, height);

		cout << "ENCODE STAT " << error;
	}




	delete result;

	delete ei;
	delete runner;
}

void avTest() {
	cout << "ello avTest()" << endl;

	EIcore_runner* runner = new EIcore_runner();
	ExecutionInterface* ei = runner->createInterface();

	//Rlocal_file file("test-res/in.mp4");

	Rnet_file file("https://cdn.discordapp.com/attachments/598323767202152458/666010465809465410/8807502_Bender_and_penguins.mp4");
	imgc::VideoLoader tree(&file);

	tools::RenderInstructionBuilder rib;

	Dbimg_FORMAT format(1340, 1200);
	//Dbimg_FORMAT format(1600, 812);

	auto rf = format.asFormat();

	auto handle = rib.addSegmentWithHandle<Dbimg>(TORASU_STD_PL_VIS, &rf);
	std::chrono::steady_clock::time_point benchStep, benchEnd, benchBegin;
	for (int i = 0; i < 100; i++) {

		cout << "RENDER BEGIN" << endl;

		benchBegin = std::chrono::steady_clock::now();
		benchStep = std::chrono::steady_clock::now();

		Dnum timeBuf((double)i/25);

		RenderContext rctx;
		rctx[TORASU_STD_CTX_TIME] = &timeBuf;

		auto result = rib.runRender(&tree, &rctx, ei);

		benchEnd = std::chrono::steady_clock::now();
		std::cout << "  Render Time = " << std::chrono::duration_cast<std::chrono::milliseconds>(benchEnd - benchStep).count() << "[ms]" << std::endl;

		benchStep = std::chrono::steady_clock::now();

		auto castedRes = handle.getFrom(result);

		ResultSegmentStatus rss = castedRes.getStatus();

		cout << "STATUS " << rss << endl;

		if (rss >= ResultSegmentStatus::ResultSegmentStatus_OK) {
			// Dbimg* bimg = castedRes.getResult();
			// int width = bimg->getWidth();
			// int height = bimg->getHeight();

			// stringstream out_name;
			// out_name << "test-res/out";
			// out_name << std::setfill('0') << std::setw(5) << i;
			// out_name << ".png";

			// uint8_t* imgData = bimg->getImageData();
			// size_t imgSize = bimg->getHeight()*bimg->getWidth()*4;

			// vector<uint8_t> lpngVec(imgSize);

			// unsigned error = lodepng::encode(out_name.str(), imgData, width, height);
			// cerr << "ENCODE STAT[" << i << "] " << error << endl;
			//delete[] t;
			//delete[] z;
		}

		benchEnd = std::chrono::steady_clock::now();
		std::cout << "  Unpack Time = " << std::chrono::duration_cast<std::chrono::milliseconds>(benchEnd - benchStep).count() << "[ms]" << std::endl;

		benchStep = std::chrono::steady_clock::now();

		delete result;

		benchEnd = std::chrono::steady_clock::now();
		std::cout << "  Delete Time = " << std::chrono::duration_cast<std::chrono::milliseconds>(benchEnd - benchStep).count() << "[ms]" << std::endl;

		std::cout << "Total Time = " << std::chrono::duration_cast<std::chrono::milliseconds>(benchEnd - benchBegin).count() << "[ms]" << std::endl;

		cout << "RENDER FIN" << endl;
	}

	// tree.load(ei);
	// tree.video_decode_example();
	//tree.debugPackets();

	delete ei;
	delete runner;
}


void anotherIMGCTest() {

	// Tree building

	Rnet_file file("https://cdn.discordapp.com/attachments/598323767202152458/666010465809465410/8807502_Bender_and_penguins.mp4");
	// Rnet_file file("https://cdn.discordapp.com/attachments/598323767202152458/666010465809465410/8807502_Bender_and_penguins.mp4");
	// Rlocal_file file("/home/cedric/git/imgc/test-res/in.mp4");
	imgc::VideoLoader tree(&file);

	// Creating Engine

	EIcore_runner* runner = new EIcore_runner();
	ExecutionInterface* ei = runner->createInterface();

	// Building Instruction

	tools::RenderInstructionBuilder rib;
	Dbimg_FORMAT format(1340, 1200);
	auto rf = format.asFormat();
	auto handle = rib.addSegmentWithHandle<Dbimg>(TORASU_STD_PL_VIS, &rf);

	// Rendering Results

	double frameTimes[] = {0.1, 2, 4, 2.5, 5.1};
	int frameCount = sizeof(frameTimes) / sizeof(double);

	cout << frameCount << " FRAMES TO RENDER!" << endl;

	for (int i = 0; i < frameCount; i++) {

		double frameTime = frameTimes[i];

		cout << "===== FRAME " << i << " @ " << frameTime << " =====" << endl;

		// Creating RenderContext
		Dnum timeBuf(frameTime);
		RenderContext rctx;
		rctx[TORASU_STD_CTX_TIME] = &timeBuf;

		// Render Result
		auto result = rib.runRender(&tree, &rctx, ei);

		// Evaluate Result

		auto castedRes = handle.getFrom(result);
		ResultSegmentStatus rss = castedRes.getStatus();

		cout << "STATUS " << rss << endl;

		if (rss >= ResultSegmentStatus::ResultSegmentStatus_OK) {
			Dbimg* bimg = castedRes.getResult();
			int width = bimg->getWidth();
			int height = bimg->getHeight();

			stringstream out_name;
			out_name << "test-res/out";
			out_name << std::setfill('0') << std::setw(5) << i;
			out_name << ".png";

			unsigned error = lodepng::encode(out_name.str(), bimg->getImageData(), width, height);
			if (error) {
				cerr << "ENCODE STAT[" << i << "] " << lodepng_error_text(error) << endl;
			}
		}

		delete result;
	}


	// Cleanup

	delete ei;
	delete runner;

}

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

void audioTest() {

	// VideoFileDeserializer des2("/home/liz3/Downloads/smptstps.mp3");
	// VideoFileDeserializer des2("/home/cedric/smptstps.mp3");
	VideoFileDeserializer des2("/home/cedric/Hold the line-WlHHS6nq1w4.mp3");

	torasu::tstd::Daudio_buffer_FORMAT audioFmt(44100, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32);
	torasu::tstd::Daudio_buffer* audioBuffer;

	auto result = des2.getSegment((SegmentRequest) {
		.start = 0,
		.end = 180,
		.videoBuffer = NULL,
		.videoFormat = NULL,
		.audioBuffer = &audioBuffer,
		.audioFormat = &audioFmt
	});

	std::cout << "C1 Video-Size: " << result->vidFrames.size() << " AudioSize: " << result->audFrames.size() << std::endl;

	writeAudio(std::string("test_files/mp3test.pcm"), audioBuffer);

}

int main() {
	//netImageTest();
	//avTest();
	//anotherIMGCTest();
	audioTest();

	return 0;
}