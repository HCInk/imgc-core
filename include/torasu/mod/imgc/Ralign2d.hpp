#ifndef INCLUDE_TORASU_MOD_IMGC_RALIGN2D_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RALIGN2D_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

#include <torasu/std/spoilsD.hpp>

namespace imgc {

class Ralign2d : public torasu::tools::SimpleRenderable {
private:
	Renderable* rndSrc;

	double posX; 
	double posY;
	double zoomFactor;
	double imageRatio;
	bool autoRatio;


	struct Ralign2d_CROPDATA {
		uint32_t srcWidth, srcHeight;
		int32_t offLeft, offRight, offTop, offBottom;
	};

	void calcAlign(double posX, double posY, double zoomFactor, bool autoRatio, double imageRatio,
					uint32_t destWidth, uint32_t destHeight, 
					Ralign2d_CROPDATA& outCropData) const;
	
	void align(torasu::tstd::Dbimg* srcImg, torasu::tstd::Dbimg* destImg, Ralign2d_CROPDATA* cropData) const;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;

public:
	Ralign2d(Renderable* rndSrc, double posX, double posY, double zoomFactor, double alignRatio);
	~Ralign2d();

	std::map<std::string, Element*> getElements() override;
	void setElement(std::string key, Element* elem) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RALIGN2D_HPP_
