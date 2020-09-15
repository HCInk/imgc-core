#include "../include/torasu/mod/imgc/Rcropdata_combined.hpp"

#include <sstream>

#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dnum.hpp>

#include <torasu/mod/imgc/pipeline_names.hpp>
#include <torasu/mod/imgc/Dcropdata.hpp>

namespace imgc {

Rcropdata_combined::Rcropdata_combined(Renderable* left, Renderable* right, Renderable* top, Renderable* bottom)
	: SimpleRenderable("IMGC::RCROPDATA_COMBINED", false, true),
	  leftRnd(left), rightRnd(right), topRnd(top), bottomRnd(bottom)  {}


Rcropdata_combined::~Rcropdata_combined() {}

torasu::ResultSegment* Rcropdata_combined::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {
	std::string pipeline = resSettings->getPipeline();
	if (pipeline.compare(IMGC_PL_ALIGN) == 0) {

		auto ei = ri->getExecutionInterface();
		auto rctx = ri->getRenderContext();

		torasu::tools::RenderInstructionBuilder rib;
		auto resHandle = rib.addSegmentWithHandle<torasu::tstd::Dnum>(TORASU_STD_PL_NUM, nullptr);

		auto rendL = rib.enqueueRender(leftRnd, rctx, ei);
		auto rendR = rib.enqueueRender(rightRnd, rctx, ei);
		auto rendT = rib.enqueueRender(topRnd, rctx, ei);
		auto rendB = rib.enqueueRender(bottomRnd, rctx, ei);

		torasu::RenderResult* resL = ei->fetchRenderResult(rendL);
		torasu::RenderResult* resR = ei->fetchRenderResult(rendR);
		torasu::RenderResult* resT = ei->fetchRenderResult(rendT);
		torasu::RenderResult* resB = ei->fetchRenderResult(rendB);

		auto fetchedL = resHandle.getFrom(resL).getResult();
		auto fetchedR = resHandle.getFrom(resR).getResult();
		auto fetchedT = resHandle.getFrom(resT).getResult();
		auto fetchedB = resHandle.getFrom(resB).getResult();

		double lVal = fetchedL != nullptr ? fetchedL->getNum() : 0;
		double rVal = fetchedR != nullptr ? fetchedR->getNum() : 0;
		double tVal = fetchedT != nullptr ? fetchedT->getNum() : 0;
		double bVal = fetchedB != nullptr ? fetchedB->getNum() : 0;

		delete resL;
		delete resR;
		delete resT;
		delete resB;

		auto* result = new imgc::Dcropdata(lVal, rVal, tVal, bVal);

		return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, result, false);
	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}
}


torasu::ElementMap Rcropdata_combined::getElements() {
	torasu::ElementMap elems;

	elems["l"] = leftRnd;
	elems["r"] = rightRnd;
	elems["t"] = topRnd;
	elems["b"] = bottomRnd;

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