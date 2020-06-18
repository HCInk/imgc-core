#include "../include/torasu/mod/imgc/Rimg_file.hpp"

#include <iostream>
#include <cmath>

#include <lodepng.h>


#include <torasu/torasu.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dstring.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dfile.hpp>

using namespace std;
using namespace torasu;
using namespace torasu::tstd;

namespace imgc {

Rimg_file::Rimg_file(Renderable* file)
	: SimpleRenderable("STD::RIMG_FILE", false, true),
	  resHandle(rib.addSegmentWithHandle<Dfile>(TORASU_STD_PL_FILE, NULL)) {
	this->rfile = file;
}

Rimg_file::~Rimg_file() {}

ResultSegment* Rimg_file::renderSegment(ResultSegmentSettings* resSettings, RenderInstruction* ri) {
	if (resSettings->getPipeline().compare(TORASU_STD_PL_VIS) == 0) {
		auto format = resSettings->getResultFormatSettings();
		if (format == NULL || format->getFormat().compare("STD::DBIMG") == 0) {
			int32_t rWidth, rHeight;
			if (format == NULL) {
				rWidth = -1;
				rHeight = -1;
			} else {
				DataResource* fmtData = format->getData();
				Dbimg_FORMAT* bimgFormat;
				if (!(bimgFormat = dynamic_cast<Dbimg_FORMAT*>(fmtData))) {
					return new ResultSegment(ResultSegmentStatus_INVALID_FORMAT);
				}
				rWidth = bimgFormat->getWidth();
				rHeight = bimgFormat->getHeight();
				cout << "RIMG RENDER " << rWidth << "x" << rHeight << endl;
			}


			auto ei = ri->getExecutionInterface();
			auto rctx = ri->getRenderContext();

			RenderResult* fileRenderResult = rib.runRender(rfile, rctx, ei);

			auto fileRes = resHandle.getFrom(fileRenderResult);

			if (fileRes.getStatus() < ResultSegmentStatus_OK) {
				// Stop because of file-render-error
				return new ResultSegment(ResultSegmentStatus_INTERNAL_ERROR);
			}

			Dfile* file = fileRes.getResult();

			vector<uint8_t> loadedImage;
			uint32_t srcWidth, srcHeight;

			uint32_t error = lodepng::decode(loadedImage, srcWidth, srcHeight, file->getFileData(), file->getFileSize());

			delete fileRenderResult;

			cout << "DECODE STATUS " << error << endl;
			if (error) {
				// Stop because file-decoding error
				return new ResultSegment(ResultSegmentStatus_INTERNAL_ERROR);
			}

			if (rWidth == 0 || rHeight == 0) {
				return new ResultSegment(ResultSegmentStatus_OK, new Dbimg(rWidth, rHeight), true);
			}

			if (rWidth < 0) {
				rWidth = srcWidth;
				rHeight = srcHeight;
			}

			Dbimg* resultImage;

			if (rWidth == ((int32_t)srcWidth) && rHeight == ((int32_t)srcHeight)) {
				resultImage = new Dbimg(rWidth, rHeight);
				copy(loadedImage.data(), loadedImage.data()+loadedImage.size(), resultImage->getImageData());

			} else {

				resultImage = new Dbimg(rWidth, rHeight);
				uint8_t* resData = resultImage->getImageData();
				uint8_t* srcData = loadedImage.data();

				uint8_t channels = 4;
				uint64_t i = 0;

				uint32_t widthB = srcWidth*channels;

				float xFact = ((double)srcWidth)/rWidth;
				float yFact = ((double)srcHeight)/rHeight;

				float posX, posY;
				float fX, fXi, fY, fYi;

				uint8_t* addrA;
				uint8_t* addrB;
				uint8_t* addrC;
				uint8_t* addrD;

				for (int32_t y = 0; y < rHeight; y++) {
					for (int32_t x = 0; x < rWidth; x++) {
						posX = ((float)x)*xFact;
						posY = ((float)y)*yFact;

						//cout << (std::fmod(posX, 1)) << " ";
						// A B
						// C D
						addrA = srcData + ( ((uint32_t)posX) + ((uint32_t)posY)*srcWidth ) * channels;
						addrB = addrA + channels;
						addrC = addrA + widthB;
						addrD = addrC + channels;

						fX = posX-((int32_t)posX);
						fXi = 1-fX;
						fY = posY-((int32_t)posY);
						fYi = 1-fY;

						for (int32_t c = 0; c < channels; c++) {
							resData[i] =  ( (*addrA)*fXi + (*addrB)*fX ) * fYi
										  + ( (*addrC)*fXi + (*addrD)*fX ) * fY;

							addrA++;
							addrB++;
							addrC++;
							addrD++;
							i++;
						}
					}
					//cout << endl;
				}
			}

			return new ResultSegment(ResultSegmentStatus_OK, resultImage, true);

		} else {
			return new ResultSegment(ResultSegmentStatus_INVALID_FORMAT);
		}

	} else {
		return new ResultSegment(ResultSegmentStatus_INVALID_SEGMENT);
	}
}

std::map<std::string, Element*> Rimg_file::getElements() {
	std::map<std::string, Element*> elems;
	elems["f"] = rfile;
	return elems;
}

void Rimg_file::setElement(std::string key, Element* elem) {
	if (key.compare("f") != 0) {
		throw invalid_argument("Invalid slot-key! Only slot-key \"f\" allowed");
	}
	if (elem == NULL) {
		throw invalid_argument("Element for slot \"f\" may not be NULL!");
	}

	if (Renderable* rnd = dynamic_cast<Renderable*>(elem)) {
		this->rfile = rnd;
	} else {
		throw invalid_argument("Only \"Renderable\" for slot \"f\" allowed");
	}
}

}
