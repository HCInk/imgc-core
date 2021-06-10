#include "../include/torasu/mod/imgc/Rmedia_creator.hpp"

#include <memory>
#include <string>

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>
#include <torasu/std/Dstring.hpp>
#include <torasu/std/Dstring_map.hpp>

#include <torasu/mod/imgc/MediaEncoder.hpp>

namespace imgc {


Rmedia_creator::Rmedia_creator(torasu::tools::RenderableSlot src, torasu::tstd::StringSlot format,
							   torasu::tstd::NumSlot begin, torasu::tstd::NumSlot end, torasu::tstd::NumSlot fps,
							   torasu::tstd::NumSlot width, torasu::tstd::NumSlot height,
							   torasu::tstd::NumSlot videoBitrate, torasu::tstd::NumSlot audioMinSampleRate,
							   torasu::tools::RenderableSlot metadata)
	: SimpleRenderable("IMGC::RMEDIA_CREATOR", false, true),
	  srcRnd(src), formatRnd(format), beginRnd(begin), endRnd(end), fpsRnd(fps),
	  widthRnd(width), heightRnd(height),
	  videoBitrateRnd(videoBitrate), audioMinSampleRateRnd(audioMinSampleRate),
	  metadataSlot(metadata) {}

Rmedia_creator::~Rmedia_creator() {}

torasu::ResultSegment* Rmedia_creator::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {
	std::string pipeline = resSettings->getPipeline();
	if (pipeline == TORASU_STD_PL_FILE) {

		auto* ei = ri->getExecutionInterface();
		auto li = ri->getLogInstruction();
		auto* rctx = ri->getRenderContext();

		std::unique_ptr<torasu::RenderResult> rrMetadata;
		std::map<std::string, std::string> metadata;

		MediaEncoder::EncodeRequest req;

		bool logProgress = li.options & torasu::LogInstruction::OPT_PROGRESS; 
		if (logProgress) li.logger->log(new torasu::LogProgress(-1, 0, "Loading video-params..."));

		{
			bool doMetadata = metadataSlot.get() != nullptr;
			torasu::tools::RenderInstructionBuilder textRib;
			auto textHandle = textRib.addSegmentWithHandle<torasu::tstd::Dstring>(TORASU_STD_PL_STRING, nullptr);
			torasu::tools::RenderInstructionBuilder numRib;
			auto numHandle = numRib.addSegmentWithHandle<torasu::tstd::Dnum>(TORASU_STD_PL_NUM, nullptr);
			torasu::tools::RenderInstructionBuilder metadataRib;
			auto metdadataHandle = metadataRib.addSegmentWithHandle<torasu::tstd::Dstring_map>(TORASU_STD_PL_MAP, nullptr);

			auto formatRid = textRib.enqueueRender(formatRnd, rctx, ei, li);
			auto beginRid = numRib.enqueueRender(beginRnd, rctx, ei, li);
			auto endRid = numRib.enqueueRender(endRnd, rctx, ei, li);
			auto fpsRid = numRib.enqueueRender(fpsRnd, rctx, ei, li);
			auto widthRid = numRib.enqueueRender(widthRnd, rctx, ei, li);
			auto heightRid = numRib.enqueueRender(heightRnd, rctx, ei, li);
			auto vbrRid = numRib.enqueueRender(videoBitrateRnd, rctx, ei, li);
			auto amsrRid = numRib.enqueueRender(audioMinSampleRateRnd, rctx, ei, li);
			auto metaRid = doMetadata ? metadataRib.enqueueRender(metadataSlot, rctx, ei, li) : 0;

			std::unique_ptr<torasu::RenderResult> rrFormat(ei->fetchRenderResult(formatRid));
			std::unique_ptr<torasu::RenderResult> rrBegin(ei->fetchRenderResult(beginRid));
			std::unique_ptr<torasu::RenderResult> rrEnd(ei->fetchRenderResult(endRid));
			std::unique_ptr<torasu::RenderResult> rrFps(ei->fetchRenderResult(fpsRid));
			std::unique_ptr<torasu::RenderResult> rrWidth(ei->fetchRenderResult(widthRid));
			std::unique_ptr<torasu::RenderResult> rrHeight(ei->fetchRenderResult(heightRid));
			std::unique_ptr<torasu::RenderResult> rrVbr(ei->fetchRenderResult(vbrRid));
			std::unique_ptr<torasu::RenderResult> rrAmsr(ei->fetchRenderResult(amsrRid));
			if (doMetadata) rrMetadata = std::unique_ptr<torasu::RenderResult>(ei->fetchRenderResult(metaRid));

			auto* dFormat = textHandle.getFrom(rrFormat.get()).getResult();
			auto* dBegin = numHandle.getFrom(rrBegin.get()).getResult();
			auto* dEnd = numHandle.getFrom(rrEnd.get()).getResult();
			auto* dFps = numHandle.getFrom(rrFps.get()).getResult();
			auto* dWidth = numHandle.getFrom(rrWidth.get()).getResult();
			auto* dHeight = numHandle.getFrom(rrHeight.get()).getResult();
			auto* dVbr = numHandle.getFrom(rrVbr.get()).getResult();
			auto* dAmsr = numHandle.getFrom(rrAmsr.get()).getResult();
			auto* dMeta = doMetadata ? metdadataHandle.getFrom(rrMetadata.get()).getResult() : nullptr;

			if (!dFormat) throw std::runtime_error("Failed to retrieve format-key");
			if (!dBegin) throw std::runtime_error("Failed to retrieve begin-timestamp");
			if (!dEnd) throw std::runtime_error("Failed to retrieve end-timestamp");
			if (!dFps) throw std::runtime_error("Failed to retrieve fps");
			if (!dAmsr) throw std::runtime_error("Failed to retrieve minimum-samplerate for audio");

			req.formatName = dFormat->getString();
			req.begin = dBegin->getNum();
			req.end = dEnd->getNum();

			req.framerate = dFps->getNum();
			if (req.framerate > 0) {
				req.doVideo = true;

				if (!dWidth) throw std::runtime_error("Failed to retrieve width");
				if (!dHeight) throw std::runtime_error("Failed to retrieve height");
				if (!dVbr) throw std::runtime_error("Failed to retrieve video-bitrate");

				req.width = dWidth->getNum();
				req.height = dHeight->getNum();
				req.videoBitrate = dVbr->getNum();
			}


			req.minSampleRate = dAmsr->getNum();

			if (req.minSampleRate >= 0) {
				req.doAudio = true;
			}

			if (doMetadata) {
				if (!dMeta) throw std::runtime_error("Failed to retrieve metadata!");
				req.metadata = &dMeta->getMap(); // dMeta, shall be kept until encode is finished
			}
		}

		torasu::tstd::Dnum frameRatio((double) req.width / req.height);

		torasu::tools::RenderInstructionBuilder visRib;
		torasu::tstd::Dbimg_FORMAT visFmt(req.width, req.height);
		auto visHandle = visRib.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, &visFmt);

		size_t countPending = 0;

		auto* visHandleRef = &visHandle;
		auto* countPendingRef = &countPending;

		MediaEncoder enc([this, ei, li, rctx, &visRib, visHandleRef, &frameRatio, countPendingRef]
		(MediaEncoder::FrameRequest* fr) {
			if (*countPendingRef > 10 && !fr->needsNow) {
				return 1; // Postpone
			}
			(*countPendingRef)++;
			if (li.level <= torasu::LogLevel::DEBUG) li.logger->log(torasu::LogLevel::DEBUG, "Sym-req counter: " + std::to_string(*countPendingRef));

			auto* modRctx = new torasu::RenderContext(*rctx);
			if (auto* vidReq = dynamic_cast<MediaEncoder::VideoFrameRequest*>(fr)) {
				auto* timeRctxVal = new torasu::tstd::Dnum(vidReq->getTime());
				auto* durRctxVal = new torasu::tstd::Dnum(vidReq->getDuration());
				(*modRctx)[TORASU_STD_CTX_TIME] = timeRctxVal;
				(*modRctx)[TORASU_STD_CTX_DURATION] = durRctxVal;
				(*modRctx)[TORASU_STD_CTX_IMG_RATIO] = &frameRatio;

				auto rid = visRib.enqueueRender(srcRnd.get(), modRctx, ei, li);

				fr->setFinish([rid, ei, visHandleRef, vidReq, modRctx, timeRctxVal, durRctxVal, countPendingRef] {
					(*countPendingRef)--;

					auto* rr = ei->fetchRenderResult(rid);
					delete modRctx;
					delete timeRctxVal;
					delete durRctxVal;
					auto fetchedRes = visHandleRef->getFrom(rr);

					vidReq->setFree([rr]() {
						delete rr;
					});

					if (fetchedRes.getResult() == nullptr) return -1;
					vidReq->setResult(fetchedRes.getResult());

					return 0;
				});

				return 0;
			}
			if (auto* audReq = dynamic_cast<MediaEncoder::AudioFrameRequest*>(fr)) {
				auto* timeRctxVal = new torasu::tstd::Dnum(audReq->getStart());
				(*modRctx)[TORASU_STD_CTX_TIME] = timeRctxVal;
				auto* durRctxVal = new torasu::tstd::Dnum(audReq->getDuration());
				(*modRctx)[TORASU_STD_CTX_DURATION] = durRctxVal;

				auto* audRib = new torasu::tools::RenderInstructionBuilder();
				auto* audHandle = new auto(audRib->addSegmentWithHandle<torasu::tstd::Daudio_buffer>(TORASU_STD_PL_AUDIO, audReq->getFormat()));

				auto rid = audRib->enqueueRender(srcRnd.get(), modRctx, ei, li);

				fr->setFinish([rid, ei, audRib, audHandle, audReq, modRctx, timeRctxVal, durRctxVal, countPendingRef] {
					(*countPendingRef)--;

					auto* rr = ei->fetchRenderResult(rid);
					delete modRctx;
					delete timeRctxVal;
					delete durRctxVal;
					auto fetchedRes = audHandle->getFrom(rr);
					delete audHandle;
					delete audRib;

					audReq->setFree([rr]() {
						delete rr;
					});

					if (fetchedRes.getResult() == nullptr) return -1;
					audReq->setResult(fetchedRes.getResult());
					return 0;
				});

				return 0;
			}
			return -3;
		}
						);


		auto* result = enc.encode(req, li);

		return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, result, true);
	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}
}

torasu::ElementMap Rmedia_creator::getElements() {
	torasu::ElementMap elems;
	elems["src"] = srcRnd.get();
	elems["begin"] = beginRnd.get();
	elems["end"] = endRnd.get();
	elems["fps"] = fpsRnd.get();
	elems["width"] = widthRnd.get();
	elems["height"] = heightRnd.get();
	elems["vbr"] = videoBitrateRnd.get();
	elems["amsr"] = audioMinSampleRateRnd.get();
	elems["meta"] = metadataSlot.get();
	return elems;
}

void Rmedia_creator::setElement(std::string key, torasu::Element* elem) {
	if (torasu::tools::trySetRenderableSlot("src", &srcRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("begin", &beginRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("end", &endRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("fps", &fpsRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("width", &widthRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("height", &heightRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("vbr", &videoBitrateRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("amsr", &audioMinSampleRateRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("meta", &metadataSlot, true, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

} // namespace imgc
