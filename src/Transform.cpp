#include "../include/torasu/mod/imgc/Transform.hpp"

namespace imgc::transform {

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

	const uint32_t channels = 4;
	const size_t dataSize = height*width*channels;

	const uint32_t clear = 0x0;

	size_t heightSub = height-1;
	size_t widthSub = width-1;
	for (uint32_t y = 0; y < height; y++) {
		double yDest = static_cast<double>(y) / heightSub;
		for (uint32_t x = 0; x < width; x++) {
			double xDest = static_cast<double>(x) / widthSub;

			double xSrc = xDest*invCords[0] + yDest*invCords[1] + invCords[2];
			double ySrc = xDest*invCords[3] + yDest*invCords[4] + invCords[5];

			const uint8_t* localSrc;
			if (xSrc >= 0 && xSrc <= 1 && ySrc >= 0 && ySrc <= 1) {
				localSrc = src+( (static_cast<size_t>(ySrc*heightSub)*width + static_cast<size_t>(xSrc*widthSub))*channels);
			} else {
				localSrc = reinterpret_cast<const uint8_t*>(&clear);
			}
			for (uint32_t c = 0; c < channels; c++) {
				(*dest) = localSrc[c];
				dest++;
			}

		}
	}
}

} // namespace imgc
