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

class Rimg_file : public torasu::tools::SimpleRenderable {
private:
	const std::string pipeline = std::string(TORASU_STD_PL_FILE);

	torasu::tools::ManagedRenderableSlot rfile;

	torasu::tools::RenderInstructionBuilder rib;
	torasu::tools::RenderResultSegmentHandle<torasu::tstd::Dfile> resHandle;

	std::mutex loadLock;
	bool loaded = false;
	std::vector<uint8_t> loadedImage;
	uint32_t srcWidth, srcHeight;

private:
	void load(torasu::RenderContext* rctx, torasu::ExecutionInterface* ei);

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;

public:
	explicit Rimg_file(torasu::tools::RenderableSlot file);
	~Rimg_file();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RIMG_FILE_HPP_
