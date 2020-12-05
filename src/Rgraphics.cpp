#include "../include/torasu/mod/imgc/Rgraphics.hpp"

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
		auto* fmt = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(resSettings->getResultFormatSettings());
		if (!fmt) {
			return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_FORMAT);
		}

		double radius = 0.2;
		auto unit = 8*radius*4*(sqrt(2)-1)/3;

		Dgraphics::GShape shape (
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
		);

		torasu::tstd::Dbimg* base = ShapeRenderer::render(*fmt, shape);

		return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, base, true);

	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}

}

} // namespace imgc
