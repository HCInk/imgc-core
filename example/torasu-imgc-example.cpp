#include <iostream>
#include <iomanip>
#include <memory>
#include <fstream>

#include <lodepng.h>

#include <torasu/torasu.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/property_names.hpp>
#include <torasu/std/EIcore_runner.hpp>
#include <torasu/std/Dstring.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Daudio_buffer.hpp>
#include <torasu/std/Dnum.hpp>
#include <torasu/std/Dfile.hpp>
#include <torasu/std/Rlocal_file.hpp>
#include <torasu/std/Rnet_file.hpp>
#include <torasu/std/Rmultiply.hpp>
#include <torasu/std/Rproperty.hpp>
#include <torasu/std/Rnum.hpp>
#include <torasu/std/Rstring.hpp>
#include <torasu/std/Rmix_pipelines.hpp>
#include <torasu/std/Rmod_rctx.hpp>
#include <torasu/std/Rrctx_value.hpp>
#include <torasu/std/Rdivide.hpp>
#include <torasu/std/Rfloor_mod.hpp>
#include <torasu/std/Radd.hpp>

#include <torasu/mod/imgc/pipeline_names.hpp>
#include <torasu/mod/imgc/Rimg_file.hpp>
#include <torasu/mod/imgc/Rmedia_file.hpp>
#include <torasu/mod/imgc/Rmedia_creator.hpp>
#include <torasu/mod/imgc/MediaDecoder.hpp>
#include <torasu/mod/imgc/MediaEncoder.hpp>
#include <torasu/mod/imgc/Ralign2d.hpp>
#include <torasu/mod/imgc/Rauto_align2d.hpp>
#include <torasu/mod/imgc/Rgain.hpp>
#include <torasu/mod/imgc/Dcropdata.hpp>
#include <torasu/mod/imgc/Rcropdata.hpp>
#include <torasu/mod/imgc/Rcropdata_combined.hpp>
#include <torasu/mod/imgc/Rgraphics.hpp>

#include "example_tools.hpp"

#ifdef IMGC_SDL_EXAMPLE
// Local header for sdl_example
namespace imgc::example_sdl {
int main(int argc, char** argv);
} // namespace imgc::example_sdl
#endif

using namespace imgc;

namespace imgc::examples {

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
	torasu::tstd::Rnet_file file("https://assets.gitlab-static.net/uploads/-/system/project/avatar/14033279/TorasuLogo2Color.png");

	Rimg_file tree(&file);


	auto* runner = new torasu::tstd::EIcore_runner();

	torasu::ExecutionInterface* ei = runner->createInterface();

	torasu::tools::RenderInstructionBuilder rib;

	torasu::tstd::Dbimg_FORMAT format(400, 400);

	auto handle = rib.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, &format);

	std::cout << "RENDER BEGIN" << std::endl;
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
	std::cout << "RENDER FIN" << std::endl;

	auto castedRes = handle.getFrom(result);

	torasu::ResultSegmentStatus rss = castedRes.getStatus();

	std::cout << "STATUS " << rss << std::endl;

	if (rss >= torasu::ResultSegmentStatus::ResultSegmentStatus_OK) {
		torasu::tstd::Dbimg* bimg = castedRes.getResult();


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

		std::cout << "ENCODE STAT " << error << std::endl;
	}




	delete result;

	delete ei;
	delete runner;
}


void writeFrames(torasu::tstd::Dbimg_sequence* sequence, std::string base_path) {
	extools::TaskPool saver(10);
	auto& frames = sequence->getFrames();
	int i = 0;
	for (auto& frame : frames) {
		std::string path = base_path + "file-" + std::to_string(i + 1) + ".png";
		torasu::tstd::Dbimg* image = frame.second;
		saver.enqueue([path, image]() {
			unsigned error = lodepng::encode(path,
											 image->getImageData(), image->getWidth(), image->getHeight());
			if (error) {
				std::cerr << "LODEPNG ERROR " << error << ": " << lodepng_error_text(error) << " - while writing " << path << std::endl;
			}
		});
		++i;
	}
}

