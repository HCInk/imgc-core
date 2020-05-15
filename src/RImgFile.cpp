#include "../include/torasu/mod/imgc/RImgFile.hpp"

#include <iostream>
#include <cmath>
#include <lodepng.h>

#include <torasu/torasu.hpp>
#include <torasu/std/pipelines.hpp>
#include <torasu/std/DPString.hpp>
#include <torasu/std/DRBImg.hpp>

using namespace std;
using namespace torasu;
using namespace torasu::tstd;

namespace imgc {

RImgFile::RImgFile() : SimpleRenderable("STD::RIMGFILE", true) {
	data = NULL;
}

RImgFile::~RImgFile() {
	if (data != NULL) delete data;
}

ResultSegment* RImgFile::renderSegment(ResultSegmentSettings* resSettings, RenderInstruction* ri) {
	if (resSettings->getPipeline().compare(TORASU_STD_PL_VIS) == 0) {
		auto format = resSettings->getResultFormatSettings();
		if (format == NULL || format->getFormat().compare("STD::DRBIMG") == 0) {
			int32_t rWidth, rHeight;
			if (format == NULL) {
				rWidth = -1;
				rHeight = -1;
			} else {
				DataResource* fmtData = format->getData();
				DRBImg_FORMAT* bimgFormat;
				if (!(bimgFormat = dynamic_cast<DRBImg_FORMAT*>(fmtData))) {
					return new ResultSegment(ResultSegmentStatus_INVALID_FORMAT);
				}
				rWidth = bimgFormat->getWidth();
				rHeight = bimgFormat->getHeight();
				cout << "RIMG RENDER " << rWidth << "x" << rHeight << endl;
			}

			vector<uint8_t> image;
			uint32_t srcWidth, srcHeight;
			string filename = this->data->getString();
			cout << "LOADING IMAGE FROM " << filename << endl;
			uint32_t error = lodepng::decode(image, srcWidth, srcHeight, filename.c_str());

			cout << "DECODE STATUS " << error << endl;
			if (error) {
				return new ResultSegment(ResultSegmentStatus_INTERNAL_ERROR);
			}

			if (rWidth == 0 || rHeight == 0) {
				return new ResultSegment(ResultSegmentStatus_OK, new DRBImg(rWidth, rHeight), true);
			}

			if (rWidth < 0) {
				rWidth = srcWidth;
				rHeight = srcHeight;
			}

			DRBImg* bimg = new DRBImg(rWidth, rHeight);
			uint8_t* data = bimg->getImageData();
			uint8_t* imgaddr = &image[0];

			if (rWidth == ((int32_t)srcWidth) && rHeight == ((int32_t)srcHeight)) {
				cout << "COPY SIZE " << image.size() << "BUFF SIZE" << bimg->getBufferSize() << endl;
				int endOffset = image.size()-1;
				copy(imgaddr, imgaddr+endOffset, data);
			} else {
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
						addrA = imgaddr + ( ((uint32_t)posX) + ((uint32_t)posY)*srcWidth ) * channels;
						addrB = addrA + channels;
						addrC = addrA + widthB;
						addrD = addrC + channels;

						fX = posX-((int32_t)posX);
						fXi = 1-fX;
						fY = posY-((int32_t)posY);
						fYi = 1-fY;

						for (int32_t c = 0; c < channels; c++) {
							data[i] =  ( (*addrA)*fXi + (*addrB)*fX ) * fYi
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

			return new ResultSegment(ResultSegmentStatus_OK, bimg, true);

		} else {
			return new ResultSegment(ResultSegmentStatus_INVALID_FORMAT);
		}

	} else {
		return new ResultSegment(ResultSegmentStatus_INVALID_SEGMENT);
	}
}

DataResource* RImgFile::getData() {
	return data;
}

void RImgFile::setData(DataResource* data) {
	if (DPString* dpstr = dynamic_cast<DPString*>(data)) {
		if (this->data != NULL) delete this->data;
		this->data = dpstr;
	} else {
		throw invalid_argument("The data-type \"DPString\" is only allowed");
	}
}

}
