#ifndef INCLUDE_TORASU_MOD_IMGC_RALIGN2D_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RALIGN2D_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/slot_tools.hpp>

#include <torasu/std/spoilsD.hpp>

namespace imgc {

class Ralign2d : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedRenderableSlot  rndSrc;
	torasu::tools::ManagedRenderableSlot rndAlign;

protected:
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:
	Ralign2d(torasu::RenderableSlot rndSrc, torasu::RenderableSlot rndAlign);
	~Ralign2d();
	torasu::Identifier getType() override;

	torasu::ElementMap getElements() override;
	const torasu::OptElementSlot setElement(std::string key, const torasu::ElementSlot* elem) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RALIGN2D_HPP_