void writeAudio(std::string path, torasu::tstd::Daudio_buffer* audioBuff) {
	std::ofstream out(path);
	size_t size = 4;
	uint8_t* l = audioBuff->getChannels()[0].data;
	uint8_t* r = audioBuff->getChannels()[1].data;

	for (size_t i = 0; i < audioBuff->getChannels()[0].dataSize/size; i++) {
		out.write(reinterpret_cast<const char*>(l), size);
		out.write(reinterpret_cast<const char*>(r), size);
		l += size;
		r += size;
	}
	out.close();
}


void videoTest() {
	imgc::MediaDecoder des2("/home/cedric/Downloads/143386147_Superstar W.mp4");
	// imgc::MediaDecoder des2("/home/cedric/Shadow Lady-71f_1JB3T1s.webm"); // Has issues
	// imgc::MediaDecoder des2("/home/cedric/Shadow Lady-71f_1JB3T1s.mkv"); // Has issues

	torasu::tstd::Dbimg_sequence* videoBuffer;

	des2.getSegment((SegmentRequest) {
		.start = 0,
		.end = 180,
		.videoBuffer = &videoBuffer,
		.audioBuffer = NULL
	});

	writeFrames(videoBuffer, "test-res/extract-");

	delete videoBuffer;
}

void audioTest() {

	// imgc::MediaDecoder des2("/home/liz3/Downloads/smptstps.mp3");
	// imgc::MediaDecoder des2("/home/cedric/smptstps.mp3");
	imgc::MediaDecoder des2("/home/cedric/Hold the line-WlHHS6nq1w4.mp3");
	// imgc::MediaDecoder des2("/home/cedric/Hold the line-WlHHS6nq1w4.opus");
	// imgc::MediaDecoder des2("/home/cedric/Shadow Lady-71f_1JB3T1s.webm"); // Has issues
	// imgc::MediaDecoder des2("/home/cedric/Shadow Lady-71f_1JB3T1s.mkv"); // Has issues

	torasu::tstd::Daudio_buffer* audioBuffer;

	des2.getSegment((SegmentRequest) {
		.start = 0,
		.end = 180,
		.videoBuffer = NULL,
		.audioBuffer = &audioBuffer
	});

	writeAudio(std::string("test_files/audio_test.pcm"), audioBuffer);

	delete audioBuffer;

}


