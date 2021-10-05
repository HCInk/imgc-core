#ifndef INCLUDE_TORASU_MOD_IMGC_TRANSFORM_HPP_
	#define INCLUDE_TORASU_MOD_IMGC_TRANSFORM_HPP_
#endif // INCLUDE_TORASU_MOD_IMGC_TRANSFORM_HPP_

#include <cstdint>
#include <stddef.h>

#include <torasu/std/Dmatrix.hpp>

namespace imgc::transform {

void transform(const uint8_t* src, uint8_t* dest, uint32_t srcWidth, uint32_t srcHeight, uint32_t destWidth, uint32_t destHeight, torasu::tstd::Dmatrix matrix);
void transformMix(const uint8_t* src, uint8_t* dest, uint32_t srcWidth, uint32_t srcHeight, uint32_t destWidth, uint32_t destHeight, const torasu::tstd::Dmatrix* transArray, size_t nTransforms);

} // namespace imgc
