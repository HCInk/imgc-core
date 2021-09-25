#include "../include/torasu/mod/imgc/Rgain.hpp"

#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>

namespace imgc {

Rgain::Rgain(torasu::tools::RenderableSlot src, torasu::tstd::NumSlot gainVal)
	: SimpleRenderable(false, true),
	  rSrc(src), rGainVal(gainVal) {}

Rgain::~Rgain() {}

torasu::Identifier Rgain::getType() { return "IMGC::RGAIN"; }

torasu::ResultSegment* Rgain::render(torasu::RenderInstruction* ri) {
	torasu::tools::RenderHelper rh(ri);
	if (!rh.matchPipeline(TORASU_STD_PL_VIS)) {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}

	auto* format = rh.rs->getFromat();
	uint32_t rWidth, rHeight;
	if (auto* bimgFormat = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(format)) {
		rWidth = bimgFormat->getWidth();
		rHeight = bimgFormat->getHeight();
	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
	}

	// Creation of Requests
	torasu::ResultSettings rsVis(TORASU_STD_PL_VIS, format);
	auto srcRndId = rh.enqueueRender(rSrc, &rsVis);

	torasu::ResultSettings rsNum(TORASU_STD_PL_NUM, nullptr);
	auto gainRndId = rh.enqueueRender(rGainVal, &rsNum);

	// Fetching of requests

	std::unique_ptr<torasu::ResultSegment> srcRndResult(rh.fetchRenderResult(srcRndId));
	std::unique_ptr<torasu::ResultSegment> gainRndResult(rh.fetchRenderResult(gainRndId));

	auto srcRes = rh.evalResult<torasu::tstd::Dbimg>(srcRndResult.get());
	auto gainRes = rh.evalResult<torasu::tstd::Dnum>(gainRndResult.get());

	torasu::tstd::Dbimg* srcImg = srcRes.getResult();

	if (srcImg == NULL) {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK_WARN, new torasu::tstd::Dbimg(rWidth, rHeight), true);
	}

	if (gainRes.getResult() == NULL) {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK_WARN, new torasu::tstd::Dbimg(*srcImg), true);
	}

	double gainVal = gainRes.getResult()->getNum();

	if (gainVal == 1) {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, new torasu::tstd::Dbimg(*srcImg), true);
	}

	torasu::tstd::Dbimg* destImg = new torasu::tstd::Dbimg(rWidth, rHeight);

	uint8_t* srcData = srcImg->getImageData();
	uint8_t* destData = destImg->getImageData();

	double buff;
	for (uint32_t p = 0; p < rHeight*rWidth; p++) {
		buff = gainVal*(*srcData);
		*destData = buff < 0xFF ? buff : 0xFF;
		srcData++;
		destData++;
		buff = gainVal*(*srcData);
		*destData = buff < 0xFF ? buff : 0xFF;
		srcData++;
		destData++;
		buff = gainVal*(*srcData);
		*destData = buff < 0xFF ? buff : 0xFF;
		srcData++;
		destData++;
		*destData = *srcData;
		srcData++;
		destData++;
	}

	return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, destImg, true);

}

torasu::ElementMap Rgain::getElements() {
	torasu::ElementMap elems;
	elems["src"] = rSrc.get();
	elems["val"] = rGainVal.get();
	return elems;
}

void Rgain::setElement(std::string key, Element* elem) {
	if (torasu::tools::trySetRenderableSlot("src", &rSrc, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("val", &rGainVal, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

} // namespace imgc