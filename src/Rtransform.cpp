#include "../include/torasu/mod/imgc/Rtransform.hpp"

#include <memory>
#include <string>
#include <optional>
#include <chrono>
#include <list>

#include <torasu/torasu.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/log_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dmatrix.hpp>

#include <torasu/mod/imgc/Transform.hpp>

namespace imgc {

Rtransform::Rtransform(torasu::tools::RenderableSlot source, torasu::tools::RenderableSlot transform, torasu::tstd::NumSlot shutter, torasu::tstd::NumSlot interpolationLimit)
	: SimpleRenderable(std::string("IMGC::RTRANSFORM"), false, true), source(source), transform(transform), shutter(shutter), interpolationLimit(interpolationLimit) {}

Rtransform::~Rtransform() {

}

torasu::ResultSegment* Rtransform::renderSegment(torasu::ResultSettings* resSettings, torasu::RenderInstruction* ri) {

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

		bool validTransform = true;

		// Calculate shutter/interpolation
		double interpolationDuration = 0;
		size_t interpolationCount = 256;

		auto li = ri->getLogInstruction();
		std::chrono::_V2::steady_clock::time_point bench;
		bool doBench = li.level <= torasu::LogLevel::DEBUG;

		if (shutter.get() != nullptr) {

			// Dynamic Base
			// torasu::tstd::Dnum baseDuration = 0;
			// {
			// 	torasu::DataResource* foundDuration = (*rh.rctx)[TORASU_STD_CTX_DURATION];
			// 	if (foundDuration) {
			// 		if (torasu::tstd::Dnum* timeNum = dynamic_cast<torasu::tstd::Dnum*>(foundDuration)) {
			// 			baseDuration = *timeNum;
			// 		}
					
			// 	}
			// }

			// Static Base
			torasu::tstd::Dnum baseDuration = 1;

			if (baseDuration.getNum() > 0) {
				torasu::tools::RenderInstructionBuilder ribNum;
				auto numHandle = ribNum.addSegmentWithHandle<torasu::tstd::Dnum>(TORASU_STD_PL_NUM, nullptr);

				auto shutterRid = ribNum.enqueueRender(shutter, &rh);
				std::unique_ptr<torasu::RenderResult> shutterRes(rh.fetchRenderResult(shutterRid));
				auto shutterRendered = numHandle.getFrom(shutterRes.get());

				if (shutterRendered) {
					interpolationDuration = baseDuration.getNum() * shutterRendered.getResult()->getNum();	
				} else {
					validTransform = false;
					if (rh.mayLog(torasu::WARN)) 
						lirb.logCause(torasu::WARN, "Sub render failed to provide shutter, will not interpolate transformation", shutterRendered.takeInfoTag());
				}

				if (interpolationDuration > 0 && interpolationLimit.get() != nullptr) {

					auto imaxRid = ribNum.enqueueRender(interpolationLimit, &rh);
					std::unique_ptr<torasu::RenderResult> imaxRes(rh.fetchRenderResult(imaxRid));
					auto imaxRendered = numHandle.getFrom(imaxRes.get());

					if (imaxRendered) {
						interpolationCount = imaxRendered.getResult()->getNum();
					} else {
						validTransform = false;
						if (rh.mayLog(torasu::WARN)) 
							lirb.logCause(torasu::WARN, 
								"Sub render failed to provide shutter-interpolation-count, will use default interpoltion-max (" + std::to_string(interpolationCount) + ")", 
								imaxRendered.takeInfoTag());
					}
				}
			}
		}

		if (interpolationDuration <= 0) {
			interpolationCount = 1;
		}

		// Calculate transform-vector(s)

		torasu::tools::RenderInstructionBuilder ribVec;
		auto vecHandle = ribVec.addSegmentWithHandle<torasu::tstd::Dmatrix>(TORASU_STD_PL_VEC, nullptr);

		std::vector<torasu::tstd::Dmatrix> matrices(interpolationCount, torasu::tstd::Dmatrix(3));

		{

			torasu::tstd::Dnum baseTime = 0;
			{
				torasu::DataResource* foundTime = (*rh.rctx)[TORASU_STD_CTX_TIME];
				if (foundTime) {
					if (torasu::tstd::Dnum* timeNum = dynamic_cast<torasu::tstd::Dnum*>(foundTime)) {
						baseTime = *timeNum;
					}
					
				}
			}

			std::list<torasu::RenderContext> rctxs;
			std::list<torasu::tstd::Dnum> nums;
			std::vector<uint64_t> renderIds(interpolationCount);

			if (doBench) bench = std::chrono::steady_clock::now();

			for (size_t i = 0; i < interpolationCount; i++) {
				torasu::RenderContext* modRctx = &rctxs.emplace_back(*rh.rctx);
				torasu::tstd::Dnum* newTime = &nums.emplace_back(baseTime.getNum() + i*(interpolationDuration/interpolationCount));
				(*modRctx)[TORASU_STD_CTX_TIME] = newTime;

				renderIds[i] = ribVec.enqueueRender(transform, &rh, modRctx);
			}

			for (size_t i = 0; i < interpolationCount; i++) {
				std::unique_ptr<torasu::RenderResult> resT(rh.fetchRenderResult(renderIds[i]));
				auto transform = vecHandle.getFrom(resT.get(), &rh);

				if (transform) {
					matrices[i] = *transform.getResult();
				} else {
					validTransform = false;
					if (rh.mayLog(torasu::WARN)) 
						lirb.logCause(torasu::WARN, "Sub render failed to provide transformation, will not transform image", transform.takeInfoTag());
				}
			}


			if (doBench) li.logger->log(torasu::LogLevel::DEBUG,
											"Trans Matrix Time = " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count()) + "[ms] for " + std::to_string(interpolationCount) + " interpolation(s)");
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

			if (doBench) bench = std::chrono::steady_clock::now();

			if (interpolationDuration > 0) {
				imgc::transform::transformMix(source.getResult()->getImageData(), result->getImageData(), width, height, matrices.data(), interpolationCount);
			} else {
				imgc::transform::transform(source.getResult()->getImageData(), result->getImageData(), width, height, *matrices.data());
			}

			if (doBench) li.logger->log(torasu::LogLevel::DEBUG,
											"Trans Time = " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count()) + "[ms]");


			return rh.buildResult(result, validTransform ? torasu::ResultSegmentStatus_OK : torasu::ResultSegmentStatus_OK_WARN);
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
	if (shutter.get() != nullptr) {
		elems["shut"] = shutter.get();
	}
	if (interpolationLimit.get() != nullptr) {
		elems["ilimit"] = interpolationLimit.get();
	}

	return elems;
}

void Rtransform::setElement(std::string key, Element* elem) {
	if (torasu::tools::trySetRenderableSlot("s", &source, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("t", &transform, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("shut", &shutter, true, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("ilimit", &interpolationLimit, true, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

} // namespace imgc