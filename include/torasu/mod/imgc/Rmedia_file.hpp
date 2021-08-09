#ifndef INCLUDE_TORASU_MOD_IMGC_RMEDIA_FILE_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RMEDIA_FILE_HPP_


#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/slot_tools.hpp>

#include <torasu/std/spoilsD.hpp>


namespace imgc {

// Spoil for MediaDecoder so <torasu/mod/imgc/MediaDecoder.hpp> doesnt have to be included in the header
// Reason for avoiding include: Heavy FFmpeg-headers
class MediaDecoder;

class Rmedia_file :  public torasu::Renderable,
	public torasu::tools::NamedIdentElement,
	public torasu::tools::SimpleDataElement {
private:
	torasu::tools::ManagedRenderableSlot srcRnd;

	void load(torasu::ExecutionInterface* ei, torasu::LogInstruction li);

public:
	Rmedia_file(torasu::tools::RenderableSlot src);
	~Rmedia_file();

	void ready(torasu::ReadyInstruction* ri) override;
	torasu::ResultSegment* render(torasu::RenderInstruction* ri) override;

	torasu::ElementMap getElements() override;
	void setElement(std::string key, torasu::Element* elem) override;

};

}

#endif // INCLUDE_TORASU_MOD_IMGC_RMEDIA_FILE_HPP_
