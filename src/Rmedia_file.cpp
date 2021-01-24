#include "../include/torasu/mod/imgc/Rmedia_file.hpp"

#include <sstream>
#include <optional>
#include <iostream>

#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/property_names.hpp>
#include <torasu/std/Dnum.hpp>
#include <torasu/std/Dfile.hpp>
#include <torasu/std/Dbimg.hpp>

#include <torasu/mod/imgc/MediaDecoder.hpp>
#include <torasu/mod/imgc/Scaler.hpp>

namespace imgc {


Rmedia_file::Rmedia_file(torasu::tools::RenderableSlot src)
	: torasu::tools::NamedIdentElement("IMGC::RMEDIA_FILE"),
	  torasu::tools::SimpleDataElement(false, true),
	  srcRnd(src) {}

Rmedia_file::~Rmedia_file() {

	if (decoder != NULL) delete decoder;

	if (srcRendRes != NULL) delete srcRendRes;

}


void Rmedia_file::load(torasu::ExecutionInterface* ei, torasu::LogInstruction li) {

	if (decoder != nullptr) return;

	ei->lock();

	if (decoder != nullptr) {
		ei->unlock();
		return;
	}

	if (srcRnd.get() == NULL) throw std::logic_error("Source renderable set loaded yet!");

	if (srcRendRes != NULL) {
		delete srcRendRes;
		srcRendRes = NULL;
		srcFile = NULL;
	}

	// Render File

	torasu::tools::RenderInstructionBuilder rib;

	auto handle = rib.addSegmentWithHandle<torasu::tstd::Dfile>(TORASU_STD_PL_FILE, NULL);

	torasu::RenderContext rctx;
	srcRendRes = rib.runRender(srcRnd.get(), &rctx, ei, li);

	auto castedResultSeg = handle.getFrom(srcRendRes);

	if (castedResultSeg.getResult() == NULL) {
		throw std::runtime_error("Sub-render of file failed");
	}

	srcFile = castedResultSeg.getResult();

	// Create Decoder

	decoder = new MediaDecoder(srcFile->getFileData(), srcFile->getFileSize());

	ei->unlock();
}


torasu::RenderResult* Rmedia_file::render(torasu::RenderInstruction* ri) {
	torasu::ExecutionInterface* ei = ri->getExecutionInterface();
	torasu::LogInstruction li = ri->getLogInstruction();
	load(ei, li);

	std::map<std::string, torasu::ResultSegment*>* results = new std::map<std::string, torasu::ResultSegment*>();

	std::optional<std::string> videoKey;
	torasu::tstd::Dbimg_FORMAT* videoFormat = NULL;
	std::optional<std::string> audioKey;

	std::string currentPipeline;
	for (torasu::ResultSegmentSettings* segmentSettings : *ri->getResultSettings()) {
		currentPipeline = segmentSettings->getPipeline();
		if (currentPipeline == TORASU_STD_PL_VIS) {
			videoKey = segmentSettings->getKey();
			auto fmtSettings = segmentSettings->getResultFormatSettings();

			if (fmtSettings != NULL) {
				if (!( videoFormat = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(fmtSettings) )) {
					(*results)[videoKey.value()] = new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
				}
			}

		} else if (currentPipeline == TORASU_STD_PL_AUDIO) {
			audioKey = segmentSettings->getKey();
		} else if (torasu::isPipelineKeyPropertyKey(currentPipeline)) { // optional so properties get skipped if it is no property
			if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_IMG_WIDTH)) {
				std::pair<int32_t, int32_t> dims = decoder->getDimensions();
				(*results)[segmentSettings->getKey()]
					= new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, new torasu::tstd::Dnum(dims.first), true);
			} else if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_IMG_HEIGHT)) {
				std::pair<int32_t, int32_t> dims = decoder->getDimensions();
				(*results)[segmentSettings->getKey()]
					= new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, new torasu::tstd::Dnum(dims.second), true);
			} else if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_IMG_RAITO)) {
				std::pair<int32_t, int32_t> dims = decoder->getDimensions();
				(*results)[segmentSettings->getKey()]
					= new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, new torasu::tstd::Dnum((double) dims.first / dims.second), true);
			} else if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_DURATION)) {
				double duration = decoder->getDuration();
				(*results)[segmentSettings->getKey()]
					= new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, new torasu::tstd::Dnum(duration), true);
			} else {
				// Unsupported Property
				(*results)[segmentSettings->getKey()]
					= new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
			}
		} else {
			(*results)[segmentSettings->getKey()]
				= new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
		}
	}

	torasu::RenderContext* rctx = ri->getRenderContext();

	if (videoKey.has_value() || audioKey.has_value()) {

		double time = 0;
		double duration = 0;

		if (rctx != NULL) {
			auto found = rctx->find(TORASU_STD_CTX_TIME);
			if (found != rctx->end()) {
				auto timeObj = found->second;
				if (torasu::tstd::Dnum* timeNum = dynamic_cast<torasu::tstd::Dnum*>(timeObj)) {
					time = timeNum->getNum();
				}
			}
			found = rctx->find(TORASU_STD_CTX_DURATION);
			if (found != rctx->end()) {
				auto durationObj = found->second;
				if (torasu::tstd::Dnum* durationNum = dynamic_cast<torasu::tstd::Dnum*>(durationObj)) {
					duration = durationNum->getNum();
				}
			}
		}

		torasu::tstd::Dbimg_sequence* vidBuff = NULL;
		torasu::tstd::Daudio_buffer* audBuff = NULL;

		ei->lock();

		decoder->getSegment((SegmentRequest) {
			.start = time,
			.end = time+duration,
			.videoBuffer = videoKey.has_value() ? &vidBuff : NULL,
			.audioBuffer = audioKey.has_value() ? &audBuff : NULL
		});

		ei->unlock();

		if (videoKey.has_value()) {
			std::unique_ptr<torasu::tstd::Dbimg_sequence> vidSeq(vidBuff);
			auto& frames = vidSeq->getFrames();
			if (frames.size() > 0) {

				auto firstFrame = frames.begin();
				frames.erase(firstFrame);

				vidSeq.reset();

				torasu::tstd::Dbimg* resultFrame = firstFrame->second;

				if (videoFormat != NULL) {

					auto* scaled = scaler::scaleImg(firstFrame->second, videoFormat);

					if (scaled != NULL) {
						delete firstFrame->second;
						resultFrame = scaled;
					}
				}

				(*results)[videoKey.value()] = new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, resultFrame, true);
			} else {
				if (li.level <= torasu::LogLevel::WARN)
					li.logger->log(torasu::LogLevel::WARN, "DECODER RETURNED NO FRAME! "
						"(TIME: " + std::to_string(time) + "-" + std::to_string(time+duration) + ")");
				(*results)[videoKey.value()] = new torasu::ResultSegment(torasu::ResultSegmentStatus_OK_WARN, new torasu::tstd::Dbimg(*videoFormat), true);
			}
		}

		if (audioKey.has_value()) {
			(*results)[audioKey.value()] = new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, audBuff, true);
		}

	}

	return new torasu::RenderResult(torasu::ResultStatus_OK, results);

}

torasu::ElementMap Rmedia_file::getElements() {
	torasu::ElementMap ret;

	if (srcRnd.get() != NULL) ret["src"] = srcRnd.get();

	return ret;
}

void Rmedia_file::setElement(std::string key, Element* elem) {
	if (torasu::tools::trySetRenderableSlot("src", &srcRnd, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}


}
