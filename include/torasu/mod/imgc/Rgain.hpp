#ifndef INCLUDE_TORASU_MOD_IMGC_RGAIN_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RGAIN_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>

namespace imgc {
	
class Rgain : public torasu::tools::SimpleRenderable {
private:
	const std::string visPipeline = std::string(TORASU_STD_PL_VIS);
	const std::string numPipeline = std::string(TORASU_STD_PL_NUM);

	torasu::Renderable* rSrc;
	torasu::Renderable* rGainVal;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri);

public:
	Rgain(Renderable* src, Renderable* gainVal);
	~Rgain();

	virtual std::map<std::string, Element*> getElements();
	virtual void setElement(std::string key, Element* elem);
};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_RGAIN_HPP_
