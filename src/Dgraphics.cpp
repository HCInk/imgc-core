#include "../include/torasu/mod/imgc/Dgraphics.hpp"

namespace imgc {
	
Dgraphics::Dgraphics(std::string jsonStripped) : DataPackable(jsonStripped) {}
Dgraphics::Dgraphics(torasu::json jsonParsed) : DataPackable(jsonParsed) {}

Dgraphics::Dgraphics() {}
Dgraphics::~Dgraphics() {}

std::string Dgraphics::getIdent() {
	return "IMGC::DGRAPHICS";
}

Dgraphics* Dgraphics::clone() {
	return new Dgraphics(*this);
}

void Dgraphics::load() {
	torasu::json json = getJson();
	
	// TODO Dgraphics: load data
}

torasu::json Dgraphics::makeJson() {
	torasu::json json;

	// TODO Dgraphics: serialize data

	return json;
}

std::vector<Dgraphics::GObject> Dgraphics::getObjects() {
	ensureLoaded();
	return objects;
}

Dgraphics::GSection::GSection(std::vector<GSegment> segments) 
	: segments(segments) {}

Dgraphics::GShape::GShape(std::vector<GSection> sections, std::vector<GCoordinate> bounds) 
	: sections(sections), bounds(bounds) {}

Dgraphics::GShape::GShape(GSection section, std::vector<GCoordinate> bounds) 
	: sections({section}), bounds(bounds) {}

} // namespace imgc

