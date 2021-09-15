#include "../include/torasu/mod/imgc/Rcropdata.hpp"

#include <string>
#include <map>

#include <torasu/torasu.hpp>
#include <torasu/DataPackable.hpp>

#include <torasu/mod/imgc/pipeline_names.hpp>

namespace imgc {

Rcropdata::Rcropdata(Dcropdata val) : SimpleRenderable("IMGC::RCROPDATA", true, false), val(new Dcropdata(val)) {}

Rcropdata::~Rcropdata() {
	delete val;
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

torasu::ResultSegment* Rcropdata::render(torasu::RenderInstruction* ri) {
	if (strcmp(ri->getResultSettings()->getPipeline(), IMGC_PL_ALIGN) == 0) {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, val, false);
	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}
}

} // namespace imgc
