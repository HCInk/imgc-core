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

class Rimg_file_readyobj : public torasu::ReadyState {
private:
	std::vector<torasu::Identifier>* ops;
	const torasu::RenderContextMask* rctxm;
protected:
	std::vector<uint8_t> loadedImage;
	uint32_t srcWidth, srcHeight;

	Rimg_file_readyobj(std::vector<torasu::Identifier>* ops, const torasu::RenderContextMask* rctxm)
		: ops(ops), rctxm(rctxm) {}
public:
	~Rimg_file_readyobj() {
		delete ops;
		if (rctxm != nullptr) delete rctxm;
	}

	virtual const std::vector<torasu::Identifier>* getOperations() const override {
		return ops;
	}

	virtual const torasu::RenderContextMask* getContextMask() const override {
		return rctxm;
	}

	virtual size_t size() const override {
		return loadedImage.size() + sizeof(Rimg_file_readyobj);
	}

	virtual ReadyState* clone() const override {
		// TODO implement clone
		throw new std::runtime_error("cloning currently unsupported");
	}

	friend Rimg_file;
};


void Rimg_file::ready(torasu::ReadyInstruction* ri) {

	torasu::tools::RenderHelper rh(ri);

	if (rfile.get() == nullptr) throw std::logic_error("Source renderable set loaded yet!");


	// Render File

	torasu::ResultSettings rs(TORASU_STD_PL_FILE, torasu::tools::NO_FORMAT);

	std::unique_ptr<torasu::RenderResult> rr(rh.runRender(rfile.get(), &rs));

	auto fileRes = rh.evalResult<torasu::tstd::Dfile>(rr.get());

	if (!fileRes) {
		// Stop because of file-render-error
		throw std::runtime_error("Failed to fetch source-file");
	}

	auto* obj = new Rimg_file_readyobj(
		new std::vector<torasu::Identifier>({TORASU_STD_PL_VIS,
											TORASU_PROPERTY(TORASU_STD_PROP_IMG_WIDTH), TORASU_PROPERTY(TORASU_STD_PROP_IMG_HEIGHT),
											TORASU_PROPERTY(TORASU_STD_PROP_IMG_RAITO)
											}),
		rh.takeResMask());

	ri->setState(obj);


	// Decode

	Dfile* file = fileRes.getResult();
	uint32_t error = lodepng::decode(obj->loadedImage, obj->srcWidth, obj->srcHeight, file->getFileData(), file->getFileSize());

	if (error) {
		// Stop because file-decoding error
		throw std::runtime_error(std::string("DECODE ERROR #") + std::to_string(error));
	}

}


Rimg_file::Rimg_file(torasu::RenderableSlot file)
	: SimpleDataElement(false, true),
	  rfile(file) {}

Rimg_file::~Rimg_file() {}

torasu::Identifier Rimg_file::getType() {
	return "STD::RIMG_FILE";
}

RenderResult* Rimg_file::render(RenderInstruction* ri) {
	torasu::tools::RenderHelper rh(ri);

	auto* state = static_cast<Rimg_file_readyobj*>(ri->getReadyState());

	if (rh.matchPipeline(TORASU_STD_PL_VIS)) {
		int32_t rWidth, rHeight;
		if ((*rh.rs->getFormats()) == nullptr) {
			rWidth = -1;
			rHeight = -1;
		} else {
			if (auto* bimgFormat = rh.getFormat<torasu::tstd::Dbimg_FORMAT>()) {
				rWidth = bimgFormat->getWidth();
				rHeight = bimgFormat->getHeight();
				// cout << "RIMG RENDER " << rWidth << "x" << rHeight << endl;
			} else {
				return new RenderResult(RenderResultStatus_INVALID_FORMAT);
			}
		}

		if (rWidth == 0 || rHeight == 0) {
			return new RenderResult(RenderResultStatus_OK, new Dbimg(rWidth, rHeight), true);
		}

		if (rWidth < 0) {
			rWidth = state->srcWidth;
			rHeight = state->srcHeight;
		}

		Dbimg_FORMAT fmt(rWidth, rHeight);

		Dbimg* resultImage = scaler::scaleImg(state->loadedImage.data(), state->srcWidth, state->srcHeight, &fmt, true);

		return new RenderResult(RenderResultStatus_OK, resultImage, true);


	} else if (rh.isProperty()) { // optional so properties get skipped if it is no property
		if (rh.matchProperty(TORASU_STD_PROP_IMG_WIDTH)) {
			return new torasu::RenderResult(torasu::RenderResultStatus_OK, new torasu::tstd::Dnum(state->srcWidth), true);
		} else if (rh.matchProperty(TORASU_STD_PROP_IMG_HEIGHT)) {
			return new torasu::RenderResult(torasu::RenderResultStatus_OK, new torasu::tstd::Dnum(state->srcHeight), true);
		} else if (rh.matchProperty(TORASU_STD_PROP_IMG_RAITO)) {
			return new torasu::RenderResult(torasu::RenderResultStatus_OK, new torasu::tstd::Dnum((double) state->srcWidth / state->srcHeight), true);
		} else {
			// Unsupported Property
			return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_SEGMENT);
		}
	} else {
		return new RenderResult(RenderResultStatus_INVALID_SEGMENT);
	}

}

torasu::ElementMap Rimg_file::getElements() {
	torasu::ElementMap elems;
	elems["f"] = rfile;
	return elems;
}

const torasu::OptElementSlot Rimg_file::setElement(std::string key, const ElementSlot* elem) {
	if (key == "f") return torasu::tools::trySetRenderableSlot(&rfile, elem);
	return nullptr;
}

}