void yetAnotherIMGCTest() {

	// Tree building

	torasu::tstd::Rnet_file file("https://cdn.discordapp.com/attachments/598323767202152458/666010465809465410/8807502_Bender_and_penguins.mp4");
	torasu::tstd::Rnet_file file2("https://assets.gitlab-static.net/uploads/-/system/project/avatar/14033279/TorasuLogo2Color.png");
	// Rlocal_file file("/home/cedric/git/imgc/test-res/in.mp4");

	imgc::Rmedia_file video(&file);
	imgc::Rimg_file image(&file2);
	imgc::Rauto_align2d align(&image, 0, 0, 1);
	torasu::tstd::Rnum gainVal(10);

	torasu::tstd::Rmultiply mul(&video, &align);
	imgc::Rgain tree(&mul, &gainVal);

	// Creating Engine

	torasu::tstd::EIcore_runner* runner = new torasu::tstd::EIcore_runner();
	torasu::ExecutionInterface* ei = runner->createInterface();

	// Some Properties

	torasu::RenderableProperties* props = torasu::tools::getProperties(&video,
	{TORASU_STD_PROP_DURATION, TORASU_STD_PROP_IMG_WIDTH, TORASU_STD_PROP_IMG_HEIGHT, TORASU_STD_PROP_IMG_RAITO},
	ei);

	auto* dataDuration = torasu::tools::getPropertyValue<torasu::tstd::Dnum>(props, TORASU_STD_PROP_DURATION);
	double videoDuration = dataDuration ? dataDuration->getNum() : 0;
	auto* dataWidth = torasu::tools::getPropertyValue<torasu::tstd::Dnum>(props, TORASU_STD_PROP_IMG_WIDTH);
	double videoWidth = dataWidth ? dataWidth->getNum() : 0;
	auto* dataHeight = torasu::tools::getPropertyValue<torasu::tstd::Dnum>(props, TORASU_STD_PROP_IMG_HEIGHT);
	double videoHeight = dataHeight ? dataHeight->getNum() : 0;
	auto* dataRatio = torasu::tools::getPropertyValue<torasu::tstd::Dnum>(props, TORASU_STD_PROP_IMG_RAITO);
	double videoRatio = dataRatio ? dataRatio->getNum() : 0;
	delete props;
	std::cout << "VID DUR " << videoDuration << " SIZE " << videoWidth << "x" << videoHeight << " (" << videoRatio << ")"<< std::endl;

	// Building Instruction

	torasu::tools::RenderInstructionBuilder rib;
	// Dbimg_FORMAT format(1340, 1200);
	torasu::tstd::Dbimg_FORMAT format(1340, 1200);
	auto handle = rib.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, &format);

	// Rendering Results

	// double frameTimes[] = {0.1, 2, 4, 2.5, 5.1, 5.1, 5.14, 5.18, 5.22};
	// int frameCount = sizeof(frameTimes) / sizeof(double);
	double fps = 25;
	int frameCount = fps*videoDuration;
	double frameTimes[frameCount];


	for (int i = 0; i < frameCount; i++) {
		frameTimes[i] = (double) i/fps;
	}

	extools::TaskPool saver(10);

	std::cout << frameCount << " FRAMES TO RENDER!" << std::endl;

	for (int i = 0; i < frameCount; i++) {

		double frameTime = frameTimes[i];

		std::cout << "===== FRAME " << i << " @ " << frameTime << " =====" << std::endl;

		auto benchBegin = std::chrono::steady_clock::now();

		// Creating RenderContext
		torasu::tstd::Dnum timeBuf(frameTime);
		torasu::RenderContext rctx;
		rctx[TORASU_STD_CTX_TIME] = &timeBuf;

		// Render Result
		auto result = rib.runRender(&tree, &rctx, ei);

		// Evaluate Result

		auto castedRes = handle.getFrom(result);
		torasu::ResultSegmentStatus rss = castedRes.getStatus();


		auto benchEnd = std::chrono::steady_clock::now();
		std::cout << "  Render Time = " << std::chrono::duration_cast<std::chrono::milliseconds>(benchEnd - benchBegin).count() << "[ms]" << std::endl;

		std::cout << "STATUS " << rss << std::endl;

		if (rss >= torasu::ResultSegmentStatus::ResultSegmentStatus_OK) {
			torasu::tstd::Dbimg* bimg = castedRes.getResult();

			std::stringstream out_name;
			out_name << "test-res/out";
			out_name << std::setfill('0') << std::setw(5) << i;
			out_name << ".png";
			std::string name = out_name.str();


			// unsigned error = lodepng::encode(name, bimg->getImageData(), bimg->getWidth(), bimg->getHeight());
			// if (error) {
			// 	cerr << "ENCODE STAT[" << name << "] " << lodepng_error_text(error) << endl;
			// }
			// delete result;
			saver.enqueue([result, bimg, name]() {
				unsigned error = lodepng::encode(name, bimg->getImageData(), bimg->getWidth(), bimg->getHeight());
				if (error) {
					std::cerr << "ENCODE STAT[" << name << "] " << lodepng_error_text(error) << std::endl;
				}

				delete result;
			});

		} else {
			delete result;
		}

	}

	saver.sync();

	// Cleanup

	delete ei;
	delete runner;

}

