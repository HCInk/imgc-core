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

namespace {

inline constexpr void recordMinMax(double in, double* min, double* max) {
	if (*min > in) *min = in;
	if (*max < in) *max = in;
}

inline constexpr void evalMatrixMinMax(double nums[6], double posX, double posY, double* xMin, double* xMax, double* yMin, double* yMax) {
	recordMinMax(nums[0]*posX+nums[1]*posY+nums[2], xMin, xMax);
	recordMinMax(nums[3]*posX+nums[4]*posY+nums[5], yMin, yMax);
}

inline torasu::tstd::Dmatrix createMatrixFromCrop(torasu::tstd::Dbimg::CropInfo crop, uint32_t originalWidth, uint32_t originalHeight) {
	double leftOff = static_cast<double>(-crop.left)/originalWidth;
	double rightOff = static_cast<double>(-crop.right)/originalWidth;
	double bottomOff = static_cast<double>(-crop.bottom)/originalHeight;
	double topOff = static_cast<double>(-crop.top)/originalHeight;

	double totalX = 1+leftOff+rightOff;
	double totalY = 1+bottomOff+topOff;
	return torasu::tstd::Dmatrix({
		1/totalX, 0, (leftOff-rightOff)/totalX,
		0, 1/totalY, (bottomOff-topOff)/totalY,
		0, 0, 1
	}, 3);
}

} // namespace


