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

torasu::Identifier Rgraphics::getType() {
	return "IMGC::RGRAPHICS";
}

torasu::RenderResult* Rgraphics::render(torasu::RenderInstruction* ri) {
	torasu::tools::RenderHelper rh(ri);

	if (ri->getResultSettings()->getPipeline() == TORASU_STD_PL_VIS) {
		auto formats = rh.getFormats<imgc::Dgraphics_FORMAT, torasu::tstd::Dbimg_FORMAT>();
		if (formats.primary != nullptr) {
			return new torasu::RenderResult(torasu::RenderResultStatus_OK, graphics.get(), false);
		} else if (formats.secondary != nullptr) {
			auto* fmt = formats.secondary;
			Dgraphics_FORMAT graphicsFmt;
			torasu::tools::ResultSettingsSingleFmt rsGraphics(TORASU_STD_PL_VIS, &graphicsFmt);


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
		} else {
			return rh.buildResult(torasu::RenderResultStatus_INVALID_FORMAT);
		}

	}

	return rh.passRender(source.get(), torasu::tools::RenderHelper::PassMode_DEFAULT);
}

torasu::ElementMap Rgraphics::getElements() {
	torasu::ElementMap map;

	if (graphics == nullptr || source.get() != nullptr) {
		map["src"] = source;
	}

	return map;
}

const torasu::OptElementSlot Rgraphics::setElement(std::string key, const torasu::ElementSlot* elem) {
	if (key == "src") return torasu::tools::trySetRenderableSlot(&source, elem);
	return nullptr;
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
