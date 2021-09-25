#ifndef INCLUDE_TORASU_MOD_IMGC_RROTHOMBUS_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RROTHOMBUS_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rrothumbus : public torasu::tools::SimpleRenderable {
private:
	Renderable* roundValRnd = nullptr;

protected:
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:
	Rrothumbus(Renderable* roundVal);
	~Rrothumbus();
	torasu::Identifier getType() override;

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_RROTHOMBUS_HPP_
