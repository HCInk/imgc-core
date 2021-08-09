#ifndef INCLUDE_TORASU_MOD_IMGC_RTRANSFORM_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RTRANSFORM_HPP_

#include <torasu/torasu.hpp>
#include <torasu/slot_tools.hpp>
#include <torasu/mod/imgc/Dgraphics.hpp>
#include <torasu/SimpleRenderable.hpp>

#include <torasu/std/Rnum.hpp>

namespace imgc {

class Rtransform : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedRenderableSlot source;
	torasu::tools::ManagedRenderableSlot transform;
	torasu::tools::ManagedRenderableSlot shutter;
	torasu::tools::ManagedRenderableSlot interpolationLimit;

protected:
	torasu::ResultSegment* render(torasu::RenderInstruction* ri) override;

public:
	Rtransform(torasu::tools::RenderableSlot source, torasu::tools::RenderableSlot transform, torasu::tstd::NumSlot shutter = nullptr, torasu::tstd::NumSlot interpolationLimit = nullptr);
	~Rtransform();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};


} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RTRANSFORM_HPP_
