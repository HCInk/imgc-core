#ifndef INCLUDE_TORASU_MOD_IMGC_RROTHOMBUS_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RROTHOMBUS_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rrothumbus : public torasu::tools::SimpleRenderable {
private:
	Renderable* roundValRnd = nullptr;

protected:
	torasu::ResultSegment* render(torasu::RenderInstruction* ri) override;

public:
	Rrothumbus(Renderable* roundVal);
	~Rrothumbus();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_RROTHOMBUS_HPP_
