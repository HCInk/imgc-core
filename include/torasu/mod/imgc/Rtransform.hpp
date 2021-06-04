#ifndef INCLUDE_TORASU_MOD_IMGC_RTRANSFORM_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RTRANSFORM_HPP_

#include <torasu/torasu.hpp>
#include <torasu/slot_tools.hpp>
#include <torasu/mod/imgc/Dgraphics.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rtransform : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedRenderableSlot source;
	torasu::tools::ManagedRenderableSlot transform;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;

public:
	Rtransform(torasu::tools::RenderableSlot source, torasu::tools::RenderableSlot transform);
	~Rtransform();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};


} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RTRANSFORM_HPP_