void cropdataExample() {

	//
	// Cropdata Example
	//

	std::cout << "//" << std::endl
			  << "// Cropdata Example" << std::endl
			  << "//" << std::endl;

	// Creating "tree" to be rendered

	// imgc::Dcropdata cropdata(0.2, 0.1, 0.3, 0.4);

	// imgc::Rcropdata tree(cropdata);

	torasu::tstd::Rnet_file file("https://gitlab.com/HCInk/torasu/torasu-docs/-/raw/master/logo/TorasuLogo2TextBannerColor.png");
	imgc::Rimg_file image(&file);

	torasu::tstd::Rproperty ratio(&image, TORASU_STD_PROP_IMG_RAITO, TORASU_STD_PL_NUM);
	torasu::tstd::Rnum left(0.2);
	torasu::tstd::Rmultiply calcLeft(&left, &ratio);
	torasu::tstd::Rnum right(0.1);
	torasu::tstd::Rnum top(0.3);
	torasu::tstd::Rnum bottom(0.4);

	imgc::Rcropdata_combined tree(&calcLeft, &right, &top, &bottom);

	// Creating the runner

	torasu::tstd::EIcore_runner runner;

	torasu::ExecutionInterface* ei = runner.createInterface();

	// Creating instruction

	torasu::tools::RenderInstructionBuilder rib;

	auto handle = rib.addSegmentWithHandle<imgc::Dcropdata>(IMGC_PL_ALIGN, NULL);

	// Running render based on instruction

	torasu::RenderContext rctx;

	torasu::RenderResult* rr = rib.runRender(&tree, &rctx, ei);

	// Finding results

	auto result = handle.getFrom(rr);
	std::cout << "CROPDATA : " << result.getResult()->getSerializedJson() << std::endl;

	// Cleaning

	delete rr;
	delete ei;
}


void cropExample() {

	//
	// Crop Example
	//

	std::cout << "//" << std::endl
			  << "// Crop Example" << std::endl
			  << "//" << std::endl;

	// Creating "tree" to be rendered

	torasu::tstd::Rnet_file file("https://gitlab.com/HCInk/torasu/torasu-docs/-/raw/master/logo/TorasuLogo2Color.png");
	// torasu::tstd::Rnet_file file("https://gitlab.com/HCInk/torasu/torasu-docs/-/raw/master/logo/TorasuLogo2TextBannerColor.png");
	Rimg_file image(&file);

	// imgc::Rcropdata cropdata(imgc::Dcropdata(0.1, -0.3, 0.3, -0.3));

	imgc::Rauto_align2d tree(&image, 0, 0, 0);

	// Creating the runner

	torasu::tstd::EIcore_runner runner;

	torasu::ExecutionInterface* ei = runner.createInterface();

	// Creating instruction

	torasu::tools::RenderInstructionBuilder rib;

	// torasu::tstd::Dbimg_FORMAT format(500, 500); // Works
	// torasu::tstd::Dbimg_FORMAT format(500, 600); // Now Works
	torasu::tstd::Dbimg_FORMAT format(600, 500); // Now Works

	auto handle = rib.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, &format);

	// Creating render-context

	torasu::RenderContext rctx;
	torasu::tstd::Dnum ratio((double) format.getWidth() / format.getHeight());
	rctx[TORASU_STD_CTX_IMG_RATIO] = &ratio;

	// Running render based on instruction

	torasu::RenderResult* rr = rib.runRender(&tree, &rctx, ei);

	// Finding results

	auto result = handle.getFrom(rr);
	std::cout << "RESULT STAT " << result.getStatus() << std::endl;
	auto* bimg = result.getResult();
	unsigned error = lodepng::encode("test-res/out.png", bimg->getImageData(), bimg->getWidth(), bimg->getHeight());
	if (error) {
		std::cerr << "ENCODE ERROR " << error << ": " << lodepng_error_text(error) << std::endl;
	} else {
		std::cout << "ENCODE OK" << std::endl;
	}

	// Cleaning

	delete rr;
	delete ei;

}

torasu::tstd::Dbimg* makeBimg(double time, torasu::tstd::Dbimg_FORMAT* format) {
	auto* bimg = new torasu::tstd::Dbimg(*format);
	uint32_t width = format->getWidth();
	uint32_t height = format->getHeight();
	uint8_t* data = bimg->getImageData();

	for (uint32_t y = 0; y < height; y++) {
		// uint8_t val = (sin(time*4+((double)y)/100)*0.5+0.5)*0xFF;
		// uint8_t val = time*4+((double)y)/100;
		uint8_t val = (((int)(time*4+((double)y)/100))%2)*0xFF;
		for (uint32_t x = 0; x < width; x++) {
			*data = x;
			data++;
			*data = y;
			data++;
			*data = val;
			data++;
			*data = 0xFF;
			data++;
		}
	}

	return bimg;
}

