#ifndef INCLUDE_TORASU_MOD_IMGC_RGAIN_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RGAIN_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/slot_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Rnum.hpp>

namespace imgc {

class Rgain : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedRenderableSlot rSrc;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> rGainVal;

protected:
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:
	Rgain(torasu::tools::RenderableSlot src, torasu::tstd::NumSlot gainVal);
	~Rgain();
	torasu::Identifier getType() override;

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_RGAIN_HPP_
