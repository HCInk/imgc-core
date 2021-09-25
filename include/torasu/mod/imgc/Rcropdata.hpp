#ifndef INCLUDE_TORASU_MOD_IMGC_RCROPDATA_HPP_
#define INCLUDE_TORASU_MOD_IMGC_RCROPDATA_HPP_

#include <map>
#include <string>

#include <torasu/torasu.hpp>
#include <torasu/SimpleRenderable.hpp>

#include <torasu/mod/imgc/Dcropdata.hpp>

namespace imgc {

class Rcropdata : public torasu::tools::SimpleRenderable {
private:
	Dcropdata* val;

protected:
	torasu::RenderResult* render(torasu::RenderInstruction* ri) override;

public:
	explicit Rcropdata(Dcropdata val);
	~Rcropdata();
	torasu::Identifier getType() override;

	torasu::DataResource* getData() override;
	void setData(torasu::DataResource* data) override;
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_RCROPDATA_HPP_
