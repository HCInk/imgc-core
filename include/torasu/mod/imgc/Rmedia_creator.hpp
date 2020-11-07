#ifndef INCLUDE_TORASU_MOD_IMGC_RMEDIA_CREATOR_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RMEDIA_CREATOR_HPP_

#include <string>

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

namespace imgc {

class Rmedia_creator : public torasu::tools::SimpleRenderable {
private:
	Renderable* srcRnd; // VIS/AUD - A/V Source
	Renderable* formatRnd; // STR - format name, e.g. mp4
	bool ownsFormat = false;
	Renderable* beginRnd; // NUM - begin of clip in seconds
	bool ownsBegin = false;
	Renderable* endRnd; // NUM - end of clip in seconds
	bool ownsEnd = false;
	Renderable* fpsRnd; // NUM - FPS of video (0 for no video)
	bool ownsFps = false;
	Renderable* widthRnd; // NUM - Width of video in pixels
	bool ownsWidth = false;
	Renderable* heightRnd; // NUM - Height of video in pixels
	bool ownsHeight = false;
	Renderable* videoBitrateRnd; // NUM - Bitrate of video
	bool ownsVideoBitrate = false;
	Renderable* audioMinSampleRateRnd; // NUM - Minimum sample-rate of audio (-1 for no audio)
	bool ownsAudioMinSampleRate = false;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) override;

public:
	Rmedia_creator(Renderable* src, Renderable* format, Renderable* begin, Renderable* end, Renderable* fps, Renderable* width, Renderable* height, Renderable* videoBitrate, Renderable* audioMinSampleRate);
	Rmedia_creator(Renderable* src, std::string format, double begin, double end, double fps = 0, uint32_t width = 0, uint32_t height = 0, size_t videoBitrate = 0, size_t audioMinSampleRate = -1);
	~Rmedia_creator();

	torasu::ElementMap getElements() override;
	void setElement(std::string key, Element* elem) override;

	// torasu::DataResource* getData() override;
	// void setData(torasu::DataResource* data) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RMEDIA_CREATOR_HPP_
