#include "../include/torasu/mod/imgc/Rcropdata_combined.hpp"

#include <sstream>

#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>

#include <torasu/mod/imgc/pipeline_names.hpp>
#include <torasu/mod/imgc/Dcropdata.hpp>

namespace imgc {

Rcropdata_combined::Rcropdata_combined(torasu::tstd::NumSlot left, torasu::tstd::NumSlot right, torasu::tstd::NumSlot top, torasu::tstd::NumSlot bottom)
	: SimpleRenderable(false, true),
	  leftRnd(left), rightRnd(right), topRnd(top), bottomRnd(bottom) {}


Rcropdata_combined::~Rcropdata_combined() {}

torasu::Identifier Rcropdata_combined::getType() {
	return "IMGC::RCROPDATA_COMBINED";
}

torasu::RenderResult* Rcropdata_combined::render(torasu::RenderInstruction* ri) {
	torasu::tools::RenderHelper rh(ri);
	auto resSettings = ri->getResultSettings();
	if (resSettings->getPipeline() == IMGC_PL_ALIGN) {
		torasu::ResultSettings rsNum(TORASU_STD_PL_NUM, nullptr);

		auto rendL = rh.enqueueRender(leftRnd, &rsNum);
		auto rendR = rh.enqueueRender(rightRnd, &rsNum);
		auto rendT = rh.enqueueRender(topRnd, &rsNum);
		auto rendB = rh.enqueueRender(bottomRnd, &rsNum);

		std::unique_ptr<torasu::RenderResult> resL(rh.fetchRenderResult(rendL));
		std::unique_ptr<torasu::RenderResult> resR(rh.fetchRenderResult(rendR));
		std::unique_ptr<torasu::RenderResult> resT(rh.fetchRenderResult(rendT));
		std::unique_ptr<torasu::RenderResult> resB(rh.fetchRenderResult(rendB));

		auto fetchedL = rh.evalResult<torasu::tstd::Dnum>(resL.get()).getResult();
		auto fetchedR = rh.evalResult<torasu::tstd::Dnum>(resR.get()).getResult();
		auto fetchedT = rh.evalResult<torasu::tstd::Dnum>(resT.get()).getResult();
		auto fetchedB = rh.evalResult<torasu::tstd::Dnum>(resB.get()).getResult();

		double lVal = fetchedL ? fetchedL->getNum() : 0;
		double rVal = fetchedR ? fetchedR->getNum() : 0;
		double tVal = fetchedT ? fetchedT->getNum() : 0;
		double bVal = fetchedB ? fetchedB->getNum() : 0;

		return new torasu::RenderResult(torasu::RenderResultStatus_OK, new imgc::Dcropdata(lVal, rVal, tVal, bVal), true);
	} else {
		return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_SEGMENT);
	}
}


torasu::ElementMap Rcropdata_combined::getElements() {
	torasu::ElementMap elems;

	elems["l"] = leftRnd.get();
	elems["r"] = rightRnd.get();
	elems["t"] = topRnd.get();
	elems["b"] = bottomRnd.get();

	return elems;
}

void Rcropdata_combined::setElement(std::string key, torasu::Element* elem) {
	if (torasu::tools::trySetRenderableSlot("l", &leftRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("r", &rightRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("t", &topRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("b", &bottomRnd, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

} // namespace imgc