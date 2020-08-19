#include "../include/torasu/mod/imgc/Ralign2d.hpp"

#include <chrono>

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dbimg.hpp>

namespace imgc {

Ralign2d::Ralign2d(Renderable* rndSrc, double posX, double posY, double zoomFactor, double imageRatio) 
	: torasu::tools::SimpleRenderable("IMGC::RALIGN2D"),
	rndSrc(rndSrc),
	posX(posX),
	posY(posY),
	zoomFactor(zoomFactor),
	imageRatio(imageRatio),
	autoRatio(false) {}

Ralign2d::~Ralign2d() {}

void Ralign2d::calcAlign(double posX, double posY, double zoomFactor, bool autoRatio, double imageRatio,
					uint32_t destWidth, uint32_t destHeight, 
					Ralign2d_CROPDATA& outCropdata) const {

	posX = 0.5 + posX / 2;
	posY = 0.5 + posY / 2;
	
	double ratio;
	if (autoRatio) {
		// TODO Ralign2d - AutoRatio 
		ratio = 1;
	} else {
		ratio = imageRatio;
	}

	double destRatio = (double) destWidth/destHeight;

	if (destRatio == imageRatio) {
		outCropdata.srcWidth = destWidth;
		outCropdata.srcHeight = destHeight;
		outCropdata.offLeft = 0;
		outCropdata.offRight = 0;
		outCropdata.offTop = 0;
		outCropdata.offBottom = 0;
	} else {
		uint32_t containWidth, containHeight, 
					coverWidth, coverHeight;
		if (destRatio > imageRatio) { // Destination more wide

			containWidth = destHeight*imageRatio;
			containHeight = destHeight;
			coverWidth = destWidth;
			coverHeight = destWidth/imageRatio;

		} else { // Destination more high

			containWidth = destWidth;
			containHeight = destWidth/imageRatio;
			coverWidth = destHeight*imageRatio;
			coverHeight = destHeight;

		}

		outCropdata.srcWidth = containWidth + (coverWidth-containWidth)*zoomFactor;
		outCropdata.srcHeight = containHeight + (coverHeight-containHeight)*zoomFactor;

		outCropdata.offLeft = destWidth-outCropdata.srcWidth;
		outCropdata.offTop = destHeight-outCropdata.srcHeight;

		outCropdata.offRight = outCropdata.offLeft*(1-posX);
		outCropdata.offBottom = outCropdata.offTop*(1-posY);
		
		outCropdata.offLeft -= outCropdata.offRight;
		outCropdata.offTop -= outCropdata.offBottom;

	}

}


void Ralign2d::align(torasu::tstd::Dbimg* srcImg, torasu::tstd::Dbimg* destImg, Ralign2d_CROPDATA* cropData) const {
	
	uint8_t*const srcData = srcImg->getImageData();
	uint8_t*const destData = destImg->getImageData();

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
	const uint32_t destCropRight = cropData->offRight>0? cropData->offRight:0;
	const uint32_t destCropTop = cropData->offTop>0? cropData->offTop:0;
	const uint32_t destCropBottom = cropData->offBottom>0? cropData->offBottom:0;

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

		torasu::tools::RenderInstructionBuilder rib;

		Ralign2d_CROPDATA cropData;

		calcAlign(posX, posY, zoomFactor, autoRatio, imageRatio, 
					fmt->getWidth(), fmt->getHeight(),
					cropData);

		torasu::tstd::Dbimg_FORMAT srcFmt(cropData.srcWidth, cropData.srcHeight);

		auto fmtHandle = srcFmt.asFormat();

		torasu::tools::RenderResultSegmentHandle<torasu::tstd::Dbimg> resHandle = rib.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, &fmtHandle);

		// Sub-Renderings
		auto ei = ri->getExecutionInterface();
		auto rctx = ri->getRenderContext();

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

} // namespace imgc