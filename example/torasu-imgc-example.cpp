#include <iostream>

#include <lodepng.h>

#include <torasu/torasu.hpp>
#include <torasu/tools.hpp>
#include <torasu/std/pipelines.hpp>
#include <torasu/std/EICoreRunner.hpp>
#include <torasu/std/DPString.hpp>
#include <torasu/std/DRBImg.hpp>

#include <torasu/mod/imgc/RImgFile.hpp>

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

int main() {

	DPString* path = new DPString("test-res/in.png");

	RImgFile tree;

	tree.setData(path);

	EICoreRunner* runner = new EICoreRunner();

	ExecutionInterface* ei = runner->createInterface();

	tools::RenderInstructionBuilder rib;

	DRBImg_FORMAT format(400, 400);

	auto rf = format.asFormat();

	auto handle = rib.addSegmentWithHandle<DRBImg>(TORASU_STD_PL_VIS, &rf);

	cout << "RENDER BEGIN" << endl;
	for (int i = 0; i < 120; i++) {
		std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
		auto result = rib.runRender(&tree, NULL, ei);
		cout << "NEXT FRAME " << i << endl;
		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
		delete result;
	}
	auto result = rib.runRender(&tree, NULL, ei);
	cout << "RENDER FIN" << endl;

	auto castedRes = handle.getFrom(result);

	ResultSegmentStatus rss = castedRes.getStatus();

	cout << "STATUS " << rss << endl;

	if (rss >= ResultSegmentStatus::ResultSegmentStatus_OK) {
		DRBImg* bimg = castedRes.getResult();

		uint8_t* data = bimg->getImageData();
		int width = bimg->getWidth();
		int height = bimg->getHeight();
		int channels = 4;
		/*int i = 0;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {

				u_int8_t red = data[i];
				u_int8_t green = data[i+1];
				u_int8_t blue = data[i+2];
				
				const char* redCodes[] = {"\33[31m|", "\33[31;1m|", "\33[30m\33[101m|", "\33[31;1m\33[101m|"};
				const char* greenCodes[] = {"\33[32m|", "\33[32;1m|", "\33[30m\33[102m|", "\33[32;1m\33[102m|"};
				const char* blueCodes[] = {"\33[34m|", "\33[34;1m|", "\33[30m\33[104m|", "\33[34;1m\33[104m|"};
				
				cout << printCode(red, redCodes) << "\33[0m";
				cout << printCode(green, greenCodes) << "\33[0m";
				cout << printCode(blue, blueCodes)  << "\33[0m";

				i+=channels;
			}
			cout << endl;
		}*/

		std::vector<uint8_t> image(data, data+(width*height*channels));
		unsigned error = lodepng::encode("test-res/out.png", image, width, height);

		cout << "ENCODE STAT " << error;
	}




	delete result;

	delete ei;
	delete runner;

	return 0;
}