#ifndef INCLUDE_TORASU_MOD_IMGC_RIMGFILE_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RIMGFILE_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

#include <torasu/std/spoilsDR.hpp>

namespace imgc {

class RImgFile : public torasu::tools::SimpleRenderable {
private:
	torasu::tstd::DPString* data;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri);

public:
	RImgFile();
	~RImgFile();

	torasu::DataResource* getData();
	void setData(torasu::DataResource* data);
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RIMGFILE_HPP_
