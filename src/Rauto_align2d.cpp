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
	double difRatio = destRatio / srcRatio;
	// The same, just inverse
	double invDifRatio = srcRatio / destRatio;

	if (difRatio == 1) {
		return new imgc::Dcropdata(0, 0, 0, 0);
	} else {

		// Margins Horizontal/Vertical
		double containHM, containVM,
				 coverHM, coverVM;

		if (difRatio > 1) { // Destination more wide

			containHM = difRatio-1;
			containVM = 0;
			coverHM = 0;
			coverVM = invDifRatio-1;

		} else { // Destination more high

			containHM = 0;
			containVM = invDifRatio-1;
			coverHM = difRatio-1;
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
			delete props;
		} else {
			ratio = this->ratio;
		}

		auto foundDestRatio = rctx->find(TORASU_STD_CTX_IMG_RATIO);
		double destRatio = foundDestRatio != rctx->end() ? static_cast<torasu::tstd::Dnum*>(foundDestRatio->second)->getNum() : 0;

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

std::map<std::string, torasu::Element*> Rauto_align2d::getElements() {
	std::map<std::string, torasu::Element*> elems;

	elems["src"] = rndSrc;

	return elems;
}

void Rauto_align2d::setElement(std::string key, torasu::Element* elem) {

	if (key.compare("src") == 0) {

		if (elem == NULL) {
			throw std::invalid_argument("Element slot \"src\" may not be empty!");
		}
		if (torasu::Renderable* rnd = dynamic_cast<torasu::Renderable*>(elem)) {
			rndSrc = rnd;
			internalAlign->setElement("src", rnd);
			return;
		} else {
			throw std::invalid_argument("Element slot \"src\" only accepts Renderables!");
		}

	} else {
		std::ostringstream errMsg;
		errMsg << "The element slot \""
			   << key
			   << "\" does not exist!";
		throw std::invalid_argument(errMsg.str());
	}
}

} // namespace imgc

