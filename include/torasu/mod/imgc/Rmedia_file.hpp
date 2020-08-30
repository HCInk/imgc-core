#ifndef INCLUDE_TORASU_MOD_IMGC_RMEDIA_FILE_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RMEDIA_FILE_HPP_


#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/std/spoilsD.hpp>


namespace imgc {

// Spoil for MediaDecoder so <torasu/mod/imgc/MediaDecoder.hpp> doesnt have to be included in the header
// Reason for avoiding include: Heavy FFmpeg-headers
class MediaDecoder;

class Rmedia_file :  public torasu::Renderable,
	public torasu::tools::NamedIdentElement,
	public torasu::tools::SimpleDataElement {
private:
	torasu::Renderable* srcRnd;
	torasu::RenderResult* srcRendRes = NULL;
	torasu::tstd::Dfile* srcFile = NULL;
	MediaDecoder* decoder = NULL;

	void load(torasu::ExecutionInterface* ei);

public:
	Rmedia_file(Renderable* src = NULL);
	~Rmedia_file();

	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

	std::map<std::string, torasu::Element*> getElements() override;
	void setElement(std::string key, torasu::Element* elem) override;
	torasu::RenderableProperties* getProperties();

};

}

#endif // INCLUDE_TORASU_MOD_IMGC_RMEDIA_FILE_HPP_
