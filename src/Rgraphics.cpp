#include "../include/torasu/mod/imgc/Rgraphics.hpp"

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>

#include <torasu/mod/imgc/ShapeRenderer.hpp>

namespace imgc {

Rgraphics::Rgraphics(Dgraphics* graphics)
	: SimpleRenderable(true, true),
	  graphics(graphics), source(this) {}


Rgraphics::Rgraphics(Renderable* graphics)
	: SimpleRenderable(true, true),
	  graphics(nullptr), source(graphics) {}

Rgraphics::~Rgraphics() {}

torasu::Identifier Rgraphics::getType() { return "IMGC::RGRAPHICS"; }

torasu::RenderResult* Rgraphics::render(torasu::RenderInstruction* ri) {
	torasu::tools::RenderHelper rh(ri);

	if (ri->getResultSettings()->getPipeline() == TORASU_STD_PL_VIS) {
		auto* reqFormat = ri->getResultSettings()->getFromat();

		if (auto* fmt = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(reqFormat)) {

			Dgraphics_FORMAT graphicsFmt;
			torasu::ResultSettings rsGraphics(TORASU_STD_PL_VIS, &graphicsFmt);


			auto rid = rh.enqueueRender(source, &rsGraphics);

			std::unique_ptr<torasu::RenderResult> res(rh.fetchRenderResult(rid));

			auto* graphics = rh.evalResult<imgc::Dgraphics>(res.get()).getResult();

			if (graphics == nullptr) {
				throw std::runtime_error("Error rendering graphics-source!");
			}

			torasu::tstd::Dbimg* base;

			if (graphics->getObjects().size() > 0) {
				if (graphics->getObjects().size() > 1) {
					throw std::runtime_error("Rendering multiple objects is currently unsupported!");
				}
				auto& object = *graphics->getObjects().begin();
				base = ShapeRenderer::render(*fmt, object.shape, ri->getLogInstruction());
			} else {
				base = new torasu::tstd::Dbimg(*fmt);
				uint32_t* data = reinterpret_cast<uint32_t*>(base->getImageData());
				std::fill(data,
						  data+(base->getWidth()*base->getHeight()),
						  0x000000);
			}

			return new torasu::RenderResult(torasu::RenderResultStatus_OK, base, true);
		} else if (dynamic_cast<imgc::Dgraphics_FORMAT*>(reqFormat)) {
			return new torasu::RenderResult(torasu::RenderResultStatus_OK, graphics.get(), false);
		}

	}

	// TODO proper way to pass a render result (while also preserving logs)
	std::unique_ptr<torasu::RenderResult> result(rh.runRender(source, rh.rs));
	return new torasu::RenderResult(result->getStatus(), result->ejectOrClone(), new torasu::RenderContextMask(*result->getResultMask()));
}

torasu::ElementMap Rgraphics::getElements() {
	torasu::ElementMap map;

	if (graphics != nullptr) {
		map["src"] = source;
	}

	return map;
}

void Rgraphics::setElement(std::string key, Element* elem) {
	if (key == "src") {

		if (elem == nullptr) {
			source = this;
		} else if(auto* rnd = dynamic_cast<Renderable*>(elem)) {
			source = rnd;
		} else {
			throw torasu::tools::makeExceptSlotOnlyRenderables(key);
		}

	} else {
		throw torasu::tools::makeExceptSlotDoesntExist(key);
	}
}

torasu::DataResource* Rgraphics::getData() {
	return graphics.get();
}

void Rgraphics::setData(torasu::DataResource* data) {
	if (data == nullptr) {
		this->graphics = nullptr;
	} else if (auto* graphics = dynamic_cast<Dgraphics*>(data)) {
		this->graphics = std::unique_ptr<Dgraphics>(graphics);
	} else {
		throw std::invalid_argument("Only data-type imgc::Dgraphics acceptable!");
	}
}

} // namespace imgc
