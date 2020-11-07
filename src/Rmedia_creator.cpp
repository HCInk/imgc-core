#include "../include/torasu/mod/imgc/Rmedia_creator.hpp"

#include <memory>
#include <string>

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>
#include <torasu/std/Dstring.hpp>

#include <torasu/std/Rnum.hpp>
#include <torasu/std/Rstring.hpp>

#include <torasu/mod/imgc/MediaEncoder.hpp>

namespace imgc {

#define CONFIGURE_OWNED(field, owns, def) field = def; owns = true;

#define CONFIGURE_DEFUALT(incooming, field, owns, def) \
	if (incooming) { \
		field = incooming; \
	} else { \
		CONFIGURE_OWNED(field, owns, def) \
	}

#define DEL_OWNED(field, owned) if (owned) delete field;

Rmedia_creator::Rmedia_creator(Renderable* src, Renderable* format, Renderable* begin, Renderable* end, 
	Renderable* fps, Renderable* width, Renderable* height, Renderable* videoBitrate, Renderable* audioMinSampleRate)
	: SimpleRenderable("IMGC::RMEDIA_CREATOR", false, true),
	  srcRnd(src), formatRnd(format), endRnd(end) {

	CONFIGURE_DEFUALT(begin, beginRnd, ownsBegin, new torasu::tstd::Rnum(0))
	CONFIGURE_DEFUALT(fps, fpsRnd, ownsFps, new torasu::tstd::Rnum(0))
	CONFIGURE_DEFUALT(width, widthRnd, ownsWidth, new torasu::tstd::Rnum(0))
	CONFIGURE_DEFUALT(height, heightRnd, ownsHeight, new torasu::tstd::Rnum(0))
	CONFIGURE_DEFUALT(videoBitrate, videoBitrateRnd, ownsVideoBitrate, new torasu::tstd::Rnum(0))
	CONFIGURE_DEFUALT(audioMinSampleRate, audioMinSampleRateRnd, ownsAudioMinSampleRate, new torasu::tstd::Rnum(0))

}

Rmedia_creator::Rmedia_creator(Renderable* src, std::string format, double begin, double end, 
	double fps, uint32_t width, uint32_t height, size_t videoBitrate, size_t audioMinSampleRate) 
	: SimpleRenderable("IMGC::RMEDIA_CREATOR", false, true),
	  srcRnd(src) {
	
	CONFIGURE_OWNED(formatRnd, ownsFormat, new torasu::tstd::Rstring(format))
	CONFIGURE_OWNED(beginRnd, ownsBegin, new torasu::tstd::Rnum(begin))
	CONFIGURE_OWNED(endRnd, ownsEnd, new torasu::tstd::Rnum(end))
	CONFIGURE_OWNED(fpsRnd, ownsFps, new torasu::tstd::Rnum(fps))
	CONFIGURE_OWNED(widthRnd, ownsWidth, new torasu::tstd::Rnum(width))
	CONFIGURE_OWNED(heightRnd, ownsHeight, new torasu::tstd::Rnum(height))
	CONFIGURE_OWNED(videoBitrateRnd, ownsVideoBitrate, new torasu::tstd::Rnum(videoBitrate))
	CONFIGURE_OWNED(audioMinSampleRateRnd, ownsAudioMinSampleRate, new torasu::tstd::Rnum(audioMinSampleRate))

}

Rmedia_creator::~Rmedia_creator() {
	DEL_OWNED(formatRnd, ownsFormat)
	DEL_OWNED(beginRnd, ownsBegin)
	DEL_OWNED(endRnd, ownsEnd)
	DEL_OWNED(fpsRnd, ownsFps)
	DEL_OWNED(widthRnd, ownsWidth)
	DEL_OWNED(heightRnd, ownsHeight)
	DEL_OWNED(videoBitrateRnd, ownsVideoBitrate)
	DEL_OWNED(audioMinSampleRateRnd, ownsAudioMinSampleRate)
}

torasu::ResultSegment* Rmedia_creator::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {
	std::string pipeline = resSettings->getPipeline();
	if (pipeline == TORASU_STD_PL_FILE) {

		auto* ei = ri->getExecutionInterface();
		auto* rctx = ri->getRenderContext();

		MediaEncoder::EncodeRequest req;
		
		{
			torasu::tools::RenderInstructionBuilder textRib;
			auto textHandle = textRib.addSegmentWithHandle<torasu::tstd::Dstring>(TORASU_STD_PL_STRING, nullptr);
			torasu::tools::RenderInstructionBuilder numRib;
			auto numHandle = numRib.addSegmentWithHandle<torasu::tstd::Dnum>(TORASU_STD_PL_NUM, nullptr);

			auto formatRid = textRib.enqueueRender(formatRnd, rctx, ei);
			auto beginRid = numRib.enqueueRender(beginRnd, rctx, ei);
			auto endRid = numRib.enqueueRender(endRnd, rctx, ei);
			auto fpsRid = numRib.enqueueRender(fpsRnd, rctx, ei);
			auto widthRid = numRib.enqueueRender(widthRnd, rctx, ei);
			auto heightRid = numRib.enqueueRender(heightRnd, rctx, ei);
			auto vbrRid = numRib.enqueueRender(videoBitrateRnd, rctx, ei);
			auto amsrRid = numRib.enqueueRender(audioMinSampleRateRnd, rctx, ei);

			std::unique_ptr<torasu::RenderResult> rrFormat(ei->fetchRenderResult(formatRid));
			std::unique_ptr<torasu::RenderResult> rrBegin(ei->fetchRenderResult(beginRid));
			std::unique_ptr<torasu::RenderResult> rrEnd(ei->fetchRenderResult(endRid));
			std::unique_ptr<torasu::RenderResult> rrFps(ei->fetchRenderResult(fpsRid));
			std::unique_ptr<torasu::RenderResult> rrWidth(ei->fetchRenderResult(widthRid));
			std::unique_ptr<torasu::RenderResult> rrHeight(ei->fetchRenderResult(heightRid));
			std::unique_ptr<torasu::RenderResult> rrVbr(ei->fetchRenderResult(vbrRid));
			std::unique_ptr<torasu::RenderResult> rrAmsr(ei->fetchRenderResult(amsrRid));

			auto* dFormat = textHandle.getFrom(rrFormat.get()).getResult();
			auto* dBegin = numHandle.getFrom(rrBegin.get()).getResult();
			auto* dEnd = numHandle.getFrom(rrEnd.get()).getResult();
			auto* dFps = numHandle.getFrom(rrFps.get()).getResult();
			auto* dWidth = numHandle.getFrom(rrWidth.get()).getResult();
			auto* dHeight = numHandle.getFrom(rrHeight.get()).getResult();
			auto* dVbr = numHandle.getFrom(rrVbr.get()).getResult();
			auto* dAmsr = numHandle.getFrom(rrAmsr.get()).getResult();

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
		}

		torasu::tstd::Dnum frameRatio((double) req.width / req.height);
		torasu::tstd::Dnum frameDuration(0);

		torasu::tools::RenderInstructionBuilder visRib;
		torasu::tstd::Dbimg_FORMAT visFmt(req.width, req.height);
		auto visHandle = visRib.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, &visFmt);

		MediaEncoder enc([this, ei, rctx, &visRib, &visHandle, &frameDuration, &frameRatio]
			(MediaEncoder::FrameRequest* fr) {
				torasu::RenderContext modRctx = *rctx;
				if (auto* vidReq = dynamic_cast<MediaEncoder::VideoFrameRequest*>(fr)) {
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
				} if (auto* audReq = dynamic_cast<MediaEncoder::AudioFrameRequest*>(fr)) {
					torasu::tstd::Dnum time(audReq->getStart());
					modRctx[TORASU_STD_CTX_TIME] = &time;
					torasu::tstd::Dnum dur(audReq->getDuration());
					modRctx[TORASU_STD_CTX_DURATION] = &dur;

					torasu::tools::RenderInstructionBuilder audRib;
					auto audHandle = audRib.addSegmentWithHandle<torasu::tstd::Daudio_buffer>(TORASU_STD_PL_AUDIO, audReq->getFormat());

					auto* rr = audRib.runRender(srcRnd, &modRctx, ei);

					fr->setFree([rr]() {
						delete rr;
					});

					auto fetchedRes = audHandle.getFrom(rr);

					if (fetchedRes.getResult() == nullptr) return -1;

					audReq->setResult(fetchedRes.getResult());

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
	elems["begin"] = beginRnd;
	elems["end"] = endRnd;
	elems["fps"] = fpsRnd;
	elems["width"] = widthRnd;
	elems["height"] = heightRnd;
	elems["vbr"] = videoBitrateRnd;
	elems["amsr"] = audioMinSampleRateRnd;
	return elems;
}

void Rmedia_creator::setElement(std::string key, torasu::Element* elem) {
	if (torasu::tools::trySetRenderableSlot("src", &srcRnd, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("begin", &beginRnd, false, key, elem, &ownsBegin)) return;
	if (torasu::tools::trySetRenderableSlot("end", &endRnd, false, key, elem, &ownsEnd)) return;
	if (torasu::tools::trySetRenderableSlot("fps", &fpsRnd, false, key, elem, &ownsFps)) return;
	if (torasu::tools::trySetRenderableSlot("width", &widthRnd, false, key, elem, &ownsWidth)) return;
	if (torasu::tools::trySetRenderableSlot("height", &heightRnd, false, key, elem, &ownsHeight)) return;
	if (torasu::tools::trySetRenderableSlot("vbr", &videoBitrateRnd, false, key, elem, &ownsVideoBitrate)) return;
	if (torasu::tools::trySetRenderableSlot("amsr", &audioMinSampleRateRnd, false, key, elem, &ownsAudioMinSampleRate)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

} // namespace imgc
