#ifndef INCLUDE_TORASU_MOD_IMGC_RALIGN2D_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RALIGN2D_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/slot_tools.hpp>

#include <torasu/std/spoilsD.hpp>

namespace imgc {

class Ralign2d : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedRenderableSlot  rndSrc;
	torasu::tools::ManagedRenderableSlot rndAlign;

	struct Ralign2d_CROPDATA {
		uint32_t srcWidth, srcHeight;
		int32_t offLeft, offRight, offTop, offBottom;
	};

	void calcAlign(Renderable* alignmentProvider, torasu::ExecutionInterface* ei, torasu::RenderContext* rctx,
				   uint32_t destWidth, uint32_t destHeight,
				   Ralign2d_CROPDATA* outCropData) const;

	void align(torasu::tstd::Dbimg* srcImg, torasu::tstd::Dbimg* destImg, Ralign2d_CROPDATA* cropData) const;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;

public:
	Ralign2d(torasu::tools::RenderableSlot rndSrc, torasu::tools::RenderableSlot rndAlign);
	~Ralign2d();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RALIGN2D_HPP_
