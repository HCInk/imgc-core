#ifndef INCLUDE_TORASU_MOD_IMGC_RIMG_FILE_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RIMG_FILE_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

#include <torasu/render_tools.hpp>
#include <torasu/std/spoilsD.hpp>
#include <torasu/std/pipeline_names.hpp>

namespace imgc {

class Rimg_file : public torasu::tools::SimpleRenderable {
private:
	const std::string pipeline = std::string(TORASU_STD_PL_FILE);
	
	torasu::Renderable* rfile;

	torasu::tools::RenderInstructionBuilder rib;
	torasu::tools::RenderResultSegmentHandle<torasu::tstd::Dfile> resHandle;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri);

public:
	explicit Rimg_file(Renderable* file);
	~Rimg_file();

	virtual std::map<std::string, Element*> getElements();
	virtual void setElement(std::string key, Element* elem);
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RIMG_FILE_HPP_
