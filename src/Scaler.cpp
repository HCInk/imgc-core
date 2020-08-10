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

	float xFact = ((double)srcWidth)/destWidth;
	float yFact = ((double)srcHeight)/destHeight;

	float posX, posY;
	float fX, fXi, fY, fYi;

	uint8_t* addrA;
	uint8_t* addrB;
	uint8_t* addrC;
	uint8_t* addrD;

	for (int32_t y = 0; y < destHeight; y++) {
		for (int32_t x = 0; x < destWidth; x++) {
			posX = ((float)x)*xFact;
			posY = ((float)y)*yFact;

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