torasu::tstd::Daudio_buffer* makeAudioSeq(double time, double duration, torasu::tstd::Daudio_buffer_FORMAT* format) {
	auto sampleRate = format->getBitrate();
	int samples = sampleRate*duration;
	auto* audBuff = new torasu::tstd::Daudio_buffer(2, sampleRate, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32, 4, 4*samples);

	auto* channels = audBuff->getChannels();

	int playhead = round(time*sampleRate);
	for (int i = 0; i < samples; i++) {
		float sampleNo = playhead+i;
		float freq = sin(sampleNo / 44100 / 1)*1000+1000 + 100;
		float val = sin(sampleNo * freq / 44100)/2;

		uint32_t d = *(reinterpret_cast<uint32_t*>(&val));
		for (int c = 0; c < 2; c++) {
			for (int b = 0; b < 4; b++) {
				channels[c].data[i*4+b] = d >> b*8 & 0xFF;
			}
		}

	}
	return audBuff;
}


void encodeExample() {

	imgc::MediaEncoder enc([](imgc::MediaEncoder::FrameRequest* fr) {
		if (auto* videoFr = dynamic_cast<imgc::MediaEncoder::VideoFrameRequest*>(fr)) {
			std::cout << "VFR " << videoFr->getTime() << std::endl;
			auto* bimg = imgc::examples::makeBimg(videoFr->getTime(), videoFr->getFormat());
			videoFr->setResult(bimg);
			videoFr->setFree([bimg]() {
				delete bimg;
			});
			return 0;
		} else if (auto* audioFr = dynamic_cast<imgc::MediaEncoder::AudioFrameRequest*>(fr)) {
			auto* audioSeq = imgc::examples::makeAudioSeq(audioFr->getStart(), audioFr->getDuration(), audioFr->getFormat());
			std::cout << "AFR " << audioFr->getStart() << std::endl;
			audioFr->setResult(audioSeq);
			audioFr->setFree([audioSeq]() {
				delete audioSeq;
			});
			return 0;
		}
		return -3;
	});

	imgc::MediaEncoder::EncodeRequest req;
	req.formatName = "mp4";
	req.end = 10;

	req.doVideo = true;
	req.width = 1920;
	req.height = 1080;
	req.framerate = 60;
	// req.videoBitrate = 224850 * 1000;

	req.doAudio = true;
	req.minSampleRate = 44100;
	// req.audioBitrate = 40 * 1000;

	auto* file = enc.encode(req);

	std::cout << "Saving..." << std::endl;
	std::ofstream sysFile("test.mp4");

	sysFile.write(const_cast<const char*>(reinterpret_cast<char*>(file->getFileData())), file->getFileSize());

	sysFile.close();

	std::cout << "Saved." << std::endl;

	delete file;

}

