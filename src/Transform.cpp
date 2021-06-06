#include "../include/torasu/mod/imgc/Transform.hpp"

namespace imgc::transform {

#define PREBUF false

namespace {
static const uint32_t channels = 4;

template<typename T> inline void addToAccu(float accu[4], const T* src, float factor) {
	for (uint32_t c = 0; c < channels; c++) {
		accu[c] += static_cast<float>(src[c])*factor;
	}
}

template<typename T> inline void pixelCollect(float accu[4], const T* src, double xAbs, double yAbs, size_t widthSub, size_t heightSub, size_t srcNewLine, bool evalulateOnLimit) {
	size_t xBase = xAbs;
	size_t yBase = yAbs;
	float xFac = xAbs-xBase;
	float yFac = yAbs-yBase;
	const T* localSrc = src+( (yBase*srcNewLine + xBase)*channels);
	if (xBase >= 0 && yBase >= 0 && xBase < widthSub && yBase < heightSub) {
		// accuAdd(accu, localSrc, 1);
		addToAccu<T>(accu, localSrc, (1-xFac)*(1-yFac));
		addToAccu<T>(accu, localSrc+channels, (xFac)*(1-yFac));
		addToAccu<T>(accu, localSrc+srcNewLine*channels, (1-xFac)*(yFac));
		addToAccu<T>(accu, localSrc+(srcNewLine+1)*channels, (xFac)*(yFac));
	} else if (evalulateOnLimit && (xBase == widthSub || xBase == heightSub)) {
		addToAccu<T>(accu, localSrc, (1-xFac)*(1-yFac));
		if (xBase < widthSub) {
			addToAccu<T>(accu, localSrc+channels, (xFac)*(1-yFac));
		} else if (yBase < heightSub) {
			addToAccu<T>(accu, localSrc+srcNewLine*channels, (1-xFac)*(yFac));
		}
	}
}

} // namespace


void transform(const uint8_t* src, uint8_t* dest, uint32_t width, uint32_t height, torasu::tstd::Dmatrix transformMatrix) {

	torasu::tstd::Dmatrix adjust({
		0.5, 0.0, 0.5,
		0.0, -0.5, 0.5,
		0.0, 0.0, 1.0
	}, 3);

	torasu::tstd::Dmatrix adjusted = adjust.multiplyByMatrix(transformMatrix).multiplyByMatrix(adjust.inverse());

	torasu::tstd::Dmatrix inverse = adjusted.inverse();

	torasu::tstd::Dnum* matrixNums = inverse.getNums();
	double invCords[9];
	for (size_t i = 0; i < 9; i++) {
		invCords[i] = matrixNums[i].getNum();
	}

#if PREBUF
	std::vector<float> srcBuffVec((width+1)*(height+1)*channels);

	{
		const uint8_t* srcPtr = src;
		float* srcBuffPtr = srcBuffVec.data();
		for (uint32_t y = 0; y < height; y++) {
			for (uint32_t x = 0; x < width; x++) {
				for (uint32_t c = 0; c < channels; c++) {
					(*srcBuffPtr) = static_cast<float>(*srcPtr);
					srcBuffPtr++;
					srcPtr++;
				}
			}

			for (uint32_t c = 0; c < channels; c++) {
				(*srcBuffPtr) = 0.0;
				srcBuffPtr++;
			}
		}
		
		for (uint32_t i = 0; i < (width+1)*channels; i++) {
			(*srcBuffPtr) = 0.0;
			srcBuffPtr++;
		}
		
	}
	
	const float* srcBuffPtr = srcBuffVec.data();
#endif

	size_t widthSub = width-1;
	size_t heightSub = height-1;
	for (uint32_t y = 0; y < height; y++) {
		double yDest = static_cast<double>(y) / heightSub;
		for (uint32_t x = 0; x < width; x++) {
			double xDest = static_cast<double>(x) / widthSub;

			double xRel = xDest*invCords[0] + yDest*invCords[1] + invCords[2];
			double yRel = xDest*invCords[3] + yDest*invCords[4] + invCords[5];
			double xAbs = xRel*widthSub;
			double yAbs = yRel*heightSub;

			// const uint8_t* localSrc;
			// if (xSrc >= 0 && xSrc <= 1 && ySrc >= 0 && ySrc <= 1) {
			// 	localSrc = src+( (yBase*width + xBase)*channels );
			// } else {
			// 	localSrc = reinterpret_cast<const uint8_t*>(&clear);
			// }
			// for (uint32_t c = 0; c < channels; c++) {
			// 	(*dest) = localSrc[c];
			// 	dest++;
			// }

			float accu[4] = {0,0,0,0};
#if PREBUF
			pixelCollect<float>(accu, srcBuffPtr, xAbs, yAbs, width, height, width+1, false);
#else
			pixelCollect<uint8_t>(accu, src, xAbs, yAbs, widthSub, heightSub, width, true);
#endif
			for (uint32_t c = 0; c < channels; c++) {
				(*dest) = static_cast<uint8_t>(accu[c]);
				dest++;
			}

		}
	}
}

} // namespace imgc
