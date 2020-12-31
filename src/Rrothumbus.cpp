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

torasu::ResultSegment* Rrothumbus::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {
	std::string pipeline = resSettings->getPipeline();
	if (pipeline == TORASU_STD_PL_VIS) {
		
		if (!(dynamic_cast<Dgraphics_FORMAT*>(resSettings->getResultFormatSettings()))) {
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
		}

		double rounding;
		bool failGetRounding = false;
		if (roundValRnd != nullptr) {

			auto* ei = ri->getExecutionInterface();
			auto* rctx = ri->getRenderContext();

			// Sub-renderings

			torasu::tools::RenderInstructionBuilder rib;
			auto segHandle = rib.addSegmentWithHandle<torasu::tstd::Dnum>(TORASU_STD_PL_NUM, nullptr);

			auto renderId = rib.enqueueRender(roundValRnd, rctx, ei);

			std::unique_ptr<torasu::RenderResult> rndRes(ei->fetchRenderResult(renderId));

			auto fetchedRes = segHandle.getFrom(rndRes.get());
			
			if (fetchedRes.getResult() != nullptr) {
				rounding = fetchedRes.getResult()->getNum();
			} else {
				rounding = 0;
				failGetRounding = true;
			}

		} else {
			rounding = 0;
		}
		// Processing
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

		return new torasu::ResultSegment(
			!failGetRounding ? torasu::ResultSegmentStatus_OK :torasu::ResultSegmentStatus_OK_WARN, 
			graphics, true);

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
