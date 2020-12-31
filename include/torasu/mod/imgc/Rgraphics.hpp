#ifndef INCLUDE_TORASU_MOD_IMGC_RGRAPHICS_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RGRAPHICS_HPP_

#include <memory>

#include <torasu/torasu.hpp>
#include <torasu/mod/imgc/Dgraphics.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rgraphics : public torasu::tools::SimpleRenderable {
private:
	std::unique_ptr<Dgraphics> graphics;
	Renderable* source;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;

public:
	Rgraphics(Dgraphics* graphics);
	Rgraphics(Renderable* graphics);
	~Rgraphics();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;

	torasu::DataResource* getData() override;
	void setData(torasu::DataResource* data) override;
};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_RGRAPHICS_HPP_
