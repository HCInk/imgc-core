#ifndef INCLUDE_TORASU_MOD_IMGC_RAUTOALIGN2D_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RAUTOALIGN2D_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/slot_tools.hpp>

#include <torasu/std/Rnum.hpp>

#include <torasu/mod/imgc/Dcropdata.hpp>

namespace imgc {

class Rauto_align2d : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedRenderableSlot rndSrc;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> rndPosX;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> rndPosY;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> rndZoomFactor;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> rndCustomRatio;
	Renderable* internalAlign;

	imgc::Dcropdata* calcAlign(double posX, double posY, double zoomFactor, double srcRatio,
							   double destRatio) const;

protected:
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:

	Rauto_align2d(torasu::RenderableSlot rndSrc, torasu::tstd::NumSlot posX, torasu::tstd::NumSlot posY, torasu::tstd::NumSlot zoomFactor, torasu::tstd::NumSlot ratio = nullptr);
	~Rauto_align2d();
	torasu::Identifier getType() override;

	torasu::ElementMap getElements() override;
	const torasu::OptElementSlot setElement(std::string key, const torasu::ElementSlot* elem) override;

};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_RAUTOALIGN2D_HPP_
