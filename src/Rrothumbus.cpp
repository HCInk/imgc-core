#include "../include/torasu/mod/imgc/Rrothombus.hpp"

#include <memory>

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dnum.hpp>

#include <torasu/mod/imgc/Dgraphics.hpp>

namespace imgc {

Rrothumbus::Rrothumbus(Renderable* roundVal)
	: SimpleRenderable(false, true), roundValRnd(roundVal) {}

Rrothumbus::~Rrothumbus() {}

torasu::Identifier Rrothumbus::getType() {
	return "IMGC::RROTHUMBUS";
}

torasu::RenderResult* Rrothumbus::render(torasu::RenderInstruction* ri) {
	auto* resSettings = ri->getResultSettings();
	if (resSettings->getPipeline() == TORASU_STD_PL_VIS) {
		torasu::tools::RenderHelper rh(ri);

		// Check format

		if (!rh.getFormat<Dgraphics_FORMAT>()) {
			return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_FORMAT);
		}

		// Retrieve rounding

		double rounding;
		if (roundValRnd) {

			torasu::ResultSettings numSettings(TORASU_STD_PL_NUM, torasu::tools::NO_FORMAT);

			std::unique_ptr<torasu::RenderResult> rndRes(rh.runRender(roundValRnd, &numSettings));

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
		return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_SEGMENT);
	}
}

torasu::ElementMap Rrothumbus::getElements() {
	torasu::ElementMap elems;
	if (roundValRnd) elems["round"] = roundValRnd;
	return elems;
}

const torasu::OptElementSlot Rrothumbus::setElement(std::string key, const torasu::ElementSlot* elem) {
	if (key == "round") return torasu::tools::trySetRenderableSlot(&roundValRnd, elem);
	return nullptr;
}


} // namespace imgc
