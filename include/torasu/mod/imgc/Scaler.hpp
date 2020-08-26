#ifndef INCLUDE_TORASU_MOD_IMGC_SCALER_HPP_
#define INCLUDE_TORASU_MOD_IMGC_SCALER_HPP_

#include <torasu/torasu.hpp>

#include <torasu/std/Dbimg.hpp>

namespace imgc::scaler {

/**
 * @brief  Scales the given image
 * @param  srcData: The source data to be scaled in RGBA-format
 * @param  srcWidth: The with of the source-image
 * @param  srcHeight: The height of the source-image
 * @param  fmt: The format to be scaled to
 * @param  copySame: Select weather to copy the the image when no scaling is required (true)
 * 						or return NULL if thats the case (false)
 * @retval The scaled image or NULL of no scaling is required
 */
torasu::tstd::Dbimg* scaleImg(u_int8_t* srcData, uint32_t srcWidth, uint32_t srcHeight, torasu::tstd::Dbimg_FORMAT* fmt, bool copySame=false);

/**
 * @brief Scales the given image
 * @param  src: The image to be scaled
 * @param  fmt: The format to be scaled to
 * @param  copySame: Select weather to copy the the image when no scaling is required (true)
 * 						or return NULL if thats the case (false)
 * @retval The scaled image or NULL of no scaling is required
 */
inline torasu::tstd::Dbimg* scaleImg(torasu::tstd::Dbimg* src, torasu::tstd::Dbimg_FORMAT* fmt, bool copySame=false) {
	return scaleImg(src->getImageData(), src->getWidth(), src->getHeight(), fmt, copySame);
}


} // namespace imgc::scaler


#endif // INCLUDE_TORASU_MOD_IMGC_SCALER_HPP_
