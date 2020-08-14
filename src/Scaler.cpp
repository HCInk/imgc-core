#include "../include/torasu/mod/imgc/Scaler.hpp"

namespace imgc::scaler {

torasu::tstd::Dbimg* scaleImg(u_int8_t* srcData, uint32_t srcWidth, uint32_t srcHeight, torasu::tstd::Dbimg_FORMAT* fmt, bool copySame) {
	
	auto destWidth = fmt->getWidth();
	auto destHeight = fmt->getHeight();

	if (srcWidth == destWidth && srcHeight == destHeight) {
		if (copySame) {
			torasu::tstd::Dbimg* resultImage = new torasu::tstd::Dbimg(destWidth, destHeight);
			std::copy(srcData, srcData+(srcWidth*srcHeight*4), resultImage->getImageData());
			return resultImage;
		} else {
			return NULL;
		}
	}

	torasu::tstd::Dbimg* resultImage = new torasu::tstd::Dbimg(destWidth, destHeight);
	uint8_t* destData = resultImage->getImageData();

	uint8_t channels = 4;
	uint64_t i = 0;

	uint32_t widthB = srcWidth*channels;

	// Factor by what the destination-coordinates become the source-coordinates
	// xDest*xFact=xSrc
	float xFact;
	// yDest*yFact=ySrc
	float yFact;
	
	if (srcWidth<=1  || destWidth<=1) {
		xFact = 0;
	} else {
		xFact = ((double)srcWidth-1)/(destWidth-1);
	}

	if (srcHeight<=1 || destHeight<=1) {
		yFact = 0;
	} else {
		yFact = ((double)srcHeight-1)/(destHeight-1);
	}



	float xSrc, ySrc;
	float fX, fXi, fY, fYi;

	uint8_t* addrA;
	uint8_t* addrB;
	uint8_t* addrC;
	uint8_t* addrD;

	for (int32_t yDest = 0; yDest < destHeight; yDest++) {
		for (int32_t xDest = 0; xDest < destWidth; xDest++) {
			xSrc = ((float)xDest)*xFact;
			ySrc = ((float)yDest)*yFact;

			// A B
			// C D
			addrA = srcData + ( ((uint32_t)xSrc) + ((uint32_t)ySrc)*srcWidth ) * channels;
			addrB = addrA + channels;
			addrC = addrA + widthB;
			addrD = addrC + channels;

			// fX: the factor by which the lower X-pixel should be taken (0) or the higher one (1) 
			fX = xSrc-((int32_t)xSrc);
			fXi = 1-fX;
			// fY: the factor by which the lower Y-pixel should be taken (0) or the higher one (1)
			fY = ySrc-((int32_t)ySrc);
			fYi = 1-fY;

			for (int32_t c = 0; c < channels; c++) {
				destData[i] =  ( (*addrA)*fXi + (*addrB)*fX ) * fYi
								+ ( (*addrC)*fXi + (*addrD)*fX ) * fY;

				addrA++;
				addrB++;
				addrC++;
				addrD++;
				i++;
			}
		}
	}

	return resultImage;

}

} // namespace imgc::scaler