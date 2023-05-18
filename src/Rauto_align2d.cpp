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

Rauto_align2d::Rauto_align2d(torasu::RenderableSlot rndSrc, torasu::tstd::NumSlot posX, torasu::tstd::NumSlot posY, torasu::tstd::NumSlot zoomFactor, torasu::tstd::NumSlot ratio)
	: SimpleRenderable(false, true),
	  rndSrc(rndSrc), rndPosX(posX), rndPosY(posY), rndZoomFactor(zoomFactor), rndCustomRatio(ratio) {

	internalAlign = new Ralign2d(this->rndSrc.get(), this);
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
		torasu::ResultSettings numberSettings(TORASU_STD_PL_NUM, torasu::tools::NO_FORMAT);

		double posX;
		double posY;
		double zoomFactor;
		double ratio;

		bool useCustomRatio = rndCustomRatio.get() != nullptr;

		torasu::ExecutionInterface::ResultPair resultVector[4];

		resultVector[0] = {.renderId = rh.enqueueRender(rndPosX, &numberSettings)};
		resultVector[1] = {.renderId = rh.enqueueRender(rndPosY, &numberSettings)};
		resultVector[2] = {.renderId = rh.enqueueRender(rndZoomFactor, &numberSettings)};
		if (useCustomRatio) {
			resultVector[3] = {.renderId = rh.enqueueRender(rndCustomRatio, &numberSettings)};
		} else {
			torasu::RenderableProperties* props = torasu::tools::getProperties(rndSrc.get(), {TORASU_STD_PROP_IMG_RAITO}, rh.ei, rh.li, rh.rctx);
			auto* dataRatio = torasu::tools::getPropertyValue<torasu::tstd::Dnum>(props, TORASU_STD_PROP_IMG_RAITO);
			ratio = dataRatio ? dataRatio->getNum() : 1;
			delete props;
		}

		rh.fetchRenderResults(resultVector, useCustomRatio ? 4 : 3);

		{
			std::unique_ptr<torasu::RenderResult> rr(resultVector[0].result);
			auto res = rh.evalResult<torasu::tstd::Dnum>(rr.get());
			if (res) {
				posX = res.getResult()->getNum();
			} else {
				if (rh.mayLog(torasu::WARN)) {
					rh.lrib.logCause(torasu::WARN, "Failed to provde posX, using x-pos 0 (center)", res.takeInfoTag());
				}
				rh.lrib.hasError = true;
				posX = 0;
			}
		}

		{
			std::unique_ptr<torasu::RenderResult> rr(resultVector[1].result);
			auto res = rh.evalResult<torasu::tstd::Dnum>(rr.get());
			if (res) {
				posY = res.getResult()->getNum();
			} else {
				if (rh.mayLog(torasu::WARN)) {
					rh.lrib.logCause(torasu::WARN, "Failed to provde posY, using y-pos 0 (center)", res.takeInfoTag());
				}
				rh.lrib.hasError = true;
				posY = 0;
			}
		}

		{
			std::unique_ptr<torasu::RenderResult> rr(resultVector[2].result);
			auto res = rh.evalResult<torasu::tstd::Dnum>(rr.get());
			if (res) {
				zoomFactor = res.getResult()->getNum();
			} else {
				if (rh.mayLog(torasu::WARN)) {
					rh.lrib.logCause(torasu::WARN, "Failed to provde zoomFactor, using zoom 0 (contain)", res.takeInfoTag());
				}
				rh.lrib.hasError = true;
				zoomFactor = 1;
			}
		}

		if (useCustomRatio) {
			std::unique_ptr<torasu::RenderResult> rr(resultVector[3].result);
			auto res = rh.evalResult<torasu::tstd::Dnum>(rr.get());
			if (res) {
				ratio = res.getResult()->getNum();
			} else {
				if (rh.mayLog(torasu::WARN)) {
					rh.lrib.logCause(torasu::WARN, "Failed to provde customRatio, using 1:1 ratio", res.takeInfoTag());
				}
				rh.lrib.hasError = true;
				ratio = 1;
			}
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

		auto* cropdata = calcAlign(posX, posY, zoomFactor, ratio, destRatio);

		return rh.buildResult(cropdata);
	} else {
		// Running render based on instruction
		return rh.runRender(internalAlign, resSettings);
	}

}

torasu::ElementMap Rauto_align2d::getElements() {
	torasu::ElementMap elems;

	elems["src"] = rndSrc;
	elems["x"] = rndPosX;
	elems["y"] = rndPosY;
	elems["zoom"] = rndZoomFactor;
	elems["ratio"] = rndCustomRatio;

	return elems;
}

const torasu::OptElementSlot Rauto_align2d::setElement(std::string key, const torasu::ElementSlot* elem) {
	if (key == "src") {
		torasu::tools::trySetRenderableSlot(&rndSrc, elem);
		torasu::ElementSlot internalSlot(rndSrc.get());
		internalAlign->setElement("src", &internalSlot);
		return rndSrc.asElementSlot();
	}
	if (key == "x") return torasu::tools::trySetRenderableSlot(&rndPosX, elem);
	if (key == "y") return torasu::tools::trySetRenderableSlot(&rndPosY, elem);
	if (key == "zoom") return torasu::tools::trySetRenderableSlot(&rndZoomFactor, elem);
	if (key == "ratio") return torasu::tools::trySetRenderableSlot(&rndCustomRatio, elem);
	return nullptr;
}

} // namespace imgc

