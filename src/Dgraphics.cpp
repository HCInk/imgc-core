#include "../include/torasu/mod/imgc/Dgraphics.hpp"

namespace imgc {
	
Dgraphics::Dgraphics(std::string jsonStripped) : DataPackable(jsonStripped) {}
Dgraphics::Dgraphics(torasu::json jsonParsed) : DataPackable(jsonParsed) {}

Dgraphics::Dgraphics(std::vector<Dgraphics::GObject> objects)
	: objects(objects) {}
Dgraphics::~Dgraphics() {}

Dgraphics_FORMAT::Dgraphics_FORMAT()
	: ResultFormatSettings("IMGC::DGRAPHICS") {}

torasu::DataDump* Dgraphics_FORMAT::dumpResource() {
	torasu::DDDataPointer dp;
	dp.b = nullptr;
	return new torasu::DataDump(dp, 0, nullptr);
}

Dgraphics_FORMAT* Dgraphics_FORMAT::clone() {
	return new Dgraphics_FORMAT();
}

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

std::vector<Dgraphics::GObject>& Dgraphics::getObjects() {
	ensureLoaded();
	return objects;
}

Dgraphics::GSection::GSection(std::vector<GSegment> segments) 
	: segments(segments) {}

Dgraphics::GShape::GShape(std::vector<GSection> sections, std::vector<GCoordinate> bounds) 
	: sections(sections), bounds(bounds) {}

Dgraphics::GShape::GShape(GSection section, std::vector<GCoordinate> bounds) 
	: sections({section}), bounds(bounds) {}

Dgraphics::GObject::GObject(GShape shape)
	: shape(shape) {}

} // namespace imgc

