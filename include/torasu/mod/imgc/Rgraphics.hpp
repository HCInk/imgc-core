#ifndef INCLUDE_TORASU_MOD_IMGC_RGRAPHICS_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RGRAPHICS_HPP_

#include <memory>

#include <torasu/torasu.hpp>
#include <torasu/slot_tools.hpp>
#include <torasu/mod/imgc/Dgraphics.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rgraphics : public torasu::tools::SimpleRenderable {
private:
	std::unique_ptr<Dgraphics> graphics;
	torasu::tools::ManagedRenderableSlot source;

protected:
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:
	Rgraphics(Dgraphics* graphics);
	Rgraphics(Renderable* graphics);
	~Rgraphics();
	torasu::Identifier getType() override;

	torasu::ElementMap getElements() override;
	const torasu::OptElementSlot setElement(std::string key, const torasu::ElementSlot* elem) override;

	torasu::DataResource* getData() override;
	void setData(torasu::DataResource* data) override;
};

} // namespace imgc


#endif // INCLUDE_TORASU_MOD_IMGC_RGRAPHICS_HPP_
