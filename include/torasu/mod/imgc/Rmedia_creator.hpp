#ifndef INCLUDE_TORASU_MOD_IMGC_RMEDIA_CREATOR_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RMEDIA_CREATOR_HPP_

#include <string>

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>
#include <torasu/slot_tools.hpp>

#include <torasu/std/Rnum.hpp>
#include <torasu/std/Rstring.hpp>

namespace imgc {

class Rmedia_creator : public torasu::tools::SimpleRenderable {
private:
	torasu::tools::ManagedRenderableSlot srcRnd; // VIS/AUD - A/V Source
	torasu::tools::ManagedSlot<torasu::tstd::StringSlot> formatRnd; // STR - format name, e.g. mp4
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> beginRnd; // NUM - begin of clip in seconds
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> endRnd; // NUM - end of clip in seconds
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> fpsRnd; // NUM - FPS of video (0 for no video)
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> widthRnd; // NUM - Width of video in pixels
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> heightRnd; // NUM - Height of video in pixels
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> videoBitrateRnd; // NUM - Bitrate of video
	torasu::tools::ManagedSlot<torasu::tstd::NumSlot> audioMinSampleRateRnd; // NUM - Minimum sample-rate of audio (-1 for no audio)
	torasu::tools::ManagedRenderableSlot metadataSlot; // MAP - Metadata-map

protected:
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:
	Rmedia_creator(torasu::RenderableSlot src, torasu::tstd::StringSlot format,
				   torasu::tstd::NumSlot begin, torasu::tstd::NumSlot end, torasu::tstd::NumSlot fps = 0.0,
				   torasu::tstd::NumSlot width = 0.0, torasu::tstd::NumSlot height = 0.0,
				   torasu::tstd::NumSlot videoBitrate = 0.0, torasu::tstd::NumSlot audioMinSampleRate = -1,
				   torasu::RenderableSlot metadata = nullptr);
	~Rmedia_creator();
	torasu::Identifier getType() override;

	torasu::ElementMap getElements() override;
	const torasu::OptElementSlot setElement(std::string key, const torasu::ElementSlot* elem) override;

};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RMEDIA_CREATOR_HPP_