namespace imgc {

Rtransform::Rtransform(torasu::RenderableSlot source, torasu::RenderableSlot transform, torasu::tstd::NumSlot shutter, torasu::tstd::NumSlot interpolationLimit)
	: SimpleRenderable(false, true), source(source), transform(transform), shutter(shutter), interpolationLimit(interpolationLimit) {}

Rtransform::~Rtransform() {}

torasu::Identifier Rtransform::getType() {
	return "IMGC::RTRANSFORM";
}

torasu::RenderResult* Rtransform::render(torasu::RenderInstruction* ri) {

	torasu::tools::RenderHelper rh(ri);
	auto& lirb = rh.lrib;

	auto resSettings = ri->getResultSettings();
	if (resSettings->getPipeline() == TORASU_STD_PL_VIS) {
		torasu::tstd::Dbimg_FORMAT* fmt;
		if (!(fmt = rh.getFormat<torasu::tstd::Dbimg_FORMAT>())) {
			return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_FORMAT);
		}

		bool validTransform = true;

		// Calculate shutter/interpolation
		double interpolationDuration = 0;
		size_t interpolationCount = 256;

		auto li = ri->getLogInstruction();
		std::chrono::steady_clock::time_point bench;
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
				torasu::ResultSettings rsNum(TORASU_STD_PL_NUM, torasu::tools::NO_FORMAT);

				std::unique_ptr<torasu::RenderResult> shutterRes(rh.runRender(shutter, &rsNum));
				auto shutterRendered = rh.evalResult<torasu::tstd::Dnum>(shutterRes.get());

				if (shutterRendered) {
					interpolationDuration = baseDuration.getNum() * shutterRendered.getResult()->getNum();
				} else {
					validTransform = false;
					if (rh.mayLog(torasu::WARN))
						lirb.logCause(torasu::WARN, "Sub render failed to provide shutter, will not interpolate transformation", shutterRendered.takeInfoTag());
				}

				if (interpolationDuration > 0 && interpolationLimit.get() != nullptr) {
					std::unique_ptr<torasu::RenderResult> imaxRes(rh.runRender(interpolationLimit, &rsNum));
					auto imaxRendered = rh.evalResult<torasu::tstd::Dnum>(imaxRes.get());

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

		double maxSizeX = 0;
		double maxSizeY = 0;
		double destMinX = INFINITY;
		double destMaxX = -INFINITY;
		double destMinY = INFINITY;
		double destMaxY = -INFINITY;

		// Calculate transform-vector(s)

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

			torasu::tools::ResultSettingsSingleFmt rsVec(TORASU_STD_PL_VEC, nullptr);

			for (size_t i = 0; i < interpolationCount; i++) {
				torasu::RenderContext* modRctx = &rctxs.emplace_back(*rh.rctx);
				torasu::tstd::Dnum* newTime = &nums.emplace_back(baseTime.getNum() + i*(interpolationDuration/interpolationCount));
				(*modRctx)[TORASU_STD_CTX_TIME] = newTime;

				renderIds[i] = rh.enqueueRender(transform, &rsVec, modRctx);
			}

			for (size_t i = 0; i < interpolationCount; i++) {
				std::unique_ptr<torasu::RenderResult> resT(rh.fetchRenderResult(renderIds[i]));
				auto transform = rh.evalResult<torasu::tstd::Dmatrix>(resT.get());

				if (transform) {
					const auto& matrix = *transform.getResult();
					double nums[6];
					{
						// Copy to a more usable array
						const auto dnums = matrix.getNums();
						for (size_t iNum = 0; iNum < 6; iNum++) {
							nums[iNum] = dnums[iNum].getNum();
						}
					}
					matrices[i] = torasu::tstd::Dmatrix({
						nums[0], nums[1], nums[2],
						nums[3], nums[4], nums[5],
						0, 0, 1,
					}, 3);
					{
						// Calculate source-scaling
						const double xScale = std::abs(nums[0]);
						if (maxSizeX < xScale) maxSizeX = xScale;
						const double yScale = std::abs(nums[4]);
						if (maxSizeY < yScale) maxSizeY = yScale;
					}
					{
						// Calculate image extremities
						evalMatrixMinMax(nums, 1, 1, &destMinX, &destMaxX, &destMinY, &destMaxY);
						evalMatrixMinMax(nums, 1, -1, &destMinX, &destMaxX, &destMinY, &destMaxY);
						evalMatrixMinMax(nums, -1, 1, &destMinX, &destMaxX, &destMinY, &destMaxY);
						evalMatrixMinMax(nums, -1, -1, &destMinX, &destMaxX, &destMinY, &destMaxY);
					}
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
		const uint32_t fullDestWidth = fmt->getWidth();
		const uint32_t fullDestHeight = fmt->getHeight();
		const uint32_t srcWidth = std::ceil(maxSizeX*fullDestWidth);
		const uint32_t srcHeight = std::ceil(maxSizeY*fullDestHeight);

		if (srcWidth == 0 || srcHeight == 0) {
			// TODO proper-bail-out
			torasu::tstd::Dbimg* result = new torasu::tstd::Dbimg(fullDestWidth, fullDestHeight);
			result->clear();
			return rh.buildResult(result, validTransform ? torasu::RenderResultStatus_OK : torasu::RenderResultStatus_OK_WARN);
		}

		torasu::tstd::Dbimg_FORMAT srcFmt(srcWidth, srcHeight);

		torasu::tools::ResultSettingsSingleFmt rsImg(TORASU_STD_PL_VIS, &srcFmt);

		std::unique_ptr<torasu::RenderResult> resS(rh.runRender(source, &rsImg));
		auto source = rh.evalResult<torasu::tstd::Dbimg>(resS.get());

		if (source) {
			torasu::tstd::Dbimg* src = source.getResult();
			torasu::tstd::Dbimg* result;
			if (const auto* requestedCrop = fmt->getCropInfo()) {
				torasu::tstd::Dbimg::CropInfo crop;
				crop.left = std::max(requestedCrop->left,
									 static_cast<int32_t>(std::floor((destMinX+1)/2*fullDestWidth)));
				crop.right = std::max(requestedCrop->right,
									  static_cast<int32_t>(std::ceil((1-destMaxX)/2*fullDestWidth)));
				crop.bottom = std::max(requestedCrop->bottom,
									   static_cast<int32_t>(std::floor((destMinY+1)/2*fullDestHeight)));
				crop.top = std::max(requestedCrop->top,
									static_cast<int32_t>(std::ceil((1-destMaxY)/2*fullDestHeight)));
				if (static_cast<int32_t>(fullDestHeight) <= crop.left+crop.right || static_cast<int32_t>(fullDestHeight) <= crop.top+crop.bottom) {
					// TODO proper-bail-out
					crop = *requestedCrop;
				}

				result = new torasu::tstd::Dbimg(fullDestWidth-crop.left-crop.right, fullDestHeight-crop.bottom-crop.top, new auto(crop));

				auto postScaleMatrix = createMatrixFromCrop(crop, fullDestWidth, fullDestHeight);
				for (size_t iMat = 0; iMat < matrices.size(); iMat++) {
					matrices[iMat] = matrices[iMat].multiplyByMatrix(postScaleMatrix);
				}
			} else {
				result = new torasu::tstd::Dbimg(fullDestWidth, fullDestHeight);
			}

			if (doBench) bench = std::chrono::steady_clock::now();

			if (interpolationDuration > 0) {
				imgc::transform::transformMix(src->getImageData(), result->getImageData(), srcWidth, srcHeight, result->getWidth(), result->getHeight(), matrices.data(), interpolationCount);
			} else {
				imgc::transform::transform(src->getImageData(), result->getImageData(), srcWidth, srcHeight, result->getWidth(), result->getHeight(), *matrices.data());
			}

			if (doBench) li.logger->log(torasu::LogLevel::DEBUG,
											"Trans Time = " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count()) + "[ms]");


			return rh.buildResult(result, validTransform ? torasu::RenderResultStatus_OK : torasu::RenderResultStatus_OK_WARN);
		} else {

			if (rh.mayLog(torasu::WARN))
				lirb.logCause(torasu::WARN, "Sub render failed to provide source, returning empty image", source.takeInfoTag());

			torasu::tstd::Dbimg* errRes = new torasu::tstd::Dbimg(*fmt);
			errRes->clear();
			return rh.buildResult(errRes, torasu::RenderResultStatus_OK_WARN);
		}

	} else {
		return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_SEGMENT);
	}

}

torasu::ElementMap Rtransform::getElements() {
	torasu::ElementMap elems;

	elems["s"] = source;
	elems["t"] = transform;
	if (shutter) elems["shut"] = shutter;
	if (interpolationLimit) elems["ilimit"] = interpolationLimit;

	return elems;
}

const torasu::OptElementSlot Rtransform::setElement(std::string key, const torasu::ElementSlot* elem) {
	if (key == "s") return torasu::tools::trySetRenderableSlot(&source, elem);
	if (key == "t") return torasu::tools::trySetRenderableSlot(&transform, elem);
	if (key == "shut") return torasu::tools::trySetRenderableSlot(&shutter, elem);
	if (key == "ilimit") return torasu::tools::trySetRenderableSlot(&interpolationLimit, elem);
	return nullptr;
}

} // namespace imgc