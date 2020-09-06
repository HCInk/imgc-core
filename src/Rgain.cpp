#include "../include/torasu/mod/imgc/Rgain.hpp"

#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>

namespace imgc {

Rgain::Rgain(Renderable* src, Renderable* gainVal)
	: SimpleRenderable("IMGC::RGAIN", false, true, false),
	  rSrc(src), rGainVal(gainVal) {}

Rgain::~Rgain() {}

torasu::ResultSegment* Rgain::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {
	if (resSettings->getPipeline() != visPipeline) {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}

	auto* format = resSettings->getResultFormatSettings();
	uint32_t rWidth, rHeight;
	if (format->getFormat() == "STD::DBIMG") {
		torasu::DataResource* fmtData = format->getData();
		torasu::tstd::Dbimg_FORMAT* bimgFormat;
		if (!(bimgFormat = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(fmtData))) {
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
		}
		rWidth = bimgFormat->getWidth();
		rHeight = bimgFormat->getHeight();
	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
	}

	torasu::ExecutionInterface* ei = ri->getExecutionInterface();
	torasu::RenderContext* rctx = ri->getRenderContext();

	// Creation of Requests

	torasu::tools::RenderInstructionBuilder ribSrc;
	auto srcHandle = ribSrc.addSegmentWithHandle<torasu::tstd::Dbimg>(visPipeline, format);
	auto srcRndId = ribSrc.enqueueRender(rSrc, rctx, ei);

	torasu::tools::RenderInstructionBuilder ribGain;
	auto gainHandle = ribGain.addSegmentWithHandle<torasu::tstd::Dnum>(numPipeline, NULL);
	auto gainRndId = ribGain.enqueueRender(rGainVal, rctx, ei);

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

std::map<std::string, torasu::Element*> Rgain::getElements() {
	std::map<std::string, Element*> elems;
	elems["src"] = rSrc;
	elems["val"] = rGainVal;
	return elems;
}

void Rgain::setElement(std::string key, Element* elem) {

	if (key == "src") {

		if (elem == NULL) {
			throw std::invalid_argument("Element slot \"src\" may not be empty!");
		}
		if (Renderable* rnd = dynamic_cast<Renderable*>(elem)) {
			rSrc = rnd;
			return;
		} else {
			throw std::invalid_argument("Element slot \"src\" only accepts Renderables!");
		}

	} else if (key == "val") {

		if (elem == NULL) {
			throw std::invalid_argument("Element slot \"val\" may not be empty!");
		}
		if (Renderable* rnd = dynamic_cast<Renderable*>(elem)) {
			rGainVal = rnd;
			return;
		} else {
			throw std::invalid_argument("Element slot \"val\" only accepts Renderables!");
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