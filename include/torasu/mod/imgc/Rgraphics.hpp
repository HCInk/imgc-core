#ifndef INCLUDE_TORASU_MOD_IMGC_RGRAPHICS_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RGRAPHICS_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rgraphics : public torasu::tools::SimpleRenderable {

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;

public:
	Rgraphics();
	~Rgraphics();
};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_RGRAPHICS_HPP_
