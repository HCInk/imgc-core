#include "../include/torasu/mod/imgc/Rimg_file.hpp"

#include <iostream>
#include <cmath>

#include <lodepng.h>


#include <torasu/torasu.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/property_names.hpp>
#include <torasu/std/Dstring.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>
#include <torasu/std/Dfile.hpp>
#include <torasu/mod/imgc/Scaler.hpp>

using namespace std;
using namespace torasu;
using namespace torasu::tstd;

namespace imgc {

Rimg_file::Rimg_file(Renderable* file)
	: SimpleRenderable("STD::RIMG_FILE", false, true),
	  resHandle(rib.addSegmentWithHandle<Dfile>(TORASU_STD_PL_FILE, NULL)) {
	this->rfile = file;
}

Rimg_file::~Rimg_file() {}

void Rimg_file::load(torasu::RenderContext* rctx, torasu::ExecutionInterface* ei) {

	loadLock.lock();

	if (!loaded) {

		RenderResult* fileRenderResult = rib.runRender(rfile, rctx, ei);

		auto fileRes = resHandle.getFrom(fileRenderResult);

		if (fileRes.getStatus() < ResultSegmentStatus_OK) {
			// Stop because of file-render-error
			throw std::runtime_error("Failed to fetch source-file");
		}

		Dfile* file = fileRes.getResult();

		uint32_t error = lodepng::decode(loadedImage, srcWidth, srcHeight, file->getFileData(), file->getFileSize());

		delete fileRenderResult;

		if (error) {
			// Stop because file-decoding error
			throw std::runtime_error(std::string("DECODE ERROR #") + std::to_string(error));
		}

		loaded = true;
	}

	loadLock.unlock();
}

ResultSegment* Rimg_file::renderSegment(ResultSegmentSettings* resSettings, RenderInstruction* ri) {
	
	auto ei = ri->getExecutionInterface();
	auto rctx = ri->getRenderContext();

	std::string currentPipeline = resSettings->getPipeline();

	if (currentPipeline == TORASU_STD_PL_VIS) {
		auto format = resSettings->getResultFormatSettings();
		if (format == NULL || format->getFormat().compare("STD::DBIMG") == 0) {
			int32_t rWidth, rHeight;
			if (format == NULL) {
				rWidth = -1;
				rHeight = -1;
			} else {
				DataResource* fmtData = format->getData();
				Dbimg_FORMAT* bimgFormat;
				if (!(bimgFormat = dynamic_cast<Dbimg_FORMAT*>(fmtData))) {
					return new ResultSegment(ResultSegmentStatus_INVALID_FORMAT);
				}
				rWidth = bimgFormat->getWidth();
				rHeight = bimgFormat->getHeight();
				cout << "RIMG RENDER " << rWidth << "x" << rHeight << endl;
			}

			load(rctx, ei);

			if (rWidth == 0 || rHeight == 0) {
				return new ResultSegment(ResultSegmentStatus_OK, new Dbimg(rWidth, rHeight), true);
			}

			if (rWidth < 0) {
				rWidth = srcWidth;
				rHeight = srcHeight;
			}


			Dbimg_FORMAT fmt(rWidth, rHeight);

			Dbimg* resultImage = scaler::scaleImg(loadedImage.data(), srcWidth, srcHeight, &fmt, true);

			return new ResultSegment(ResultSegmentStatus_OK, resultImage, true);

		} else {
			return new ResultSegment(ResultSegmentStatus_INVALID_FORMAT);
		}

	} else if (torasu::isPipelineKeyPropertyKey(currentPipeline)) { // optional so properties get skipped if it is no property
		if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_IMG_WIDTH)) {
			load(rctx, ei);
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, new torasu::tstd::Dnum(srcWidth), true);
		} else if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_IMG_HEIGHT)) {
			load(rctx, ei);
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, new torasu::tstd::Dnum(srcHeight), true);
		} else if (currentPipeline == TORASU_PROPERTY(TORASU_STD_PROP_IMG_RAITO)) {
			load(rctx, ei);
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, new torasu::tstd::Dnum((double) srcWidth / srcHeight), true);
		} else {
			// Unsupported Property
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
		}
	} else {
		return new ResultSegment(ResultSegmentStatus_INVALID_SEGMENT);
	}
}

std::map<std::string, Element*> Rimg_file::getElements() {
	std::map<std::string, Element*> elems;
	elems["f"] = rfile;
	return elems;
}

void Rimg_file::setElement(std::string key, Element* elem) {
	if (torasu::tools::trySetRenderableSlot("f", &rfile, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}

}
