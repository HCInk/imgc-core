#include "../include/torasu/mod/imgc/Rgraphics.hpp"

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>

#include <torasu/mod/imgc/Dgraphics.hpp>
#include <torasu/mod/imgc/ShapeRenderer.hpp>

namespace imgc {

Rgraphics::Rgraphics() 
	: SimpleRenderable("IMGC::RGRAPHICS", false, false) {

}

Rgraphics::~Rgraphics() {}

torasu::ResultSegment* Rgraphics::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {

	if (resSettings->getPipeline() == TORASU_STD_PL_VIS) {

		if (auto* fmt = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(resSettings->getResultFormatSettings())) {
			
			Renderable* graphicsSrc = this;

			torasu::tools::RenderInstructionBuilder rib;
			Dgraphics_FORMAT graphicsFmt;
			auto resHandle = rib.addSegmentWithHandle<imgc::Dgraphics>(TORASU_STD_PL_VIS, &graphicsFmt);

			auto ei = ri->getExecutionInterface();

			auto rid = rib.enqueueRender(graphicsSrc, ri->getRenderContext(), ei);

			std::unique_ptr<torasu::RenderResult> res(ei->fetchRenderResult(rid));

			auto* graphics = resHandle.getFrom(res.get()).getResult();

			if (graphics == nullptr) {
				throw std::runtime_error("Error rendering graphics-source!");
			}

			torasu::tstd::Dbimg* base;

			if (graphics->getObjects().size() > 0) {
				if (graphics->getObjects().size() > 1) {
					throw std::runtime_error("Rendering multiple objects is currently unsupported!");
				}
				auto& object = *graphics->getObjects().begin();
				base = ShapeRenderer::render(*fmt, object.shape);
			} else {
				base = new torasu::tstd::Dbimg(*fmt);
				uint32_t* data = reinterpret_cast<uint32_t*>(base->getImageData());
				std::fill(data, 
						data+(base->getWidth()*base->getHeight()),
						0x000000);
			}

			return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, base, true);
		} else if (auto* fmt = dynamic_cast<imgc::Dgraphics_FORMAT*>(resSettings->getResultFormatSettings())) {

			double radius = 0.2;
			auto unit = 8*radius*4*(sqrt(2)-1)/3;

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

			return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, graphics, true);

		} else {
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
		}



	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}

}

} // namespace imgc
