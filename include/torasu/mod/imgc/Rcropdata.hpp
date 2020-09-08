#ifndef INCLUDE_TORASU_MOD_IMGC_RCROPDATA_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RCROPDATA_HPP_

#include <map>
#include <string>

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

#include <torasu/mod/imgc/pipeline_names.hpp>
#include <torasu/mod/imgc/Dcropdata.hpp>

namespace imgc {

class Rcropdata : public torasu::tools::SimpleRenderable {
private:
	std::string pipeline = std::string(IMGC_PL_ALIGN);

	Dcropdata* val;

protected:
	virtual torasu::ResultSegment* renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri);

public:
	explicit Rcropdata(Dcropdata* val);
	virtual ~Rcropdata();

	virtual torasu::DataResource* getData();
	virtual void setData(torasu::DataResource* data);
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RCROPDATA_HPP_
