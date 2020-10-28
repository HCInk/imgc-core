#include "../include/torasu/mod/imgc/Rmedia_creator.hpp"

#include <memory>
#include <string>

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>

#include <torasu/mod/imgc/MediaEncoder.hpp>

namespace imgc {

Rmedia_creator::Rmedia_creator(torasu::Renderable* srcRnd)
	: SimpleRenderable("IMGC::RMEDIA_CREATOR", /* true */false, true),
	  srcRnd(srcRnd) {}


Rmedia_creator::~Rmedia_creator() {
	// delete data;
}

torasu::ResultSegment* Rmedia_creator::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {
	std::string pipeline = resSettings->getPipeline();
	if (pipeline == TORASU_STD_PL_FILE) {

		auto* ei = ri->getExecutionInterface();
		auto* rctx = ri->getRenderContext();

		MediaEncoder::EncodeRequest req;
		
		req.formatName = "mp4";
		req.end = 10;

		req.doVideo = true;
		req.videoBitrate = 4000 * 1000;
		req.width = 1920;
		req.height = 1080;
		req.framerate = 25;

		torasu::tstd::Dnum frameRatio((double) req.width / req.height);
		torasu::tstd::Dnum frameDuration(0);

		torasu::tools::RenderInstructionBuilder visRib;
		torasu::tstd::Dbimg_FORMAT visFmt(req.width, req.height);
		auto visHandle = visRib.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, &visFmt);

		MediaEncoder enc([this, ei, rctx, &visRib, &visHandle, &frameDuration, &frameRatio]
			(MediaEncoder::FrameRequest* fr) {
				if (auto* vidReq = dynamic_cast<MediaEncoder::VideoFrameRequest*>(fr)) {
					torasu::RenderContext modRctx = *rctx;
					torasu::tstd::Dnum time(vidReq->getTime());
					modRctx[TORASU_STD_CTX_TIME] = &time;
					modRctx[TORASU_STD_CTX_DURATION] = &frameDuration;
					modRctx[TORASU_STD_CTX_IMG_RATIO] = &frameRatio;

					auto* rr = visRib.runRender(srcRnd, &modRctx, ei);

					fr->setFree([rr]() {
						delete rr;
					});

					auto fetchedRes = visHandle.getFrom(rr);

					if (fetchedRes.getResult() == nullptr) return -1;

					vidReq->setResult(fetchedRes.getResult());

					return 0;
				}
				return -3;
			}
		);


		auto* result = enc.encode(req);

		return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, result, true);
	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}
}

torasu::ElementMap Rmedia_creator::getElements() {
	torasu::ElementMap elems;
	elems["src"] = srcRnd;
	return elems;
}

void Rmedia_creator::setElement(std::string key, torasu::Element* elem) {
	if (torasu::tools::trySetRenderableSlot("src", &srcRnd, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

/* torasu::DataResource* Rmedia_creator::getData() {
	return data;
} */

/* void Rmedia_creator::setData(torasu::DataResource* data) {
	if (auto* castedData = dynamic_cast<Dboilerplate*>(data)) {
		delete this->data;
		this->data = castedData;
	} else {
		throw std::invalid_argument("The data-type \"Dboilerplate\" is only allowed");
	}
} */

} // namespace imgc
