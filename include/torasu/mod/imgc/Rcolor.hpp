#ifndef INCLUDE_TORASU_MOD_IMGC_RCOLOR_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RCOLOR_HPP_

#include <string>

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/std/Rnum.hpp>

namespace imgc {

class Rcolor : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> rSrc;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> gSrc;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> bSrc;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> aSrc;

protected:
	torasu::ResultSegment* render(torasu::RenderInstruction* ri) override;

public:
	Rcolor(torasu::tstd::NumSlot r, torasu::tstd::NumSlot g, torasu::tstd::NumSlot b, torasu::tstd::NumSlot a);
	~Rcolor();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RCOLOR_HPP_
