#include "../include/torasu/mod/imgc/Rcropdata.hpp"

#include <string>
#include <map>

#include <torasu/torasu.hpp>
#include <torasu/DataPackable.hpp>

#include <torasu/mod/imgc/pipeline_names.hpp>

namespace imgc {

Rcropdata::Rcropdata(Dcropdata val) : SimpleRenderable(true, false), val(new Dcropdata(val)) {}

Rcropdata::~Rcropdata() {
	delete val;
}

torasu::Identifier Rcropdata::getType() {
	return "IMGC::RCROPDATA";
}

torasu::DataResource* Rcropdata::getData() {
	return val;
}

void Rcropdata::setData(torasu::DataResource* data) {
	if (auto* castedData = dynamic_cast<Dcropdata*>(data)) {
		delete val;
		val = castedData;
	} else {
		throw std::invalid_argument("The data-type \"Dcropdata\" is only allowed");
	}
}

torasu::RenderResult* Rcropdata::render(torasu::RenderInstruction* ri) {
	if (ri->getResultSettings()->getPipeline() == IMGC_PL_ALIGN) {
		return new torasu::RenderResult(torasu::RenderResultStatus_OK, val, false);
	} else {
		return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_SEGMENT);
	}
}

} // namespace imgc
