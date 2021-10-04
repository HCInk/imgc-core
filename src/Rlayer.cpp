#include "../include/torasu/mod/imgc/Rlayer.hpp"

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/property_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>

namespace imgc {

Rlayer::Rlayer(torasu::tools::RenderableSlot layers)
	: SimpleRenderable(false, true), layers(layers) {}

Rlayer::~Rlayer() {}

torasu::Identifier Rlayer::getType() {
	return "IMGC::RLAYER";
}

torasu::RenderResult* Rlayer::render(torasu::RenderInstruction* ri) {
	torasu::tools::RenderHelper rh(ri);

	if (rh.matchPipeline(TORASU_STD_PL_VIS)) {
		auto* fmt = rh.getFormat<torasu::tstd::Dbimg_FORMAT>();
		if (fmt == nullptr) {
			return rh.buildResult(torasu::RenderResultStatus_INVALID_FORMAT);
		}

		size_t layerCount = 1;
		{
			torasu::ResultSettings rsLen(TORASU_PROPERTY(TORASU_STD_PROP_IT_LENGTH), torasu::tools::NO_FORMAT);
			std::unique_ptr<torasu::RenderResult> rrLen(rh.runRender(layers, &rsLen));
			auto resLen = rh.evalResult<torasu::tstd::Dnum>(rrLen.get());
			if (resLen) {
				layerCount = std::round(resLen.getResult()->getNum());
			} else {
				rh.lrib.hasError = true;
				if (rh.mayLog(torasu::WARN)) {
					rh.lrib.logCause(torasu::WARN, "Failed to get count of layers, will get first layer", resLen.takeInfoTag());
				}
			}
		}

		if (layerCount <= 0) {
			return rh.buildResult(torasu::RenderResultStatus_INVALID_SEGMENT);
		}

		torasu::tstd::Dbimg_FORMAT imgFmt(fmt->getWidth(), fmt->getHeight());
		torasu::tools::ResultSettingsSingleFmt visFmt(TORASU_STD_PL_VIS, &imgFmt);
		std::vector<torasu::RenderContext> rctxBuffer(layerCount, *rh.rctx);
		std::vector<torasu::tstd::Dnum> numBuffer(layerCount);
		std::vector<uint64_t> rids(layerCount);

		for (int64_t layerIndex = layerCount-1; layerIndex >= 0; layerIndex--) {
			auto& number = numBuffer[layerIndex];
			number = layerIndex;
			auto& rctx = rctxBuffer[layerIndex];
			rctx[TORASU_STD_CTX_IT] = &number;
			rids[layerIndex] = rh.enqueueRender(layers, &visFmt, &rctx);
		}
		std::unique_ptr<torasu::tstd::Dbimg> resultImg;
		uint8_t* imgData;
		size_t pixelCount;

		for (int64_t layerIndex = layerCount-1; layerIndex >= 0; layerIndex--) {
			std::unique_ptr<torasu::RenderResult> rr(rh.fetchRenderResult(rids[layerIndex]));
			if (rr->getStatus() == torasu::RenderResultStatus_INVALID_SEGMENT) {
				continue;
			}
			auto res = rh.evalResult<torasu::tstd::Dbimg>(rr.get());
			if (!res) {
				if (rh.mayLog(torasu::WARN)) {
					rh.lrib.logCause(torasu::WARN,
									 "Result for index " + std::to_string(layerIndex) + " did not provide an result!",
									 res.takeInfoTag());
				}
				rh.lrib.hasError = true;
				continue;
			}
			if (!resultImg) {
				resultImg = std::unique_ptr<torasu::tstd::Dbimg>(res.ejectOrCloneResult());
				imgData = resultImg->getImageData();
				pixelCount = resultImg->getWidth()*resultImg->getHeight();
				continue;
			}

			uint8_t* bottomLayerData = imgData;
			uint8_t* topLayerData = res.getResult()->getImageData();

			for (int64_t i = pixelCount*4-1; i >= 3; ) {
				// ALPHA
				uint16_t alpha = topLayerData[i];
				uint16_t alphaInv = 0xFF - alpha;
				bottomLayerData[i] = 0xFF - alphaInv*(0xFF-bottomLayerData[i])/0xFF;
				i--;

				// BLUE / GREEN / RED
				for (size_t c = 0; c < 3; c++) {
					bottomLayerData[i] = (alpha*topLayerData[i] + alphaInv*bottomLayerData[i]) / 0xFF;
					i--;
				}
			}

		}

		return rh.buildResult(resultImg.release());
	}

	return rh.passRender(layers.get(), torasu::tools::RenderHelper::PassMode_DEFAULT);
}

torasu::ElementMap Rlayer::getElements() {
	torasu::ElementMap map;
	map["layers"] = layers.get();
	return map;
}

void Rlayer::setElement(std::string key, Element* elem) {
	if (torasu::tools::trySetRenderableSlot("layers", &layers, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}


} // namespace imgc
