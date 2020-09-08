#ifndef INCLUDE_TORASU_MOD_IMGC_DCROPDATA_HPP_
#define INCLUDE_TORASU_MOD_IMGC_DCROPDATA_HPP_

#include <torasu/torasu.hpp>
#include <torasu/DataPackable.hpp>

namespace imgc {

class Dcropdata : public torasu::DataPackable {

private:
	const std::string ident = std::string("IMGC::DCROPDATA");

		double offLeft, offRight, offTop, offBottom;

public:
	explicit Dcropdata(std::string jsonStripped);
	explicit Dcropdata(nlohmann::json jsonParsed);
	explicit Dcropdata(double offLeft, double offRight, double offTop, double offBottom);

	double getOffLeft();
	double getOffRight();
	double getOffTop();
	double getOffBottom();

	std::string getIdent();
	void load();
	nlohmann::json makeJson();

};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_DCROPDATA_HPP_
