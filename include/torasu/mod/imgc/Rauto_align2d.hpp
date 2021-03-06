#ifndef INCLUDE_TORASU_MOD_IMGC_RAUTOALIGN2D_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RAUTOALIGN2D_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/slot_tools.hpp>

#include <torasu/mod/imgc/Dcropdata.hpp>

namespace imgc {

class Rauto_align2d : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedRenderableSlot rndSrc;
	Renderable* internalAlign;
	double posX, posY, zoomFactor, ratio;

	imgc::Dcropdata* calcAlign(double posX, double posY, double zoomFactor, double srcRatio,
							   double destRatio) const;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;

public:

	Rauto_align2d(torasu::tools::RenderableSlot rndSrc, double posX, double posY, double zoomFactor, double ratio = 0);
	~Rauto_align2d();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;

};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_RAUTOALIGN2D_HPP_
