#include "../include/torasu/mod/imgc/Ralign2d.hpp"

#include <chrono>
#include <cmath>
#include <stdexcept>

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/Dnum.hpp>
#include <torasu/std/Dbimg.hpp>

#include <torasu/mod/imgc/pipeline_names.hpp>
#include <torasu/mod/imgc/Dcropdata.hpp>

namespace imgc {

Ralign2d::Ralign2d(Renderable* rndSrc, Renderable* rndAlign)
	: torasu::tools::SimpleRenderable("IMGC::RALIGN2D", false, true),
	  rndSrc(rndSrc),
	  rndAlign(rndAlign) {}

Ralign2d::~Ralign2d() {}

#define ROUND_PRECISION 40000

void Ralign2d::calcAlign(Renderable* alignmentProvider, torasu::ExecutionInterface* ei, torasu::RenderContext* rctx,
				   uint32_t destWidth, uint32_t destHeight,
				   Ralign2d_CROPDATA* outCropData) const {

	// Creating instruction to get alignment

	torasu::tools::RenderInstructionBuilder rib;
	auto handle = rib.addSegmentWithHandle<imgc::Dcropdata>(IMGC_PL_ALIGN, NULL);

	// Running render based on instruction

	torasu::RenderResult* rr = rib.runRender(alignmentProvider, rctx, ei);

	// Finding results

	auto result = handle.getFrom(rr);
	
	if (result.getResult() == nullptr) {
		throw std::runtime_error("Alignment calculation failed!");
	}

	imgc::Dcropdata* cropdata = result.getResult();

	outCropData->offLeft = std::round ( ((double) destWidth * cropdata->getOffLeft() )*ROUND_PRECISION )/ROUND_PRECISION;
	outCropData->offRight = std::round( ((double) destWidth * cropdata->getOffRight() )*ROUND_PRECISION )/ROUND_PRECISION;
	outCropData->offTop = std::round( ((double) destHeight * cropdata->getOffTop() )*ROUND_PRECISION )/ROUND_PRECISION;
	outCropData->offBottom = std::round( ((double) destHeight * cropdata->getOffBottom() )*ROUND_PRECISION )/ROUND_PRECISION;

	outCropData->srcWidth = destWidth - (outCropData->offLeft + outCropData->offRight);
	outCropData->srcHeight = destHeight - (outCropData->offTop + outCropData->offBottom);

	delete rr;
}


void Ralign2d::align(torasu::tstd::Dbimg* srcImg, torasu::tstd::Dbimg* destImg, Ralign2d_CROPDATA* cropData) const {

	uint8_t* const srcData = srcImg->getImageData();
	uint8_t* const destData = destImg->getImageData();

	const uint32_t srcWidth = srcImg->getWidth();
	const uint32_t srcHeight = srcImg->getHeight();
	const uint32_t destWidth = destImg->getWidth();
	const uint32_t destHeight = destImg->getHeight();
	const uint8_t channels = 4;

	const uint32_t srcCropLeft = cropData->offLeft<0? -cropData->offLeft:0;
	const uint32_t srcCropRight = cropData->offRight<0? -cropData->offRight:0;
	const uint32_t srcCropTop = cropData->offTop<0? -cropData->offTop:0;
	const uint32_t srcCropBottom = cropData->offBottom<0? -cropData->offBottom:0;

	const uint32_t destCropLeft = cropData->offLeft>0? cropData->offLeft:0;
	// const uint32_t destCropRight = cropData->offRight>0? cropData->offRight:0;
	const uint32_t destCropTop = cropData->offTop>0? cropData->offTop:0;
	// const uint32_t destCropBottom = cropData->offBottom>0? cropData->offBottom:0;

	const size_t copySize = ( srcWidth-(srcCropRight + srcCropLeft ) ) * channels;

	const size_t srcBegin = (srcCropTop*srcWidth + srcCropLeft) * channels;
	const size_t srcLineSize = srcWidth*channels;

	const size_t destBegin = (destCropTop*destWidth + destCropLeft) * channels;
	const size_t destLineSize = destWidth*channels;
	const size_t destSkipSize = destLineSize-copySize;
	const size_t destTotalSize = destLineSize*destHeight;

	uint8_t* currSrcData = srcData;
	uint8_t* currDestData = destData;

	currSrcData+=srcBegin;
	currDestData+=destBegin;

	std::fill(destData, currDestData, 0);

	uint32_t y = srcCropTop;
	while (true) {

		std::copy(currSrcData, currSrcData+copySize, currDestData);
		currDestData+=copySize;

		y++;
		if (y < srcHeight-srcCropBottom) {
			std::fill(currDestData, currDestData+destSkipSize, 0);
			currDestData+=destSkipSize;
			currSrcData+=srcLineSize;
			continue;
		} else {
			break;
		}
	}

	std::fill(currDestData, destData+destTotalSize, 0);

}


torasu::ResultSegment* Ralign2d::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {

	if (resSettings->getPipeline().compare(TORASU_STD_PL_VIS) == 0) {
		torasu::tstd::Dbimg_FORMAT* fmt;
		if ( !( resSettings->getResultFormatSettings() != NULL
				&& resSettings->getResultFormatSettings()->getFormat() == "STD::DBIMG"
				&& (fmt = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(resSettings->getResultFormatSettings()->getData())) )) {
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
		}

		auto ei = ri->getExecutionInterface();
		auto rctx = ri->getRenderContext();

		torasu::tools::RenderInstructionBuilder rib;

		Ralign2d_CROPDATA cropData;

		calcAlign(rndAlign, ei, rctx, fmt->getWidth(), fmt->getHeight(), &cropData);

		// Format creation

		torasu::tstd::Dbimg_FORMAT srcFmt(cropData.srcWidth, cropData.srcHeight);

		auto fmtHandle = srcFmt.asFormat();

		torasu::tools::RenderResultSegmentHandle<torasu::tstd::Dbimg> resHandle = rib.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, &fmtHandle);

		// Render-context modification

		torasu::RenderContext modRctx(*rctx); // Create copy of the render context
		torasu::tstd::Dnum ratio((double)cropData.srcWidth/cropData.srcHeight);
		modRctx[TORASU_STD_CTX_IMG_RATIO] = &ratio;

		// Sub-Renderings
		auto srcRndId = rib.enqueueRender(rndSrc, rctx, ei);

		torasu::RenderResult* srcRes = ei->fetchRenderResult(srcRndId);

		// Calculating Result from Results

		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dbimg> a = resHandle.getFrom(srcRes);

		torasu::tstd::Dbimg* result = NULL;

		if (a.getResult() != NULL) {

			result = new torasu::tstd::Dbimg(*fmt);

			auto benchBegin = std::chrono::steady_clock::now();

			align(a.getResult(), result, &cropData);

			auto benchEnd = std::chrono::steady_clock::now();
			std::cout << "  Align Time = " << std::chrono::duration_cast<std::chrono::milliseconds>(benchEnd - benchBegin).count() << "[ms] " << std::chrono::duration_cast<std::chrono::microseconds>(benchEnd - benchBegin).count() << "[us]" << std::endl;

		}

		delete srcRes;

		if (result != NULL) {
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, result, true);
		} else {
			torasu::tstd::Dbimg* errRes = new torasu::tstd::Dbimg(*fmt);
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK_WARN, errRes, true);
		}

	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}

}

std::map<std::string, torasu::Element*> Ralign2d::getElements() {
	std::map<std::string, torasu::Element*> elems;

	elems["src"] = rndSrc;
	elems["align"] = rndAlign;

	return elems;
}

void Ralign2d::setElement(std::string key, torasu::Element* elem) {

	if (torasu::tools::trySetRenderableSlot("key", &rndSrc, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("align", &rndAlign, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);

}

} // namespace imgc