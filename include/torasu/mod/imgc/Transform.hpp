#ifndef INCLUDE_TORASU_MOD_IMGC_TRANSFORM_HPP_
#define INCLUDE_TORASU_MOD_IMGC_TRANSFORM_HPP_
#endif // INCLUDE_TORASU_MOD_IMGC_TRANSFORM_HPP_

#include <cstdint>
#include <stddef.h>

#include <torasu/std/Dmatrix.hpp>

namespace imgc::transform {

void transform(const uint8_t* src, uint8_t* dest, uint32_t width, uint32_t height, torasu::tstd::Dmatrix matrix);

} // namespace imgc
