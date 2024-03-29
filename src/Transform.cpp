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

template<typename T> inline void pixelCollect(float accu[4], const T* src, double xAbs, double yAbs, int32_t widthSub, int32_t heightSub, int32_t srcNewLine, bool evalulateOnLimit) {
	int32_t xBase = xAbs;
	int32_t yBase = yAbs;
	float xFac = xAbs-xBase;
	float yFac = yAbs-yBase;
	const T* localSrc = src+( (yBase*srcNewLine + xBase)*channels);
	if (xBase >= 0 && yBase >= 0) {
		if (xBase < widthSub && yBase < heightSub) {
			// accuAdd(accu, localSrc, 1);
			addToAccu<T>(accu, localSrc, (1-xFac)*(1-yFac));
			addToAccu<T>(accu, localSrc+channels, (xFac)*(1-yFac));
			addToAccu<T>(accu, localSrc+srcNewLine*channels, (1-xFac)*(yFac));
			addToAccu<T>(accu, localSrc+(srcNewLine+1)*channels, (xFac)*(yFac));
		} else if (evalulateOnLimit && (xBase <= widthSub && yBase <= heightSub)) {
			addToAccu<T>(accu, localSrc, (1-xFac)*(1-yFac));
			if (xBase < widthSub) {
				addToAccu<T>(accu, localSrc+channels, (xFac)*(1-yFac));
			} else if (yBase < heightSub) {
				addToAccu<T>(accu, localSrc+srcNewLine*channels, (1-xFac)*(yFac));
			}
		}
	}
}

void calcInvCords(double invCords[9], torasu::tstd::Dmatrix transformMatrix) {
	torasu::tstd::Dmatrix adjust({
		0.5, 0.0, 0.5,
		0.0, -0.5, 0.5,
		0.0, 0.0, 1.0
	}, 3);

	torasu::tstd::Dmatrix adjusted = adjust.multiplyByMatrix(transformMatrix).multiplyByMatrix(adjust.inverse());

	torasu::tstd::Dmatrix inverse = adjusted.inverse();

	torasu::tstd::Dnum* matrixNums = inverse.getNums();

	for (size_t i = 0; i < 9; i++) {
		invCords[i] = matrixNums[i].getNum();
	}
}

} // namespace


void transform(const uint8_t* src, uint8_t* dest, uint32_t srcWidth, uint32_t srcHeight, uint32_t destWidth, uint32_t destHeight, torasu::tstd::Dmatrix transformMatrix) {
	double invCords[9];
	calcInvCords(invCords, transformMatrix);

#if PREBUF
	std::vector<float> srcBuffVec((srcWidth+1)*(srcHeight+1)*channels);

	{
		const uint8_t* srcPtr = src;
		float* srcBuffPtr = srcBuffVec.data();
		for (uint32_t y = 0; y < srcHeight; y++) {
			for (uint32_t x = 0; x < srcWidth; x++) {
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

		for (uint32_t i = 0; i < (srcWidth+1)*channels; i++) {
			(*srcBuffPtr) = 0.0;
			srcBuffPtr++;
		}

	}

	const float* srcBuffPtr = srcBuffVec.data();
#endif

	const int32_t srcWidthSub = srcWidth-1;
	const int32_t srcHeightSub = srcHeight-1;
	const int32_t destWidthSub = destWidth-1;
	const int32_t destHeightSub = destHeight-1;
	for (uint32_t y = 0; y < destHeight; y++) {
		double yDest = static_cast<double>(y) / destHeightSub;
		for (uint32_t x = 0; x < destWidth; x++) {
			double xDest = static_cast<double>(x) / destWidthSub;

			double xRel = xDest*invCords[0] + yDest*invCords[1] + invCords[2];
			double yRel = xDest*invCords[3] + yDest*invCords[4] + invCords[5];
			double xAbs = xRel*srcWidthSub;
			double yAbs = yRel*srcHeightSub;

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
			pixelCollect<float>(accu, srcBuffPtr, xAbs, yAbs, srcWidth, srcHeight, srcWidth+1, false);
#else
			pixelCollect<uint8_t>(accu, src, xAbs, yAbs, srcWidthSub, srcHeightSub, srcWidth, true);
#endif
			for (uint32_t c = 0; c < channels; c++) {
				// (*dest) = static_cast<uint8_t>(accu[c]);
				(*dest) = static_cast<uint8_t>(std::round(accu[c]));
				dest++;
			}

		}
	}
}

struct TransformMatrix {
	double values[9];
};

void transformMix(const uint8_t* src, uint8_t* dest, uint32_t srcWidth, uint32_t srcHeight, uint32_t destWidth, uint32_t destHeight, const torasu::tstd::Dmatrix* transArray, size_t nTransforms) {
	std::vector<TransformMatrix> invCordsVec(nTransforms);
	auto invCordsArr = invCordsVec.data();
	for (size_t t = 0; t < nTransforms; t++) {
		calcInvCords(invCordsArr[t].values, transArray[t]);
	}

#if PREBUF
	std::vector<float> srcBuffVec((srcWidth+1)*(srcHeight+1)*channels);

	{
		const uint8_t* srcPtr = src;
		float* srcBuffPtr = srcBuffVec.data();
		for (uint32_t y = 0; y < srcHeight; y++) {
			for (uint32_t x = 0; x < srcWidth; x++) {
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

		for (uint32_t i = 0; i < (srcWidth+1)*channels; i++) {
			(*srcBuffPtr) = 0.0;
			srcBuffPtr++;
		}

	}

	const float* srcBuffPtr = srcBuffVec.data();
#endif

	const int32_t srcWidthSub = srcWidth-1;
	const int32_t srcHeightSub = srcHeight-1;
	const int32_t destWidthSub = destWidth-1;
	const int32_t destHeightSub = destHeight-1;
	for (uint32_t y = 0; y < destHeight; y++) {
		double yDest = static_cast<double>(y) / destHeightSub;
		for (uint32_t x = 0; x < destWidth; x++) {
			double xDest = static_cast<double>(x) / destWidthSub;

			float accu[4] = {0,0,0,0};
			for (size_t t = 0; t < nTransforms; t++) {

				auto invCords = invCordsArr[t].values;


				double xRel = xDest*invCords[0] + yDest*invCords[1] + invCords[2];
				double yRel = xDest*invCords[3] + yDest*invCords[4] + invCords[5];
				double xAbs = xRel*srcWidthSub;
				double yAbs = yRel*srcHeightSub;

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

#if PREBUF
				pixelCollect<float>(accu, srcBuffPtr, xAbs, yAbs, srcWidth, srcHeight, srcWidth+1, false);
#else
				pixelCollect<uint8_t>(accu, src, xAbs, yAbs, srcWidthSub, srcHeightSub, srcWidth, true);
#endif
			}

			for (uint32_t c = 0; c < channels; c++) {
				(*dest) = static_cast<uint8_t>(accu[c]/nTransforms);
				dest++;
			}

		}
	}
}

} // namespace imgc
