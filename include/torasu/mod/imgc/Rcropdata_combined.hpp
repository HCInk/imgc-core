#ifndef INCLUDE_TORASU_MOD_IMGC_RCROPDATA_COMBINED_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RCROPDATA_COMBINED_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rcropdata_combined : public torasu::tools::SimpleRenderable {
private:
	Renderable* leftRnd;
	Renderable* rightRnd;
	Renderable* topRnd;
	Renderable* bottomRnd;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;

public:
	Rcropdata_combined(Renderable* left, Renderable* right, Renderable* top, Renderable* bottom);
	~Rcropdata_combined();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RCROPDATA_COMBINED_HPP_
