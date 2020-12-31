#include "../include/torasu/mod/imgc/Rgain.hpp"

#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>

namespace imgc {

Rgain::Rgain(torasu::tools::RenderableSlot src, torasu::tstd::NumSlot gainVal)
	: SimpleRenderable("IMGC::RGAIN", false, true),
	  rSrc(src), rGainVal(gainVal) {}

Rgain::~Rgain() {}

torasu::ResultSegment* Rgain::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {
	if (resSettings->getPipeline() != visPipeline) {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}

	auto* format = resSettings->getResultFormatSettings();
	uint32_t rWidth, rHeight;
	if (auto* bimgFormat = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(format)) {
		rWidth = bimgFormat->getWidth();
		rHeight = bimgFormat->getHeight();
	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
	}

	torasu::ExecutionInterface* ei = ri->getExecutionInterface();
	torasu::LogInstruction li = ri->getLogInstruction();
	torasu::RenderContext* rctx = ri->getRenderContext();

	// Creation of Requests

	torasu::tools::RenderInstructionBuilder ribSrc;
	auto srcHandle = ribSrc.addSegmentWithHandle<torasu::tstd::Dbimg>(visPipeline, format);
	auto srcRndId = ribSrc.enqueueRender(rSrc, rctx, ei, li);

	torasu::tools::RenderInstructionBuilder ribGain;
	auto gainHandle = ribGain.addSegmentWithHandle<torasu::tstd::Dnum>(numPipeline, NULL);
	auto gainRndId = ribGain.enqueueRender(rGainVal, rctx, ei, li);

	// Fetching of requests

	std::unique_ptr<torasu::RenderResult> srcRndResult(ei->fetchRenderResult(srcRndId));
	std::unique_ptr<torasu::RenderResult> gainRndResult(ei->fetchRenderResult(gainRndId));

	auto srcRes = srcHandle.getFrom(srcRndResult.get());
	auto gainRes = gainHandle.getFrom(gainRndResult.get());

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