#include "../include/torasu/mod/imgc/Rmedia_file.hpp"

#include <sstream>
#include <optional>

#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/Dnum.hpp>
#include <torasu/std/Dfile.hpp>

#include <torasu/mod/imgc/MediaDecoder.hpp>

namespace imgc {


Rmedia_file::Rmedia_file(torasu::Renderable* src)
	: torasu::tools::NamedIdentElement("IMGC::RMEDIA_FILE"),
	  torasu::tools::SimpleDataElement(false, true),
	  srcRnd(src) {}

Rmedia_file::~Rmedia_file() {

	if (decoder != NULL) delete decoder;

	if (srcRendRes != NULL) delete srcRendRes;

}


void Rmedia_file::load(torasu::ExecutionInterface* ei) {

	if (decoder != NULL) return;

	if (srcRnd == NULL) throw std::logic_error("Source renderable set loaded yet!");

	if (srcRendRes != NULL) {
		delete srcRendRes;
		srcRendRes = NULL;
		srcFile = NULL;
	}

	// Render File

	torasu::tools::RenderInstructionBuilder rib;

	auto handle = rib.addSegmentWithHandle<torasu::tstd::Dfile>(TORASU_STD_PL_FILE, NULL);

	srcRendRes = rib.runRender(srcRnd, NULL, ei);

	auto castedResultSeg = handle.getFrom(srcRendRes);

	if (castedResultSeg.getResult() == NULL) {
		throw std::runtime_error("Sub-render of file failed");
	}

	srcFile = castedResultSeg.getResult();

	// Create Decoder

	decoder = new MediaDecoder(srcFile->getFileData(), srcFile->getFileSize());

}


torasu::RenderResult* Rmedia_file::render(torasu::RenderInstruction* ri) {

	load(ri->getExecutionInterface());

	std::map<std::string, torasu::ResultSegment*>* results = new std::map<std::string, torasu::ResultSegment*>();

	std::optional<std::string> videoKey;
	std::optional<std::string> audioKey;

	for (torasu::ResultSegmentSettings* segmentSettings : *ri->getResultSettings()) {
		if (segmentSettings->getPipeline() == TORASU_STD_PL_VIS) {
			videoKey = segmentSettings->getKey();
		} else if (segmentSettings->getPipeline() == TORASU_STD_PL_AUDIO) {
			audioKey = segmentSettings->getKey();
		} else {
			(*results)[segmentSettings->getKey()]
				= new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
		}
	}

	if (videoKey.has_value() || audioKey.has_value()) {

		double time = 0;
		double duration = 0;

		torasu::RenderContext* rctx = ri->getRenderContext();
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

		decoder->getSegment((SegmentRequest) {
			.start = time,
			.end = time+duration,
			.videoBuffer = videoKey.has_value() ? &vidBuff : NULL,
			.audioBuffer = audioKey.has_value() ? &audBuff : NULL
		});

		if (videoKey.has_value()) {
			auto firstFrame = vidBuff->getFrames().begin();
			vidBuff->getFrames().erase(firstFrame);
			delete vidBuff;

			(*results)[videoKey.value()] = new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, firstFrame->second, true);
		}

		if (audioKey.has_value()) {
			(*results)[audioKey.value()] = new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, audBuff, true);
		}

	}

	return new torasu::RenderResult(torasu::ResultStatus_OK, results);

}

std::map<std::string, torasu::Element*> Rmedia_file::getElements() {
	std::map<std::string, torasu::Element*> ret;

	if (srcRnd != NULL) ret["src"] = srcRnd;

	return ret;
}

void Rmedia_file::setElement(std::string key, Element* elem) {
	if (key == "src") {

		if (elem == NULL) {
			throw std::invalid_argument("Element slot \"src\" may not be empty!");
		}
		if (torasu::Renderable* rnd = dynamic_cast<torasu::Renderable*>(elem)) {
			srcRnd = rnd;
			return;
		} else {
			throw std::invalid_argument("Element slot \"src\" only accepts Renderables!");
		}

	} else {
		std::ostringstream errMsg;
		errMsg << "The element slot \""
			   << key
			   << "\" does not exist!";
		throw std::invalid_argument(errMsg.str());
	}
}

}
