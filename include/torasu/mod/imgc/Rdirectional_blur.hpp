#ifndef INCLUDE_TORASU_MOD_IMGC_RDIRECTIONAL_BLUR_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RDIRECTIONAL_BLUR_HPP_

#include <torasu/torasu.hpp>
#include <torasu/slot_tools.hpp>
#include <torasu/SimpleRenderable.hpp>

#include <torasu/std/Rnum.hpp>

namespace imgc {

class Rdirectional_blur : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedRenderableSlot src;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> direction;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> strength;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;


public:
	Rdirectional_blur(torasu::tools::RenderableSlot src, torasu::tstd::NumSlot direction, torasu::tstd::NumSlot strength);
	~Rdirectional_blur();

	torasu::ElementMap getElements();
	void setElement(std::string key, torasu::Element* elem);

};


} // namespace imgc



#endif // INCLUDE_TORASU_MOD_IMGC_RDIRECTIONAL_BLUR_HPP_
