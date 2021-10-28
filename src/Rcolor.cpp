#include "../include/torasu/mod/imgc/Rcolor.hpp"

#include <torasu/render_tools.hpp>

#include <torasu/std/Dbimg.hpp>
#include <torasu/std/pipeline_names.hpp>

namespace {
static const auto IDENT = "IMGC::RCOLOR";
} // namespace

namespace imgc {

Rcolor::Rcolor(torasu::tstd::NumSlot r, torasu::tstd::NumSlot g, torasu::tstd::NumSlot b, torasu::tstd::NumSlot a)
	: SimpleRenderable(false, true), rSrc(r), gSrc(g), bSrc(b), aSrc(a) {}

Rcolor::~Rcolor() {}

torasu::Identifier Rcolor::getType() {
	return IDENT;
}

torasu::RenderResult* Rcolor::render(torasu::RenderInstruction* ri) {
	torasu::tools::RenderHelper rh(ri);
	auto& lirb = rh.lrib;

	if (ri->getResultSettings()->getPipeline() == TORASU_STD_PL_VIS) {

		torasu::tstd::Dbimg_FORMAT* fmt;
		if (!(fmt = rh.getFormat<torasu::tstd::Dbimg_FORMAT>())) {
			return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_FORMAT);
		}

		torasu::ResultSettings rsNum(TORASU_STD_PL_NUM, torasu::tools::NO_FORMAT);
		// Sub-Renderings

		auto rendR = rh.enqueueRender(rSrc, &rsNum);
		auto rendG = rh.enqueueRender(gSrc, &rsNum);
		auto rendB = rh.enqueueRender(bSrc, &rsNum);
		auto rendA = rh.enqueueRender(aSrc, &rsNum);

		std::unique_ptr<torasu::RenderResult> resR(rh.fetchRenderResult(rendR));
		std::unique_ptr<torasu::RenderResult> resG(rh.fetchRenderResult(rendG));
		std::unique_ptr<torasu::RenderResult> resB(rh.fetchRenderResult(rendB));
		std::unique_ptr<torasu::RenderResult> resA(rh.fetchRenderResult(rendA));

		// Calculating Result from Results

		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dnum> r = rh.evalResult<torasu::tstd::Dnum>(resR.get());
		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dnum> g = rh.evalResult<torasu::tstd::Dnum>(resG.get());
		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dnum> b = rh.evalResult<torasu::tstd::Dnum>(resB.get());
		torasu::tools::CastedRenderSegmentResult<torasu::tstd::Dnum> a = rh.evalResult<torasu::tstd::Dnum>(resA.get());

		double rVal, gVal, bVal, aVal;

		if (r) {
			rVal = r.getResult()->getNum();
		} else {
			rVal = 0;
			if (rh.mayLog(torasu::WARN)) {
				lirb.logCause(torasu::WARN, "Red-value failed to render defaulting to 0", r.takeInfoTag());
			}
		}

		if (g) {
			gVal = g.getResult()->getNum();
		} else {
			gVal = 0;
			if (rh.mayLog(torasu::WARN)) {
				lirb.logCause(torasu::WARN, "Green-value failed to render defaulting to 0", g.takeInfoTag());
			}
		}

		if (b) {
			bVal = b.getResult()->getNum();
		} else {
			bVal = 0;
			if (rh.mayLog(torasu::WARN)) {
				lirb.logCause(torasu::WARN, "Blue-value failed to render defaulting to 0", b.takeInfoTag());
			}
		}

		if (a) {
			aVal = a.getResult()->getNum();
		} else {
			aVal = 1;
			if (rh.mayLog(torasu::WARN)) {
				lirb.logCause(torasu::WARN, "Alpha-value failed to render defaulting to 1", a.takeInfoTag());
			}
		}

		torasu::tstd::Dbimg* result = new torasu::tstd::Dbimg(*fmt);

		struct {
			uint8_t r;
			uint8_t g;
			uint8_t b;
			uint8_t a;
		} color {
			static_cast<uint8_t>(rVal * 0xFF),
			static_cast<uint8_t>(gVal * 0xFF),
			static_cast<uint8_t>(bVal * 0xFF),
			static_cast<uint8_t>(aVal * 0xFF)
		};

		uint32_t copyVal = *reinterpret_cast<uint32_t*>(&color);

		uint32_t* dataPtr = reinterpret_cast<uint32_t*>(result->getImageData());
		size_t pixelCount = result->getHeight()*result->getWidth();

		std::fill_n(dataPtr, pixelCount, copyVal);

		return rh.buildResult(result);

	} else {
		return new torasu::RenderResult(torasu::RenderResultStatus::RenderResultStatus_NON_EXISTENT);
	}

}

torasu::ElementMap Rcolor::getElements() {
	torasu::ElementMap em;
	em["r"] = rSrc.get();
	em["g"] = gSrc.get();
	em["b"] = bSrc.get();
	em["a"] = aSrc.get();
	return em;
}

void Rcolor::setElement(std::string key, Element* elem) {
	if (torasu::tools::trySetRenderableSlot("r", &rSrc, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("g", &gSrc, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("b", &bSrc, false, key, elem)) return;
	if (torasu::tools::trySetRenderableSlot("a", &aSrc, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}


namespace {

static const torasu::ElementFactory::SlotDescriptor SLOT_INDEX[] = {
	{.id = "r", .label = {.name = "Red", .description = "Red component of color"}, .optional = false, .renderable = true},
	{.id = "g", .label = {.name = "Green", .description = "Green component of color"}, .optional = false, .renderable = true},
	{.id = "b", .label = {.name = "Blue", .description = "Blue component of color"}, .optional = false, .renderable = true},
	{.id = "a", .label = {.name = "Alpha", .description = "Alpha component of color"}, .optional = false, .renderable = true},
};

static class : public torasu::ElementFactory {
	torasu::Identifier getType() const override {
		return IDENT;
	}

	torasu::UserLabel getLabel() const override {
		return {
			.name = "Color",
			.description = "Defines a color"
		};
	}

	torasu::Element* create(torasu::DataResource** data, const torasu::ElementMap& elements) const override {
		std::unique_ptr<Rcolor> elem(new Rcolor(1.0,1.0,1.0,1.0));
		for (auto slot : elements) {
			elem->setElement(slot.first, slot.second);
		}
		return elem.release();
	}

	SlotIndex getSlotIndex() const override {
		return {.slotIndex = SLOT_INDEX, .slotCount = sizeof(SLOT_INDEX)/sizeof(ElementFactory::SlotDescriptor)};
	}
} FACTORY_INSTANCE;

} // namespace

const torasu::ElementFactory* const Rcolor::FACTORY = &FACTORY_INSTANCE;

} // namespace imgc