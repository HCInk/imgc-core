#ifndef INCLUDE_TORASU_MOD_IMGC_RMEDIA_CREATOR_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RMEDIA_CREATOR_HPP_

#include <string>

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rmedia_creator : public torasu::tools::SimpleRenderable {
private:
	Renderable* srcRnd;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;

public:
	Rmedia_creator(Renderable* srcRnd);
	~Rmedia_creator();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;

	// torasu::DataResource* getData() override;
	// void setData(torasu::DataResource* data) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RMEDIA_CREATOR_HPP_
