#ifndef INCLUDE_TORASU_MOD_IMGC_RIMG_FILE_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RIMG_FILE_HPP_

#include <mutex>
#include <vector>

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/slot_tools.hpp>

#include <torasu/std/spoilsD.hpp>
#include <torasu/std/pipeline_names.hpp>

namespace imgc {

class Rimg_file : public torasu::Renderable,
	public torasu::tools::NamedIdentElement,
	public torasu::tools::SimpleDataElement {
private:
	const std::string pipeline = std::string(TORASU_STD_PL_FILE);

	torasu::tools::ManagedRenderableSlot rfile;

private:
	void load(torasu::tools::RenderHelper* rh);

protected:
	torasu::ResultSegment* render(torasu::RenderInstruction* ri) override;

public:
	explicit Rimg_file(torasu::tools::RenderableSlot file);
	~Rimg_file();

	void ready(torasu::ReadyInstruction* ri) override;
	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RIMG_FILE_HPP_
