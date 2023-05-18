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
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:
	Rtransform(torasu::RenderableSlot source, torasu::RenderableSlot transform, torasu::tstd::NumSlot shutter = nullptr, torasu::tstd::NumSlot interpolationLimit = nullptr);
	~Rtransform();
	torasu::Identifier getType() override;

	torasu::ElementMap getElements() override;
	const torasu::OptElementSlot setElement(std::string key, const torasu::ElementSlot* elem) override;
};


} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RTRANSFORM_HPP_
