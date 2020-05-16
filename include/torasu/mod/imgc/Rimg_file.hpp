#ifndef INCLUDE_TORASU_MOD_IMGC_RIMG_FILE_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RIMG_FILE_HPP_

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

#include <torasu/std/spoilsD.hpp>

namespace imgc {

class Rimg_file : public torasu::tools::SimpleRenderable {
private:
	torasu::tstd::Dstring* data;

protected:
	torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri);

public:
	Rimg_file();
	~Rimg_file();

	torasu::DataResource* getData();
	void setData(torasu::DataResource* data);
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RIMG_FILE_HPP_