void encodeTorasu() {
	// This is a constant, still looking for a better alternative compared to this here
	torasu::tstd::Rnum one(1);

	torasu::tstd::Rnet_file videoFile1("https://cdn.discordapp.com/attachments/598323767202152458/666010465809465410/8807502_Bender_and_penguins.mp4");
	torasu::tstd::Rnet_file videoFile2("https://cdn.discordapp.com/attachments/770711233065517106/770715070622990366/155216075_Bear_Freestyler_in_Rust.mp4");

	imgc::Rmedia_file video1(&videoFile1);
	imgc::Rmedia_file video2(&videoFile2);
	
	// Remap time to speed it up and then loop it from the 2nd to 4th second
	torasu::tstd::Rnum speed(2);
	torasu::tstd::Rnum loop(2);
	torasu::tstd::Rnum offset(2);
	torasu::tstd::Rrctx_value time(TORASU_STD_CTX_TIME, TORASU_STD_PL_NUM);
	torasu::tstd::Rmultiply timeRemapping1(&time, &speed);
	torasu::tstd::Rfloor_mod timeRemapping2(&timeRemapping1, &loop);
	torasu::tstd::Radd timeRemapping3(&timeRemapping2, &offset);
	torasu::tstd::Rmod_rctx speedVid(&video1, &timeRemapping3, TORASU_STD_CTX_TIME, TORASU_STD_PL_NUM);

	// Take video1 and replace its audio with the audio of video2
	torasu::tstd::Rmix_pipelines comp(&speedVid, {{TORASU_STD_PL_AUDIO, &video2}});

	torasu::tstd::Rstring format("mp4");
	torasu::tstd::Rnum begin(0);
	torasu::tstd::Rnum fps(25);
	torasu::tstd::Rnum videoBitrate(4000*1000);
	torasu::tstd::Rnum audioMinSamplerate(44100);

	torasu::tstd::Rproperty duration(&video1, TORASU_STD_PROP_DURATION, TORASU_STD_PL_NUM);
	torasu::tstd::Rdivide durFac(&one, &speed);
	torasu::tstd::Rmultiply end(&duration, &durFac);
	torasu::tstd::Rproperty width(&video1, TORASU_STD_PROP_IMG_WIDTH, TORASU_STD_PL_NUM);
	torasu::tstd::Rproperty height(&video1, TORASU_STD_PROP_IMG_HEIGHT, TORASU_STD_PL_NUM);

	imgc::Rmedia_creator encoded(&comp, &format, &begin, &end, &fps, &width, &height, &videoBitrate, &audioMinSamplerate);

	auto* tree = &encoded;

	// Creating Engine

	torasu::tstd::EIcore_runner* runner = new torasu::tstd::EIcore_runner();
	torasu::ExecutionInterface* ei = runner->createInterface();

	torasu::tools::RenderInstructionBuilder rib;
	auto handle = rib.addSegmentWithHandle<torasu::tstd::Dfile>(TORASU_STD_PL_FILE, nullptr);

	torasu::RenderContext rctx;
	std::unique_ptr<torasu::RenderResult> rr(rib.runRender(tree, &rctx, ei));

	auto seg = handle.getFrom(rr.get());

	auto* resFile = seg.getResult();

	std::cout << "Saving..." << std::endl;
	std::ofstream sysFile("test.mp4");

	sysFile.write(const_cast<const char*>(reinterpret_cast<char*>(resFile->getFileData())), resFile->getFileSize());

	sysFile.close();

	std::cout << "Saved." << std::endl;

	delete ei;
	delete runner;

}

void graphicsExample() {
	Rgraphics tree;


	auto* runner = new torasu::tstd::EIcore_runner();

	torasu::ExecutionInterface* ei = runner->createInterface();

	torasu::tools::RenderInstructionBuilder rib;

	torasu::tstd::Dbimg_FORMAT format(/* 400 */1080*2, /* 300 */1080*2);
	// torasu::tstd::Dbimg_FORMAT format(30*3, 30*3);

	auto handle = rib.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, &format);

	std::cout << "RENDER BEGIN" << std::endl;

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	torasu::RenderContext rctx;
	auto result = rib.runRender(&tree, &rctx, ei);

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
	std::cout << "RENDER FIN" << std::endl;

	auto castedRes = handle.getFrom(result);

	torasu::ResultSegmentStatus rss = castedRes.getStatus();

	std::cout << "STATUS " << rss << std::endl;

	if (rss >= torasu::ResultSegmentStatus::ResultSegmentStatus_OK) {
		torasu::tstd::Dbimg* bimg = castedRes.getResult();

		unsigned error = lodepng::encode("out.png", bimg->getImageData(), bimg->getWidth(), bimg->getHeight());

		std::cout << "ENCODE STAT " << error << std::endl;
	}




	delete result;

	delete ei;
	delete runner;
}

}  // namespace imgc::examples

int main(int argc, char** argv) {
	//netImageTest();
	//avTest();
	// anotherIMGCTest();
	// audioTest();
	// videoTest();
	// example_sdl::main(argc, argv);
	// examples::yetAnotherIMGCTest();
	// examples::cropdataExample();
	// examples::cropExample();
	// examples::encodeExample();
	// examples::encodeTorasu();
	examples::graphicsExample();

	return 0;
}