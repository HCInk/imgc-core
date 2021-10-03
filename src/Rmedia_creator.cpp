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
	: SimpleRenderable(false, true),
	  srcRnd(src), formatRnd(format), beginRnd(begin), endRnd(end), fpsRnd(fps),
	  widthRnd(width), heightRnd(height),
	  videoBitrateRnd(videoBitrate), audioMinSampleRateRnd(audioMinSampleRate),
	  metadataSlot(metadata) {}

Rmedia_creator::~Rmedia_creator() {}

torasu::Identifier Rmedia_creator::getType() {
	return "IMGC::RMEDIA_CREATOR";
}

torasu::RenderResult* Rmedia_creator::render(torasu::RenderInstruction* ri) {
	if (ri->getResultSettings()->getPipeline() == TORASU_STD_PL_FILE) {
		torasu::tools::RenderHelper rh(ri);

		std::unique_ptr<torasu::RenderResult> rrMetadata;
		std::map<std::string, std::string> metadata;

		MediaEncoder::EncodeRequest req;

		bool logProgress = rh.li.options & torasu::LogInstruction::OPT_PROGRESS;
		if (logProgress) rh.li.logger->log(new torasu::LogProgress(-1, 0, "Loading video-params..."));

		{
			bool doMetadata = metadataSlot.get() != nullptr;
			torasu::ResultSettings textSettings(TORASU_STD_PL_STRING, torasu::tools::NO_FORMAT);
			torasu::ResultSettings numSettings(TORASU_STD_PL_NUM, torasu::tools::NO_FORMAT);
			torasu::ResultSettings metadataSettings(TORASU_STD_PL_MAP, torasu::tools::NO_FORMAT);

			auto formatRid = rh.enqueueRender(formatRnd, &textSettings);
			auto beginRid = rh.enqueueRender(beginRnd, &numSettings);
			auto endRid = rh.enqueueRender(endRnd, &numSettings);
			auto fpsRid = rh.enqueueRender(fpsRnd, &numSettings);
			auto widthRid = rh.enqueueRender(widthRnd, &numSettings);
			auto heightRid = rh.enqueueRender(heightRnd, &numSettings);
			auto vbrRid = rh.enqueueRender(videoBitrateRnd, &numSettings);
			auto amsrRid = rh.enqueueRender(audioMinSampleRateRnd, &numSettings);
			auto metaRid = doMetadata ? rh.enqueueRender(metadataSlot, &metadataSettings) : 0;

			std::unique_ptr<torasu::RenderResult> rrFormat(rh.fetchRenderResult(formatRid));
			std::unique_ptr<torasu::RenderResult> rrBegin(rh.fetchRenderResult(beginRid));
			std::unique_ptr<torasu::RenderResult> rrEnd(rh.fetchRenderResult(endRid));
			std::unique_ptr<torasu::RenderResult> rrFps(rh.fetchRenderResult(fpsRid));
			std::unique_ptr<torasu::RenderResult> rrWidth(rh.fetchRenderResult(widthRid));
			std::unique_ptr<torasu::RenderResult> rrHeight(rh.fetchRenderResult(heightRid));
			std::unique_ptr<torasu::RenderResult> rrVbr(rh.fetchRenderResult(vbrRid));
			std::unique_ptr<torasu::RenderResult> rrAmsr(rh.fetchRenderResult(amsrRid));
			if (doMetadata) rrMetadata = std::unique_ptr<torasu::RenderResult>(rh.fetchRenderResult(metaRid));

			auto* dFormat = rh.evalResult<torasu::tstd::Dstring>(rrFormat.get()).getResult();
			auto* dBegin = rh.evalResult<torasu::tstd::Dnum>(rrBegin.get()).getResult();
			auto* dEnd = rh.evalResult<torasu::tstd::Dnum>(rrEnd.get()).getResult();
			auto* dFps = rh.evalResult<torasu::tstd::Dnum>(rrFps.get()).getResult();
			auto* dWidth = rh.evalResult<torasu::tstd::Dnum>(rrWidth.get()).getResult();
			auto* dHeight = rh.evalResult<torasu::tstd::Dnum>(rrHeight.get()).getResult();
			auto* dVbr = rh.evalResult<torasu::tstd::Dnum>(rrVbr.get()).getResult();
			auto* dAmsr = rh.evalResult<torasu::tstd::Dnum>(rrAmsr.get()).getResult();
			auto* dMeta = doMetadata ? rh.evalResult<torasu::tstd::Dstring_map>(rrMetadata.get()).getResult() : nullptr;

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

		torasu::tstd::Dbimg_FORMAT visFmt(req.width, req.height);
		torasu::tools::ResultSettingsSingleFmt visSettings(TORASU_STD_PL_VIS, &visFmt);

		size_t countPending = 0;

		auto* countPendingRef = &countPending;

		MediaEncoder enc([this, &rh, &visSettings, &frameRatio, countPendingRef]
		(MediaEncoder::FrameRequest* fr) {
			if (*countPendingRef > 10 && !fr->needsNow) {
				return 1; // Postpone
			}
			(*countPendingRef)++;
			if (rh.mayLog(torasu::DEBUG)) rh.li.logger->log(torasu::LogLevel::DEBUG, "Sym-req counter: " + std::to_string(*countPendingRef));

			auto* modRctx = new torasu::RenderContext(*rh.rctx);
			if (auto* vidReq = dynamic_cast<MediaEncoder::VideoFrameRequest*>(fr)) {
				auto* timeRctxVal = new torasu::tstd::Dnum(vidReq->getTime());
				auto* durRctxVal = new torasu::tstd::Dnum(vidReq->getDuration());
				(*modRctx)[TORASU_STD_CTX_TIME] = timeRctxVal;
				(*modRctx)[TORASU_STD_CTX_DURATION] = durRctxVal;
				(*modRctx)[TORASU_STD_CTX_IMG_RATIO] = &frameRatio;

				auto rid = rh.enqueueRender(srcRnd.get(), &visSettings, modRctx);

				fr->setFinish([rid, &rh, vidReq, modRctx, timeRctxVal, durRctxVal, countPendingRef] {
					(*countPendingRef)--;

					auto* rr = rh.fetchRenderResult(rid);
					delete modRctx;
					delete timeRctxVal;
					delete durRctxVal;
					auto fetchedRes = rh.evalResult<torasu::tstd::Dbimg>(rr);

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

				auto* audSettings = new torasu::tools::ResultSettingsSingleFmt(TORASU_STD_PL_AUDIO, audReq->getFormat());

				auto rid = rh.enqueueRender(srcRnd.get(), audSettings, modRctx);

				fr->setFinish([rid, &rh, audSettings, audReq, modRctx, timeRctxVal, durRctxVal, countPendingRef] {
					(*countPendingRef)--;

					auto* rr = rh.fetchRenderResult(rid);
					delete modRctx;
					delete timeRctxVal;
					delete durRctxVal;
					delete audSettings;

					auto fetchedRes = rh.evalResult<torasu::tstd::Daudio_buffer>(rr);

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


		auto* result = enc.encode(req, rh.li);

		return new torasu::RenderResult(torasu::RenderResultStatus_OK, result, true);
	} else {
		return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_SEGMENT);
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
