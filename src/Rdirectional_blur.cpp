#include "../include/torasu/mod/imgc/Rdirectional_blur.hpp"

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dbimg.hpp>

#define DEBUG_COLOR_MODE 0

namespace {

#if DEBUG_COLOR_MODE != 0
const uint8_t RED[] {0xFF, 0x00, 0x00, 0xFF};
const uint8_t YELLOW[] {0xFF, 0xFF, 0x00, 0xFF};
const uint8_t GREEN[] {0x00, 0xFF, 0x00, 0xFF};
const uint8_t BLUE[] {0x00, 0x00, 0xFF, 0xFF};
#endif

} // namespace


namespace imgc {


Rdirectional_blur::Rdirectional_blur(torasu::tools::RenderableSlot src, torasu::tstd::NumSlot direction, torasu::tstd::NumSlot strength)
	: SimpleRenderable("IMGC::RDIRECTIONAL_BLUR", false, true), src(src), direction(direction), strength(strength) {}

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
		auto rendDir = numRib.enqueueRender(direction, &rh);
		auto rendStrgh = numRib.enqueueRender(strength, &rh);

		std::unique_ptr<torasu::RenderResult> resSrc(rh.fetchRenderResult(rendSrc));
		std::unique_ptr<torasu::RenderResult> resDir(rh.fetchRenderResult(rendDir));
		std::unique_ptr<torasu::RenderResult> resStrgh(rh.fetchRenderResult(rendStrgh));

		// Calculating Result from Results

		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dbimg> src = visHandle.getFrom(resSrc.get(), &rh);
		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dnum> dir = numHandle.getFrom(resDir.get(), &rh);
		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dnum> strgh = numHandle.getFrom(resStrgh.get(), &rh);

		if (src) {
			torasu::tstd::Dbimg* original = src.getResult();
			if (dir && strgh) {
				auto* result = new torasu::tstd::Dbimg(*fmt);

				const uint32_t height = result->getHeight();
				const uint32_t width = result->getWidth();
				const uint32_t channels = 4;
				uint8_t* destPtr = result->getImageData();
				uint8_t* srcPtr = original->getImageData();


				auto li = ri->getLogInstruction();

				bool doBench = li.level <= torasu::LogLevel::DEBUG;
				std::chrono::_V2::steady_clock::time_point bench;
				if (doBench) bench = std::chrono::steady_clock::now();

				size_t convolveCount = strgh.getResult()->getNum()*width;
				if (convolveCount > width) convolveCount = width; 
				float convolveFactor = (double) 1/convolveCount;

				// for (size_t i = 0; i < height*width*channels; i++) {
				// 	destPtr[i] = srcPtr[i];
				// }

				typedef float buff_val;
				const auto preDiv = 0xFF;
				const auto onDiv = 0x01;
				const auto writeBackFac = 0xFF;

				// typedef uint8_t buff_val;
				// const auto preDiv = 0x1;
				// const auto onDiv = 0xFF;
				// const auto writeBackFac = 0xFF;

				std::vector<buff_val> lineVec(convolveCount*4);
				buff_val* lineBuf = lineVec.data();

				

				for (size_t y = 0; y < height; y++) {
					size_t lineLeft = width;
					{ // Init buffer
						buff_val* currLineBuf = lineBuf;
						const auto halfConvCount = convolveCount/2;
						for (size_t i = 0; i < halfConvCount*channels; i++) {
#if DEBUG_COLOR_MODE == 1
							*currLineBuf = *(RED+(i%channels)) / preDiv;
#else
							*currLineBuf = (buff_val) *srcPtr / preDiv;
#endif
							currLineBuf++;
						}
						const auto preread = convolveCount-halfConvCount;
						for (size_t i = 0; i < preread*channels; i++) {
#if DEBUG_COLOR_MODE == 1
							*currLineBuf = *(YELLOW+(i%channels)) / preDiv;
#else
							*currLineBuf = (buff_val) *srcPtr / preDiv;
#endif
							currLineBuf++;
							srcPtr++;
						}
						lineLeft -= preread;
					}

					size_t writePos = convolveCount-1;
					
					for (size_t x = 0; x < width; x++) {
						{
							buff_val* currLineBuf = lineBuf;
							buff_val accu[4] = {0, 0, 0, 0};
							for (size_t s = 0; s < convolveCount; s++) {
								for (size_t c = 0; c < 4; c++) {
									accu[c] += convolveFactor * currLineBuf[c] / onDiv;
								}
								currLineBuf += channels;
							}

							for (size_t c = 0; c < 4; c++) {
								destPtr[c] = accu[c] * writeBackFac;
							}
						}


						// for (size_t c = 0; c < channels; c++) {
						// 	destPtr[c] = lineBuf[( (writePos+(convolveCount/2+1) ) % convolveCount)*channels+c];
						// }
						
						destPtr += channels;

						size_t prevPos = writePos;
						writePos = (prevPos+1)%convolveCount;

						if (lineLeft > 0) {

							for (size_t c = 0; c < 4; c++) {
#if DEBUG_COLOR_MODE == 1
								lineBuf[writePos*channels+c] = (buff_val) GREEN[c] / preDiv;
#else
								lineBuf[writePos*channels+c] = (buff_val) srcPtr[c] / preDiv;
#endif
							}
							lineLeft--;
							srcPtr += 4;
						} else {
							for (size_t c = 0; c < 4; c++) {
#if DEBUG_COLOR_MODE == 1
								lineBuf[writePos*channels+c] = (buff_val) BLUE[c] / preDiv;
#else
								lineBuf[writePos*channels+c] = lineBuf[prevPos*channels+c];
#endif
							}
						}
					}
				}

				/* for (size_t i = 0; i < dataSize; i++) {

					

					// dest[i] = (srcA[i]>>4)*(srcB[i]>>4);
					switch (i % 4) {
						case 0: dest[i] = scale; break;
						case 1: dest[i] = direction; break;
						case 2: dest[i] = strength; break;
						case 3: dest[i] = 0xFF; break;
					}
					// *dest++ = ((uint16_t) *srcA++ * *srcB++) >> 8;
				}*/

				if (doBench) li.logger->log(torasu::LogLevel::DEBUG,
												"Blur Time = " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count()) + "[ms]");


				return rh.buildResult(result);
			} else {

				if (rh.mayLog(torasu::WARN)) {
					torasu::tools::LogInfoRefBuilder errorCauses(lirb.linstr);
					if (!dir)
						errorCauses.logCause(torasu::WARN, "Blur-direction failed to render", dir.takeInfoTag());
					if (!strgh)
						errorCauses.logCause(torasu::WARN, "Blur strength failed to render", strgh.takeInfoTag());

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
	elems["dir"] = direction.get();
	elems["strgh"] = strength.get();

	return elems;
}

void Rdirectional_blur::setElement(std::string key, torasu::Element* elem) {
	if (torasu::tools::trySetRenderableSlot("src", &src, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("dir", &direction, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("strgh", &strength, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

}