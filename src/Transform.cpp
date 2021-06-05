#include "../include/torasu/mod/imgc/Transform.hpp"

namespace imgc::transform {

#define PREBUF false

namespace {
static const uint32_t channels = 4;

#if PREBUF
inline void addToAccu(float accu[4], const float* src, float factor) {
	for (uint32_t c = 0; c < channels; c++) {
		accu[c] += src[c]*factor;
	}
}
#else
inline void addToAccu(float accu[4], const uint8_t* src, float factor) {
	for (uint32_t c = 0; c < channels; c++) {
		accu[c] += static_cast<float>(src[c])*factor/0xFF;
	}
}
#endif

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
					(*srcBuffPtr) = static_cast<float>(*srcPtr)/0xFF;
					srcBuffPtr++;
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
			size_t xBase = xAbs;
			size_t yBase = yAbs;

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
			{
#if PREBUF
				const float* localSrc = srcBuffPtr+( (yBase*width + xBase)*channels);
#else
				const uint8_t* localSrc = src+( (yBase*width + xBase)*channels);
#endif

				float xFac = xAbs-xBase;
				float yFac = yAbs-yBase;
				if (xBase >= 0 && yBase >= 0 && xBase < widthSub && yBase < heightSub) {
					// addToAccu(accu, localSrc, 1);
					addToAccu(accu, localSrc, (1-xFac)*(1-yFac));
					addToAccu(accu, localSrc+channels, (xFac)*(1-yFac));
					addToAccu(accu, localSrc+width*channels, (1-xFac)*(yFac));
					addToAccu(accu, localSrc+(width+1)*channels, (xFac)*(yFac));
				}
			}

			for (uint32_t c = 0; c < channels; c++) {
				(*dest) = static_cast<uint8_t>(accu[c]*0xFF);
				dest++;
			}

		}
	}
}

} // namespace imgc
