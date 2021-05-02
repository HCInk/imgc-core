#include "../include/torasu/mod/imgc/Rdirectional_blur.hpp"

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dbimg.hpp>

#define FLOAT_MODE true
#define OUT_OF_IMAGE_MODE 0 // 0: Mirror (default), 1: Transparent, 2: Unclean
#define CONVOLVE_MODE 0 // 0: New (default), 1: legacy

namespace imgc {


Rdirectional_blur::Rdirectional_blur(torasu::tools::RenderableSlot src, torasu::tstd::NumSlot strX, torasu::tstd::NumSlot strY)
	: SimpleRenderable("IMGC::RDIRECTIONAL_BLUR", false, true), src(src), strX(strX), strY(strY) {}

Rdirectional_blur::~Rdirectional_blur() {}

torasu::ResultSegment* Rdirectional_blur::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {
	
	torasu::tools::RenderHelper rh(ri);
	auto& lirb = rh.lrib;

	if (resSettings->getPipeline() == TORASU_STD_PL_VIS) {

		torasu::tstd::Dbimg_FORMAT* fmt;
		auto fmtSettings = resSettings->getResultFormatSettings();
		if ( !( fmtSettings != nullptr
				&& (fmt = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(fmtSettings)) )) {
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
		}

		torasu::tools::RenderInstructionBuilder visRib;
		torasu::tools::RenderResultSegmentHandle<torasu::tstd::Dbimg> visHandle = visRib.addSegmentWithHandle<torasu::tstd::Dbimg>(TORASU_STD_PL_VIS, fmt);
		torasu::tools::RenderInstructionBuilder numRib;
		torasu::tools::RenderResultSegmentHandle<torasu::tstd::Dnum> numHandle = numRib.addSegmentWithHandle<torasu::tstd::Dnum>(TORASU_STD_PL_NUM, nullptr);

		// Sub-Renderings

		auto rendSrc = visRib.enqueueRender(src, &rh);
		auto rendStrX = numRib.enqueueRender(strX, &rh);
		auto rendStrY = numRib.enqueueRender(strY, &rh);

		std::unique_ptr<torasu::RenderResult> resSrc(rh.fetchRenderResult(rendSrc));
		std::unique_ptr<torasu::RenderResult> resStrX(rh.fetchRenderResult(rendStrX));
		std::unique_ptr<torasu::RenderResult> resStrY(rh.fetchRenderResult(rendStrY));

		// Calculating Result from Results

		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dbimg> src = visHandle.getFrom(resSrc.get(), &rh);
		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dnum> strengthX = numHandle.getFrom(resStrX.get(), &rh);
		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dnum> strengthY = numHandle.getFrom(resStrY.get(), &rh);

		if (src) {
			torasu::tstd::Dbimg* original = src.getResult();
			if (strengthX && strengthY) {
				auto* result = new torasu::tstd::Dbimg(*fmt);

				const uint32_t height = result->getHeight();
				const uint32_t width = result->getWidth();
				const uint32_t channels = 4;
				uint8_t* destPtr = result->getImageData();


				auto li = ri->getLogInstruction();

				bool doBench = li.level <= torasu::LogLevel::DEBUG;
				std::chrono::_V2::steady_clock::time_point bench;
				if (doBench) bench = std::chrono::steady_clock::now();

				double strengthXval = strengthX.getResult()->getNum();
				double strengthYval = strengthY.getResult()->getNum();
				size_t convolveXsize = strengthXval * width;				
				size_t convolveYsize = strengthYval * height;
				if (convolveXsize > width) convolveXsize = width; 
				if (convolveYsize > height) convolveYsize = height;


				// Sizes need to be uneven for this to work
				if (convolveXsize % 2 == 0) convolveXsize++;
				if (convolveYsize % 2 == 0) convolveYsize++;

				size_t srcPaddingX = convolveXsize/2;
				size_t srcWidth = width + (srcPaddingX*2);
				size_t srcPaddingY = convolveYsize/2;
				size_t srcHeight = height + (srcPaddingY*2);
				size_t srcNewlinePremul = srcWidth*channels;

				// for (size_t i = 0; i < height*width*channels; i++) {
				// 	destPtr[i] = srcPtr[i];
				// }

#if FLOAT_MODE
				typedef float buff_val;
				const buff_val preDiv = 0xFF;
				const buff_val onDiv = 0x01;
				const buff_val writeBackFac = 0xFF;
#else
				typedef uint8_t buff_val;
				const auto preDiv = 0x1;
				const auto onDiv = 0xFF;
				const auto writeBackFac = 0xFF;
#endif
				// float convolveFacX = (buff_val) 1/convolveXsize;
				// float convolveFacY = (buff_val) 1/convolveYsize;

				typedef float fac_val;
				size_t facVecSize = convolveYsize*convolveXsize;
				std::vector<fac_val> facVec(facVecSize);
				fac_val* facPtr = facVec.data();
				{
					fac_val facTotal = 0;
					fac_val* currFac = facPtr;
					for (size_t y = 0; y < convolveYsize; ) {
						y++;
						for (size_t x = 0; x < convolveXsize; ) {
							x++;
							fac_val fac = x <= srcPaddingX ? (fac_val)x/(srcPaddingX+1) : 1-((fac_val)(x-srcPaddingX-1)/(srcPaddingX+1));
							fac *= y <= srcPaddingY ? (fac_val)y/(srcPaddingY+1) : 1-((fac_val)(y-srcPaddingY-1)/(srcPaddingY+1));
							facTotal += fac;
							*currFac = fac;
							currFac++;
						}
					}

					// fac_val validation = 0;
					for (size_t i = 0; i < facVecSize; i++) {
						facPtr[i] /= facTotal;
						// validation += facPtr[i];
					}
					// torasu::tools::log_checked(li, torasu::DEBUG, "fac-valid: " + std::to_string(validation));
				}

#if CONVOLVE_MODE == 0
				std::vector<buff_val> srcVec(srcWidth*srcHeight*channels);
				buff_val* srcPtr = srcVec.data();
				{
					uint8_t* origPtr = original->getImageData();
					buff_val* currWrite = srcPtr + srcPaddingY*srcNewlinePremul + srcPaddingX*channels;

					for (size_t y = 0; y < height; y++) {

						// Copy line
						buff_val* lineWriteHead = currWrite;
						for (size_t x = 0; x < width; x++) {
							for (size_t c = 0; c < channels; c++) {
								*lineWriteHead = ((buff_val)*origPtr) / preDiv;
								origPtr++;
								lineWriteHead++;
							}
						}
#if OUT_OF_IMAGE_MODE == 0
						// Mirror right
						buff_val* endPivot = lineWriteHead - channels;
						for (size_t x = 0; x < srcPaddingX; ) {
							buff_val* from = endPivot-(x*channels);
							x++;
							buff_val* to = endPivot+(x*channels);
							for (size_t c = 0; c < channels; c++)
								to[c] = from[c];
						}

						// Mirror left
						for (size_t x = 0; x < srcPaddingX; ) {
							buff_val* from = currWrite+(x*channels);
							x++;
							buff_val* to = currWrite-(x*channels);
							for (size_t c = 0; c < channels; c++)
								to[c] = from[c];
						}
#endif

						currWrite += srcNewlinePremul;
					}

#if OUT_OF_IMAGE_MODE == 0
					// Mirror top
					buff_val* topPivot = srcPtr + srcPaddingY*srcNewlinePremul;
					for (size_t y = 0; y < srcPaddingY; ) {
						buff_val* from = topPivot+(y*srcNewlinePremul);
						y++;
						buff_val* to = topPivot-(y*srcNewlinePremul);
						std::copy_n(from, srcNewlinePremul, to);
					}

					// Mirror bottom
					buff_val* bottomPivot = srcPtr + ((srcPaddingY+height-1)*srcNewlinePremul);
					for (size_t y = 0; y < srcPaddingY; ) {
						buff_val* from = bottomPivot-(y*srcNewlinePremul);
						y++;
						buff_val* to = bottomPivot+(y*srcNewlinePremul);
						std::copy_n(from, srcNewlinePremul, to);
					}
#endif

				}

				// std::vector<buff_val> lineVec(srcNewlinePremul*convolveYsize);
				// buff_val* linePtr = lineVec.data();

				buff_val* currSrc = srcPtr;
				for (size_t y = 0; y < height; y++) {

					// std::copy_n(currSrc, srcNewlinePremul*convolveYsize, linePtr);
					buff_val* pxPtr = currSrc;

					for (size_t x = 0; x < width; x++) {
						buff_val accu[4] = {0, 0, 0, 0};
						buff_val* samplePrePtr = pxPtr;
						fac_val* currFacPtr = facPtr;
						for (size_t sY = 0; sY < convolveYsize; sY++) {
							buff_val* samplePtr = samplePrePtr;
							for (size_t sX = 0; sX < convolveXsize; sX++) {
								fac_val fac = *currFacPtr;
								for (size_t c = 0; c < channels; c++) {
									accu[c] += fac * samplePtr[c] / onDiv;
								}
								samplePtr += channels;
								currFacPtr++;
							}
							samplePrePtr += srcNewlinePremul;
						}

						for (size_t c = 0; c < 4; c++) {
							destPtr[c] = accu[c] * writeBackFac;
						}

						destPtr += channels;
						pxPtr += channels;
					}

					currSrc += srcNewlinePremul;
				}


				// buff_val* currRead = srcPtr;
				// uint8_t* currWrite = destPtr;
				// for (size_t y = 0; y < height; y++) {
				// 	for (size_t x = 0; x < width; x++) {
				// 		for (size_t c = 0; c < channels; c++) {
				// 			*currWrite = currRead[x*channels+c] * writeBackFac;
				// 			currWrite++;
				// 		}
				// 	}
				// 	currRead += srcNewlinePremul;
				// }
#elif CONVOLVE_MODE == 1

				uint8_t* srcPtr = original->getImageData();
				size_t lineVecSize = width+(srcPaddingX*2);
				std::vector<buff_val> lineVec(lineVecSize*4);
				buff_val* lineBuf = lineVec.data();

				for (size_t y = 0; y < height; y++) {

					{
						buff_val* currLineBuf = lineBuf;
						
						{
							buff_val firstPx[channels];
							for (size_t c = 0; c < channels; c++) {
								firstPx[c] = convolveFacX * ((buff_val)srcPtr[c])/preDiv;
							}

							for (size_t i = 0; i < srcPaddingX; i++) {
								for (size_t c = 0; c < channels; c++) {
									currLineBuf[c] = firstPx[c];
								}
								currLineBuf += channels;
							}
						}

						for (size_t i = 0;;) {
							for (size_t c = 0; c < channels; c++) {
								currLineBuf[c] = ((buff_val)srcPtr[c])/preDiv;
							}
							currLineBuf += channels;
							i++;
							if (i >= width) break;
							srcPtr += channels;
						}

						{
							buff_val lastPx[channels];
							for (size_t c = 0; c < channels; c++) {
								lastPx[c] = convolveFacX * ((buff_val)srcPtr[c])/preDiv;
							}

							for (size_t i = srcPaddingX+width; i < lineVecSize; i++) {
								for (size_t c = 0; c < channels; c++) {
									currLineBuf[c] = lastPx[c];
								}
								currLineBuf += channels;
							}
						}
						srcPtr += channels;

					}

					buff_val* currLineBuf = lineBuf;
					
					for (size_t x = 0; x < width; x++) {
						buff_val accu[4] = {0, 0, 0, 0};
						buff_val* linePtr = currLineBuf;
						for (size_t s = 0; s < convolveXsize; s++) {
							for (size_t c = 0; c < channels; c++) {
								accu[c] += convolveFacX * linePtr[c] / onDiv;
							}
							linePtr += channels;
						}

						for (size_t c = 0; c < 4; c++) {
							destPtr[c] = accu[c] * writeBackFac;
						}

						destPtr += channels;
						currLineBuf += channels;
					}
					srcPtr += width*channels;
				}
#endif
				if (doBench) li.logger->log(torasu::LogLevel::DEBUG,
												"Blur Time = " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count()) + "[ms]");


				return rh.buildResult(result);
			} else {

				if (rh.mayLog(torasu::WARN)) {
					torasu::tools::LogInfoRefBuilder errorCauses(lirb.linstr);
					if (!strengthX)
						errorCauses.logCause(torasu::WARN, "Blur strX failed to render", strengthX.takeInfoTag());
					if (!strengthY)
						errorCauses.logCause(torasu::WARN, "Blur-strY failed to render", strengthY.takeInfoTag());

					lirb.logCause(torasu::WARN, "Returning original image", errorCauses);

				}

				return rh.buildResult(original->clone(), torasu::ResultSegmentStatus_OK_WARN);
			}
		} else {
			if (rh.mayLog(torasu::WARN)) {
				lirb.logCause(torasu::WARN, "Source image failed to render, returning empty image", src.takeInfoTag());
			}

			torasu::tstd::Dbimg* errRes = new torasu::tstd::Dbimg(*fmt);
			errRes->clear();
			return rh.buildResult(errRes, torasu::ResultSegmentStatus_OK_WARN);
		}

	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}

}


torasu::ElementMap Rdirectional_blur::getElements() {
	torasu::ElementMap elems;

	elems["src"] = src.get();
	elems["x"] = strX.get();
	elems["y"] = strY.get();

	return elems;
}

void Rdirectional_blur::setElement(std::string key, torasu::Element* elem) {
	if (torasu::tools::trySetRenderableSlot("src", &src, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("x", &strX, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("y", &strY, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

}