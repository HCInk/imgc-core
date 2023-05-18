#include "../include/torasu/mod/imgc/Rgain.hpp"

#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>

namespace imgc {

Rgain::Rgain(torasu::RenderableSlot src, torasu::tstd::NumSlot gainVal)
	: SimpleRenderable(false, true),
	  rSrc(src), rGainVal(gainVal) {}

Rgain::~Rgain() {}

torasu::Identifier Rgain::getType() {
	return "IMGC::RGAIN";
}

torasu::RenderResult* Rgain::render(torasu::RenderInstruction* ri) {
	torasu::tools::RenderHelper rh(ri);
	if (!rh.matchPipeline(TORASU_STD_PL_VIS)) {
		return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_SEGMENT);
	}

	torasu::tstd::Dbimg_FORMAT* bimgFormat;
	if (!(bimgFormat = rh.getFormat<torasu::tstd::Dbimg_FORMAT>())) {
		return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_FORMAT);
	}
	uint32_t rWidth = bimgFormat->getWidth();
	uint32_t rHeight = bimgFormat->getHeight();

	// Creation of Requests
	torasu::tools::ResultSettingsSingleFmt rsVis(TORASU_STD_PL_VIS, bimgFormat);
	auto srcRndId = rh.enqueueRender(rSrc, &rsVis);

	torasu::ResultSettings rsNum(TORASU_STD_PL_NUM, torasu::tools::NO_FORMAT);
	auto gainRndId = rh.enqueueRender(rGainVal, &rsNum);

	// Fetching of requests

	std::unique_ptr<torasu::RenderResult> srcRndResult(rh.fetchRenderResult(srcRndId));
	std::unique_ptr<torasu::RenderResult> gainRndResult(rh.fetchRenderResult(gainRndId));

	auto srcRes = rh.evalResult<torasu::tstd::Dbimg>(srcRndResult.get());
	auto gainRes = rh.evalResult<torasu::tstd::Dnum>(gainRndResult.get());

	torasu::tstd::Dbimg* srcImg = srcRes.getResult();

	if (srcImg == NULL) {
		return new torasu::RenderResult(torasu::RenderResultStatus_OK_WARN, new torasu::tstd::Dbimg(rWidth, rHeight), true);
	}

	if (gainRes.getResult() == NULL) {
		return new torasu::RenderResult(torasu::RenderResultStatus_OK_WARN, new torasu::tstd::Dbimg(*srcImg), true);
	}

	double gainVal = gainRes.getResult()->getNum();

	if (gainVal == 1) {
		return new torasu::RenderResult(torasu::RenderResultStatus_OK, new torasu::tstd::Dbimg(*srcImg), true);
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

	return new torasu::RenderResult(torasu::RenderResultStatus_OK, destImg, true);

}

torasu::ElementMap Rgain::getElements() {
	torasu::ElementMap elems;
	elems["src"] = rSrc;
	elems["val"] = rGainVal;
	return elems;
}

const torasu::OptElementSlot Rgain::setElement(std::string key, const torasu::ElementSlot* elem) {
	if (key == "src") return torasu::tools::trySetRenderableSlot(&rSrc, elem);
	if (key == "val") return torasu::tools::trySetRenderableSlot(&rGainVal, elem);
	return nullptr;
}

} // namespace imgc