#include "../include/torasu/mod/imgc/Rlayer.hpp"

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/property_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>

namespace imgc {

Rlayer::Rlayer(torasu::RenderableSlot layers)
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
		uint32_t destWidth = fmt->getWidth();
		uint32_t destHeight = fmt->getHeight();
		torasu::tstd::Dbimg_FORMAT imgFmt(destWidth, destHeight, new torasu::tstd::Dbimg::CropInfo());
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

			torasu::tstd::Dbimg* nextLayerImg = res.getResult();

			if (!resultImg) {
				if (nextLayerImg->getCropInfo() == nullptr ||
						(nextLayerImg->getCropInfo()->left == 0 && nextLayerImg->getCropInfo()->right == 0 &&
						 nextLayerImg->getCropInfo()->top == 0 && nextLayerImg->getCropInfo()->bottom == 0)) {
					resultImg = std::unique_ptr<torasu::tstd::Dbimg>(res.ejectOrCloneResult());
					imgData = resultImg->getImageData();
					continue;
				} else {
					resultImg = std::unique_ptr<torasu::tstd::Dbimg>(new torasu::tstd::Dbimg(destWidth, destHeight));
					resultImg->clear();
					imgData = resultImg->getImageData();
				}
			}


			uint8_t* bottomLayerData = imgData;
			uint8_t* topLayerData = nextLayerImg->getImageData();
			uint32_t srcWidth = nextLayerImg->getWidth();
			uint32_t srcHeight = nextLayerImg->getHeight();
			uint32_t offRight, offBottom;
			if (const torasu::tstd::Dbimg::CropInfo* cropInfo = nextLayerImg->getCropInfo()) {
				offRight = cropInfo->right;
				offBottom = cropInfo->bottom;
			} else {
				offRight = 0;
				offBottom = 0;
			}

			uint32_t lineSkip = (destWidth-srcWidth)*4;
			size_t iSrc = (srcWidth*srcHeight)*4-1;
			size_t iDest = (destWidth*destHeight-(destWidth*offBottom+offRight))*4-1;
			for (uint32_t y = 0; y < srcHeight; y++) {
				for (uint32_t x = 0; x < srcWidth; x++) {
					// ALPHA

					uint16_t alpha = topLayerData[iSrc];
					uint16_t alphaInv = 0xFF - alpha;
					bottomLayerData[iDest] = 0xFF - alphaInv*(0xFF-bottomLayerData[iDest])/0xFF;
					iDest--;
					iSrc--;

					// BLUE / GREEN / RED
					for (size_t c = 0; c < 3; c++) {
						bottomLayerData[iDest] = (alpha*topLayerData[iSrc] + alphaInv*bottomLayerData[iDest]) / 0xFF;
						iDest--;
						iSrc--;
					}
				}
				iDest -= lineSkip;
			}

		}

		return rh.buildResult(resultImg.release());
	}

	return rh.passRender(layers.get(), torasu::tools::RenderHelper::PassMode_DEFAULT);
}

torasu::ElementMap Rlayer::getElements() {
	torasu::ElementMap map;
	map["layers"] = layers;
	return map;
}

const torasu::OptElementSlot Rlayer::setElement(std::string key, const torasu::ElementSlot* elem) {
	if (key == "layers") return torasu::tools::trySetRenderableSlot(&layers, elem);
	return nullptr;
}


} // namespace imgc
