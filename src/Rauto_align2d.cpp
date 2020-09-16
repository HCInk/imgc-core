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

Rauto_align2d::Rauto_align2d(Renderable* rndSrc, double posX, double posY, double zoomFactor, double ratio)
	: SimpleRenderable("IMGC::RAUTO_ALIGN2D", true, false),
	  rndSrc(rndSrc), posX(posX), posY(posY), zoomFactor(zoomFactor), ratio(ratio) {

	internalAlign = new Ralign2d(rndSrc, this);
}

Rauto_align2d::~Rauto_align2d() {
	delete internalAlign;
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

torasu::ResultSegment* Rauto_align2d::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {

	auto* ei = ri->getExecutionInterface();
	auto* rctx = ri->getRenderContext();

	if (resSettings->getPipeline() == IMGC_PL_ALIGN) {
		double ratio;
		if (this->ratio == 0) {
			torasu::RenderableProperties* props = torasu::tools::getProperties(rndSrc, {TORASU_STD_PROP_IMG_RAITO}, ei);
			auto* dataRatio = torasu::tools::getPropertyValue<torasu::tstd::Dnum>(props, TORASU_STD_PROP_IMG_RAITO);
			ratio = dataRatio ? dataRatio->getNum() : 1;
			delete dataRatio; // XXX For now until props handles the property-deletion
			delete props;
		} else {
			ratio = this->ratio;
		}

		auto foundDestRatio = rctx->find(TORASU_STD_CTX_IMG_RATIO);
		double destRatio = foundDestRatio != rctx->end() ? dynamic_cast<torasu::tstd::Dnum*>(foundDestRatio->second)->getNum() : 0;

		auto* cropdata = calcAlign(this->posX, this->posY, this->zoomFactor, ratio, destRatio);

		return new torasu::ResultSegment(torasu::ResultSegmentStatus::ResultSegmentStatus_OK, cropdata, true);

	} else {
		torasu::tools::RenderInstructionBuilder rib;

		auto handle = rib.addSegmentWithHandle<torasu::DataResource>(resSettings->getPipeline(), resSettings->getResultFormatSettings());

		// Running render based on instruction

		torasu::RenderResult* rr = rib.runRender(internalAlign, rctx, ei);

		auto res = handle.getFrom(rr);

		auto* rs = res.canFreeResult() ?
				   new torasu::ResultSegment(res.getStatus(), res.ejectResult(), true) :
				   new torasu::ResultSegment(res.getStatus(), res.getResult(), false);

		delete rr;

		return rs;
	}

}

torasu::ElementMap Rauto_align2d::getElements() {
	torasu::ElementMap elems;

	elems["src"] = rndSrc;

	return elems;
}

void Rauto_align2d::setElement(std::string key, torasu::Element* elem) {
	if (torasu::tools::trySetRenderableSlot("src", &rndSrc, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

} // namespace imgc

