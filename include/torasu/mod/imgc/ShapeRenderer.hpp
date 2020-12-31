#ifndef INCLUDE_TORASU_MOD_IMGC_GRAPHICSRENDERER_HPP_
#define INCLUDE_TORASU_MOD_IMGC_GRAPHICSRENDERER_HPP_

#include <torasu/std/Dbimg.hpp>

#include <torasu/mod/imgc/Dgraphics.hpp>

namespace imgc {
class ShapeRenderer {

public:

	static torasu::tstd::Dbimg* render(torasu::tstd::Dbimg_FORMAT fmt, Dgraphics::GShape shape);
};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_GRAPHICSRENDERER_HPP_
