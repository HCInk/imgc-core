#include "../include/torasu/mod/imgc/Rauto_align2d.hpp"

#include <sstream>

#include <torasu/torasu.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/RenderableProperties.hpp>

#include <torasu/std/context_names.hpp>
#include <torasu/std/property_names.hpp>
#include <torasu/std/Dnum.hpp>

#include <torasu/mod/imgc/pipeline_names.hpp>
#include <torasu/mod/imgc/Ralign2d.hpp>

namespace imgc {

Rauto_align2d::Rauto_align2d(torasu::tools::RenderableSlot rndSrc, double posX, double posY, double zoomFactor, double ratio)
	: SimpleRenderable(true, false),
	  rndSrc(rndSrc), posX(posX), posY(posY), zoomFactor(zoomFactor), ratio(ratio) {

	internalAlign = new Ralign2d(rndSrc, this);
}

Rauto_align2d::~Rauto_align2d() {
	delete internalAlign;
}

torasu::Identifier Rauto_align2d::getType() {
	return "IMGC::RAUTO_ALIGN2D";
}

imgc::Dcropdata* Rauto_align2d::calcAlign(double posX, double posY, double zoomFactor, double srcRatio,
		double destRatio) const {

	posX = 0.5 + posX / 2;
	posY = 0.5 + posY / 2;

	// <1: Destination less wide then source
	// =1: Same ratio
	// >1: Destination more wide then source
	// (How wide is the destination to the source)
	double difRatio = destRatio / srcRatio;
	// The same, just inverse (How wide is the source relative to the destination)
	double invDifRatio = srcRatio / destRatio;

	if (difRatio == 1) {
		return new imgc::Dcropdata(0, 0, 0, 0);
	} else {

		// Margins Horizontal/Vertical
		double containHM, containVM,
			   coverHM, coverVM;

		if (difRatio > 1) { // Destination more wide

			containHM = -(invDifRatio-1);
			containVM = 0;
			coverHM = 0;
			coverVM = -(difRatio-1);

		} else { // Destination more high

			containHM = 0;
			containVM = -(difRatio-1);
			coverHM = -(invDifRatio-1);
			coverVM = 0;

		}

		// Horizontal Margin
		double hm = coverHM*zoomFactor + containHM*(1-zoomFactor);
		// Vertival Margin
		double vm = coverVM*zoomFactor + containVM*(1-zoomFactor);

		return new imgc::Dcropdata(hm*posX,     // Left
								   hm*(1-posX),// Right
								   vm*posY,    // Top
								   vm*(1-posY) // Bottom
								  );

	}

}

torasu::RenderResult* Rauto_align2d::render(torasu::RenderInstruction* ri) {

	torasu::tools::RenderHelper rh(ri);
	torasu::ResultSettings* resSettings = ri->getResultSettings();

	if (resSettings->getPipeline() == IMGC_PL_ALIGN) {
		double ratio;
		if (this->ratio == 0) {
			torasu::RenderableProperties* props = torasu::tools::getProperties(rndSrc.get(), {TORASU_STD_PROP_IMG_RAITO}, rh.ei, rh.li, rh.rctx);
			auto* dataRatio = torasu::tools::getPropertyValue<torasu::tstd::Dnum>(props, TORASU_STD_PROP_IMG_RAITO);
			ratio = dataRatio ? dataRatio->getNum() : 1;
			delete props;
		} else {
			ratio = this->ratio;
		}

		auto* rctx = rh.rctx;

		auto foundDestRatio = rctx->find(TORASU_STD_CTX_IMG_RATIO);
		double destRatio;

		if (foundDestRatio != rctx->end()) {
			destRatio = dynamic_cast<torasu::tstd::Dnum*>(foundDestRatio->second)->getNum();
		} else {
			throw std::runtime_error("Render-Request provided no TORASU_STD_CTX_IMG_RATIO in the context, so the image can't be aligned!");
		}

		if (destRatio <= 0) {
			throw std::runtime_error("Render-Request provided an invalid TORASU_STD_CTX_IMG_RATIO in the context (" +  std::to_string(destRatio) + ") - only ratios > 0 are allowed!");
		}

		auto* cropdata = calcAlign(this->posX, this->posY, this->zoomFactor, ratio, destRatio);

		return new torasu::RenderResult(torasu::RenderResultStatus::RenderResultStatus_OK, cropdata, true);

	} else {
		// Running render based on instruction
		return rh.runRender(internalAlign, resSettings);
	}

}

torasu::ElementMap Rauto_align2d::getElements() {
	torasu::ElementMap elems;

	elems["src"] = rndSrc.get();

	return elems;
}

void Rauto_align2d::setElement(std::string key, torasu::Element* elem) {
	if (torasu::tools::trySetRenderableSlot("src", &rndSrc, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

} // namespace imgc

