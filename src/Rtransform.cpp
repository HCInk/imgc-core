#include "../include/torasu/mod/imgc/Rtransform.hpp"

#include <memory>
#include <string>
#include <optional>
#include <chrono>

#include <torasu/torasu.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/log_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dmatrix.hpp>

#include <torasu/mod/imgc/Transform.hpp>

namespace imgc {

Rtransform::Rtransform(torasu::tools::RenderableSlot source, torasu::tools::RenderableSlot transform)
	: SimpleRenderable(std::string("IMGC::RTRANSFORM"), false, true), source(source), transform(transform) {}

Rtransform::~Rtransform() {

}

torasu::ResultSegment* Rtransform::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {

	torasu::tools::RenderHelper rh(ri);
	auto& lirb = rh.lrib;

	const auto selPipleine = resSettings->getPipeline();

	if (selPipleine == TORASU_STD_PL_VIS) {
		torasu::tstd::Dbimg_FORMAT* fmt;
		auto fmtSettings = resSettings->getResultFormatSettings();
		if ( !( fmtSettings != nullptr
				&& (fmt = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(fmtSettings)) )) {
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
		}

		// Calculate transform-vector

		torasu::tools::RenderInstructionBuilder ribVec;
		auto vecHandle = ribVec.addSegmentWithHandle<torasu::tstd::Dmatrix>(TORASU_STD_PL_VEC, nullptr);

		auto rendT = ribVec.enqueueRender(transform, &rh);
		std::unique_ptr<torasu::RenderResult> resT(rh.fetchRenderResult(rendT));
		auto transform = vecHandle.getFrom(resT.get(), &rh);

		bool validTransform;
		torasu::tstd::Dmatrix transformMatrix(3);
		if (transform) {
			validTransform = true;
			transformMatrix = *transform.getResult();
		} else {
			validTransform = false;
			if (rh.mayLog(torasu::WARN)) 
				lirb.logCause(torasu::WARN, "Sub render failed to provide tranformation, will not tranform image", transform.takeInfoTag());
		}

		// Calculate source

		torasu::tools::RenderInstructionBuilder ribImg;
		auto imgHandle = ribImg.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, fmt);

		auto rendS = ribImg.enqueueRender(source, &rh);
		std::unique_ptr<torasu::RenderResult> resS(rh.fetchRenderResult(rendS));
		auto source = imgHandle.getFrom(resS.get(), &rh);

		if (source) {
			torasu::tstd::Dbimg* result = new torasu::tstd::Dbimg(*fmt);

			const uint32_t height = result->getHeight();
			const uint32_t width = result->getWidth();

			auto li = ri->getLogInstruction();

			bool doBench = li.level <= torasu::LogLevel::DEBUG;
			std::chrono::_V2::steady_clock::time_point bench;
			if (doBench) bench = std::chrono::steady_clock::now();


			imgc::transform::transform(source.getResult()->getImageData(), result->getImageData(), width, height, transformMatrix);

			if (doBench) li.logger->log(torasu::LogLevel::DEBUG,
											"Trans Time = " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count()) + "[ms]");


			return rh.buildResult(result);
		} else {

			if (rh.mayLog(torasu::WARN))
				lirb.logCause(torasu::WARN, "Sub render failed to provide source, returning empty image", source.takeInfoTag());

			torasu::tstd::Dbimg* errRes = new torasu::tstd::Dbimg(*fmt);
			errRes->clear();
			return rh.buildResult(errRes, torasu::ResultSegmentStatus_OK_WARN);
		}

	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}

}

torasu::ElementMap Rtransform::getElements() {
	torasu::ElementMap elems;

	elems["s"] = source.get();
	elems["t"] = transform.get();

	return elems;
}

void Rtransform::setElement(std::string key, Element* elem) {
	if (torasu::tools::trySetRenderableSlot("s", &source, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("t", &transform, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

} // namespace imgc