#ifndef INCLUDE_TORASU_MOD_IMGC_RLAYER_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RLAYER_HPP_

#include <torasu/torasu.hpp>
#include <torasu/slot_tools.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rlayer : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedRenderableSlot layers;

protected:
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:
	Rlayer(torasu::RenderableSlot layers);
	~Rlayer();
	torasu::Identifier getType() override;

	torasu::ElementMap getElements() override;
	const torasu::OptElementSlot setElement(std::string key, const torasu::ElementSlot* elem) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RLAYER_HPP_
