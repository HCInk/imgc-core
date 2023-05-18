#ifndef INCLUDE_TORASU_MOD_IMGC_RCROPDATA_COMBINED_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RCROPDATA_COMBINED_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/slot_tools.hpp>

#include <torasu/std/Rnum.hpp>

namespace imgc {

class Rcropdata_combined : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> leftRnd;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> rightRnd;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> topRnd;
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> bottomRnd;

protected:
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:
	Rcropdata_combined(torasu::tstd::NumSlot left, torasu::tstd::NumSlot right, torasu::tstd::NumSlot top, torasu::tstd::NumSlot bottom);
	~Rcropdata_combined();
	torasu::Identifier getType() override;

	torasu::ElementMap getElements() override;
	const torasu::OptElementSlot setElement(std::string key, const torasu::ElementSlot* elem) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RCROPDATA_COMBINED_HPP_
