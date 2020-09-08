#include "../include/torasu/mod/imgc/Dcropdata.hpp"

using json = nlohmann::json;

namespace imgc {

Dcropdata::Dcropdata(std::string jsonStripped) 
	: DataPackable(jsonStripped) {}

Dcropdata::Dcropdata(nlohmann::json jsonParsed) 
	: DataPackable(jsonParsed) {}

Dcropdata::Dcropdata(double offLeft, double offRight, double offTop, double offBottom) 
	: offLeft(offLeft), offRight(offRight), offTop(offTop), offBottom(offBottom) {}


double Dcropdata::getOffLeft() {
	return offLeft;
}

double Dcropdata::getOffRight() {
	return offRight;
}

double Dcropdata::getOffTop() {
	return offTop;
}

double Dcropdata::getOffBottom() {
	return offBottom;
}

std::string Dcropdata::getIdent() {
	return ident;
}

void Dcropdata::load() {
	auto json = this->getJson();
	offLeft = json["l"];
	offRight = json["r"];
	offTop = json["t"];
	offBottom = json["b"];
}


json Dcropdata::makeJson() {
	json madeJson;
	madeJson["l"] = offLeft;
	madeJson["r"] = offRight;
	madeJson["t"] = offTop;
	madeJson["b"] = offBottom;
	return madeJson;
}

} // namespace imgc
