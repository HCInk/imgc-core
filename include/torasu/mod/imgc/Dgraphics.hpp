#ifndef INCLUDE_TORASU_MOD_IMGC_DGRAPHICS_HPP_
#define INCLUDE_TORASU_MOD_IMGC_DGRAPHICS_HPP_

#include <torasu/torasu.hpp>
#include <torasu/DataPackable.hpp>

namespace imgc {

class Dgraphics_FORMAT : public torasu::ResultFormatSettings {
public:
	Dgraphics_FORMAT();

	torasu::DataDump* dumpResource() override;
	Dgraphics_FORMAT* clone() const override;
};

class Dgraphics : public torasu::DataPackable {
public:

	/**
	 * @brief  Graphics-Coordinate
	 * (0,0): top-left
	 * (1,1): bottom-right
	 */
	struct GCoordinate {
		double x, y;
	};

	/**
	 * @brief  A segment which defines a path between the two points a and b
	 */
	struct GSegment {
		GCoordinate a, ca, cb, b;
	};

	/**
	 * @brief  A section which is made out of segments
	 * which enclose the area the section defines
	 */
	class GSection {
	public:
		const std::vector<GSegment> segments;

	public:
		explicit GSection(std::vector<GSegment> segments);
	};

	/**
	 * @brief  A shape which is defined by a one or more sections
	 */
	class GShape {
	public:
		const std::vector<GSection> sections;
		const std::vector<GCoordinate> bounds;

	public:
		GShape(std::vector<GSection> sections, std::vector<GCoordinate> bounds);
		GShape(GSection section, std::vector<GCoordinate> bounds);
	};

	/**
	 * @brief  Object to be filled
	 */
	class GObject {
	public:
		GShape shape;
	public:
		explicit GObject(GShape shape);
	};

private:
	std::vector<GObject> objects;

protected:
	void load() override;
	torasu::json makeJson() override;

public:
	Dgraphics(std::string jsonStripped);
	Dgraphics(torasu::json jsonParsed);

	Dgraphics(std::vector<GObject> objects);
	~Dgraphics();

	std::string getIdent() const override;
	Dgraphics* clone() const override;

	std::vector<GObject>& getObjects();
};

} // namespace imgc

#endif // INCLUDE_TORASU_MOD_IMGC_DGRAPHICS_HPP_
