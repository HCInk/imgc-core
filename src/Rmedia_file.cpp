#include "../include/torasu/mod/imgc/Rmedia_file.hpp"

#include <sstream>
#include <optional>
#include <iostream>
#include <memory>

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

class Rmedia_creator_readyobj : public torasu::ReadyState {
private:
	std::vector<torasu::Identifier>* ops;
	const torasu::RenderContextMask* rctxm;
protected:
	torasu::RenderResult* srcRendRes = nullptr;
	torasu::tstd::Dfile* srcFile = nullptr;
	MediaDecoder* decoder = nullptr;

	Rmedia_creator_readyobj(std::vector<torasu::Identifier>* ops, const torasu::RenderContextMask* rctxm) 
		: ops(ops), rctxm(rctxm) {}
public:
	~Rmedia_creator_readyobj() {
		delete ops;
		if (decoder != nullptr) delete decoder;
		if (srcRendRes != nullptr) delete srcRendRes;
	}

	virtual const std::vector<torasu::Identifier>* getOperations() const override {
		return ops;
	}

	virtual const torasu::RenderContextMask* getContextMask() const override {
		return rctxm;
	}

	virtual size_t size() const override {
		size_t size = 0;
		
		if (srcFile != nullptr) size += srcFile->getFileSize();

		return size;
	}

	virtual ReadyState* clone() const override {
		// TODO implement clone
		throw new std::runtime_error("cloning currently unsupported");
	}

	friend Rmedia_file;
};

Rmedia_file::Rmedia_file(torasu::tools::RenderableSlot src)
	: torasu::tools::SimpleDataElement(false, true), srcRnd(src) {}

Rmedia_file::~Rmedia_file() {}

torasu::Identifier Rmedia_file::getType() { return "IMGC::RMEDIA_FILE"; }

void Rmedia_file::ready(torasu::ReadyInstruction* ri) {
	if (srcRnd.get() == NULL) throw std::logic_error("Source renderable set loaded yet!");

	torasu::tools::RenderHelper rh(ri);

	// Render File

	torasu::ResultSettings fileSettings(TORASU_STD_PL_FILE, nullptr);

	std::unique_ptr<torasu::RenderResult> rr(rh.runRender(srcRnd.get(), &fileSettings));

	auto castedResultSeg = rh.evalResult<torasu::tstd::Dfile>(rr.get());

	if (!castedResultSeg) {
		constexpr auto message = "Failed loading source-file of media!";
		if (rh.mayLog(torasu::ERROR)) {
			torasu::tools::LogInfoRefBuilder lrib(rh.li);
			lrib.addCause(castedResultSeg.takeInfoTag());
			rh.li.logger->log(new torasu::LogMessage(torasu::ERROR, message, lrib.build()));
		}
		throw std::logic_error(message);
	}

	auto* obj = new Rmedia_creator_readyobj(
					new std::vector<torasu::Identifier>({
						TORASU_STD_PL_VIS, TORASU_STD_PL_AUDIO, 
						TORASU_PROPERTY(TORASU_STD_PROP_IMG_WIDTH), TORASU_PROPERTY(TORASU_STD_PROP_IMG_HEIGHT), 
						TORASU_PROPERTY(TORASU_STD_PROP_IMG_RAITO), TORASU_PROPERTY(TORASU_STD_PROP_DURATION)}), 
					castedResultSeg.getResultMask());

	ri->setState(obj);


	auto* srcFile = castedResultSeg.getResult();
	obj->srcFile = srcFile;

	// Create Decoder

	auto* decoder = new MediaDecoder(srcFile->getFileData(), srcFile->getFileSize(), rh.li);
	obj->decoder = decoder;

	// Close refs so it can be stored, without its LogInstruction still existing
	rr->closeRefs();
	obj->srcRendRes = rr.release();

}


torasu::RenderResult* Rmedia_file::render(torasu::RenderInstruction* ri) {
	torasu::ExecutionInterface* ei = ri->getExecutionInterface();
	torasu::LogInstruction li = ri->getLogInstruction();

	torasu::tools::RenderHelper rh(ri);
	auto* decoder = static_cast<Rmedia_creator_readyobj*>(ri->getReadyState())->decoder;

	std::optional<std::string> videoKey;
	std::optional<std::string> audioKey;

	auto segmentSettings = ri->getResultSettings();
	auto currentPipeline = segmentSettings->getPipeline();

	if (currentPipeline == TORASU_STD_PL_VIS || currentPipeline == TORASU_STD_PL_AUDIO) {

		// Parse RenderContext
		auto* rctx = rh.rctx;

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

		auto* fmt = segmentSettings->getFromat();

		if (currentPipeline == TORASU_STD_PL_VIS) {
			// Render video

			torasu::tstd::Dbimg_FORMAT* videoFormat;
			if (fmt == nullptr || !( videoFormat = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(fmt) )) {
				return rh.buildResult(torasu::RenderResultStatus_INVALID_FORMAT);
			}

			torasu::tstd::Dbimg_sequence* vidBuff = nullptr;

			ei->lock();

			decoder->getSegment((SegmentRequest) {
				.start = time,
				.end = time+duration,
				.videoBuffer = &vidBuff,
				.audioBuffer = nullptr
			}, li);

			ei->unlock();

			std::unique_ptr<torasu::tstd::Dbimg_sequence> vidSeq(vidBuff);
			auto& frames = vidSeq->getFrames();
			if (frames.size() <= 0) {
				if (rh.mayLog(torasu::WARN))
					rh.lrib.logCause(torasu::LogLevel::WARN, "DECODER RETURNED NO FRAME! "
						"(TIME: " + std::to_string(time) + "-" + std::to_string(time+duration) + ")");
				auto* errRes = new torasu::tstd::Dbimg(*videoFormat);
				errRes->clear();
				return rh.buildResult(errRes, torasu::RenderResultStatus_OK_WARN);
			}

			auto firstFrame = frames.begin();
			torasu::tstd::Dbimg* resultFrame = firstFrame->second;
			frames.erase(firstFrame);

			vidSeq.reset();


			if (videoFormat != nullptr) {

				auto* scaled = scaler::scaleImg(resultFrame, videoFormat);

				if (scaled != nullptr) {
					delete resultFrame;
					resultFrame = scaled;
				}
			}

			return rh.buildResult(resultFrame);

		} else if (currentPipeline == TORASU_STD_PL_AUDIO) {
			// Render audio

			torasu::tstd::Daudio_buffer* audBuff = nullptr;
			ei->lock();

			decoder->getSegment((SegmentRequest) {
				.start = time,
				.end = time+duration,
				.videoBuffer = nullptr,
				.audioBuffer = &audBuff
			}, li);

			ei->unlock();

			return rh.buildResult(audBuff);

		}
		

	} else if (torasu::isPipelineKeyPropertyKey(currentPipeline)) { // optional so properties get skipped if it is no property
		if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_IMG_WIDTH)) {
			auto dims = decoder->getDimensions();
			return rh.buildResult(new torasu::tstd::Dnum(dims.first));
		} else if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_IMG_HEIGHT)) {
			auto dims = decoder->getDimensions();
			return rh.buildResult(new torasu::tstd::Dnum(dims.second));
		} else if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_IMG_RAITO)) {
			auto dims = decoder->getDimensions();
			return rh.buildResult(new torasu::tstd::Dnum((double) dims.first / dims.second));
		} else if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_DURATION)) {
			double duration = decoder->getDuration();
			return rh.buildResult(new torasu::tstd::Dnum(duration));
		}
	}

	return rh.buildResult(torasu::RenderResultStatus_INVALID_SEGMENT);

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
