#include "../include/torasu/mod/imgc/Rrothombus.hpp"

#include <memory>

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dnum.hpp>

#include <torasu/mod/imgc/Dgraphics.hpp>

namespace imgc {

Rrothumbus::Rrothumbus(Renderable* roundVal)
	: SimpleRenderable("IMGC::RROTHUMBUS", false, true),
	  roundValRnd(roundVal) {}

Rrothumbus::~Rrothumbus() {}

torasu::ResultSegment* Rrothumbus::render(torasu::RenderInstruction* ri) {
	auto* resSettings = ri->getResultSettings();
	std::string pipeline = resSettings->getPipeline();
	if (pipeline == TORASU_STD_PL_VIS) {
		torasu::tools::RenderHelper rh(ri);

		// Check format

		if (!(dynamic_cast<Dgraphics_FORMAT*>(resSettings->getFromat()))) {
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
		}

		// Retrieve rounding

		double rounding;
		if (roundValRnd != nullptr) {

			torasu::ResultSettings numSettings(TORASU_STD_PL_NUM, nullptr);

			std::unique_ptr<torasu::ResultSegment> rndRes(rh.runRender(roundValRnd, &numSettings));

			auto fetchedRes = rh.evalResult<torasu::tstd::Dnum>(rndRes.get());

			if (fetchedRes) {
				rounding = fetchedRes.getResult()->getNum();
			} else {
				rounding = 0;
				rh.lrib.hasError = true;
			}

		} else {
			rounding = 0;
		}

		// Create graphics

		double radius = 0.2; // TODO remove the radius this is just in for show as of now and should be removed once transformation-layers are availabe
		auto unit = rounding*radius*4*(sqrt(2)-1)/3;

		auto* graphics = new Dgraphics({
			Dgraphics::GObject(
				Dgraphics::GShape(
			Dgraphics::GSection({
				{
					{.5-radius,.5},
					{.5-radius,.5-unit},
					{.5-unit,.5-radius},
					{.5,.5-radius},
				},
				{
					{.5,.5-radius},
					{.5+unit,.5-radius},
					{.5+radius,.5-unit},
					{.5+radius,.5},
				},
				{
					{.5+radius,.5},
					{.5+radius,.5+unit},
					{.5+unit,.5+radius},
					{.5,.5+radius},
				},
				{
					{.5,.5+radius},
					{.5-unit,.5+radius},
					{.5-radius,.5+unit},
					{.5-radius,.5},
				},
			}),
			{
				{0,0}, {0,1}, {1,1}, {1,0}
			}
				))});

		return rh.buildResult(graphics);
	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}
}

torasu::ElementMap Rrothumbus::getElements() {
	torasu::ElementMap elems;
	if (roundValRnd) {
		elems["round"] = roundValRnd;
	}
	return elems;
}

void Rrothumbus::setElement(std::string key, torasu::Element* elem) {
	if (torasu::tools::trySetRenderableSlot("round", &roundValRnd, true, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}


} // namespace imgc
