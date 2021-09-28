#ifndef INCLUDE_TORASU_MOD_IMGC_RTEXT_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RTEXT_HPP_

#include <torasu/torasu.hpp>
#include <torasu/slot_tools.hpp>
#include <torasu/std/Rstring.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rtext : public torasu::Renderable, public torasu::tools::SimpleDataElement {
private:
	torasu::tools::ManagedSlot<torasu::tstd::StringSlot> textRnd;

protected:
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:
	Rtext(torasu::tstd::StringSlot roundVal);
	~Rtext();
	torasu::Identifier getType() override;

	void ready(torasu::ReadyInstruction* ri) override;

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_RTEXT_HPP_
