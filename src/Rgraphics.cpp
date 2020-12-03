#include "../include/torasu/mod/imgc/Rgraphics.hpp"

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dbimg.hpp>

#include <cmath>
#include <stack>
#include <list>
#include <iostream>

namespace {

struct ProjectionSettings {
	uint32_t width, height;
};

struct Coordinate {
	double x, y;
};

struct CurvedSegment {
	Coordinate a, ca, cb, b;
};

// #define ABS_CORD_FAC 1000
// typedef int64_t abs_cord_t;

#define ABS_CORD_FAC 1
typedef double abs_cord_t;

class OptimisedCurvedSegment {
public:
	const abs_cord_t 
		ax, ay, 
		cax, cay, 
		cbx, cby, 
		bx, by;

	OptimisedCurvedSegment(const CurvedSegment& seg, const ProjectionSettings& set) 
		: ax(seg.a.x * set.width * ABS_CORD_FAC), ay(seg.a.y * set.height * ABS_CORD_FAC),
		cax(seg.ca.x * set.width * 3 * ABS_CORD_FAC), cay(seg.ca.y * set.height * 3 * ABS_CORD_FAC),
		cbx(seg.cb.x * set.width * 3 * ABS_CORD_FAC), cby(seg.cb.y * set.height * 3 * ABS_CORD_FAC),
		bx(seg.b.x * set.width * ABS_CORD_FAC), by(seg.b.y * set.height * ABS_CORD_FAC) {}
};

struct AbsoluteCoordinate {
	abs_cord_t x, y;
};

struct InterpRational {
	uint32_t num, den;
};

struct InterpStackElem {
	InterpRational interp;
	AbsoluteCoordinate cord;
};

// Gurantee: First element is 
struct RasterizedFraction {
	AbsoluteCoordinate a, b; // a.x <= b.x // lineY <= a.y <= lineY+1 // 0 <= b.y <= 1
};

class ScanlineParticleStorage {
public:
	std::map<size_t, std::multimap<int64_t, RasterizedFraction>> stor;
	std::vector<std::vector<RasterizedFraction>> fracs;
	ScanlineParticleStorage(size_t height) 
		: fracs(height) {}

	/**
	 * @brief  Fetches an ordered line for rasterisation
	 * @note   Not thread-safe
	 * @param  lineNo: Number of line to be fetched (0 <= lineNo < height)
	 * 
	 * @retval The ordered vector of the contained lines in order of thier RasterizedFraction.a.x
	 */
	inline std::vector<RasterizedFraction>& getOrderedLine(size_t lineNo) {
		auto& line = fracs[lineNo];

		if (line.size() > 2) {
			
			struct {
				bool operator()(RasterizedFraction a, RasterizedFraction b) const {
					return a.a.x < b.a.x; // Returns true if less
				}
			} fractionComp;

			std::sort(line.begin(), line.end(), fractionComp);
		}

		return line;
	}
};

uint8_t* fillOffset(uint8_t* data, size_t offset, uint32_t color) {
	/* auto* dataEnd = data+(offset*4);
	std::fill(data, dataEnd, 0xFF);
	return dataEnd; */

	/* std::fill_n(data, offset*4, 0xFFFFFF);
	return data+(offset*4); */

	uint32_t* optData = reinterpret_cast<uint32_t*>(data);
	// for (size_t i = offset-1; i >= 0; i--) {
	for (size_t i = 0; i < offset; i++) {
		*optData = color;
		// *optData = 0xFFFFFFFF;
		optData++;
		// *data = 0xFF;
		// data++;
		// *data = 0xFF;
		// data++;
		// *data = 0xFF;
		// data++;
		// *data = 0xFF;
		// data++;
	}

	return reinterpret_cast<uint8_t*>(optData);
}

inline void eval(const OptimisedCurvedSegment& seg, const ProjectionSettings& settings, InterpStackElem* elem) {
	// elem->cord.x = (size_t) elem->interp.num *ABS_CORD_FAC* settings.width / elem->interp.den ;
	// elem->cord.y = (size_t) elem->interp.num *ABS_CORD_FAC* settings.height / elem->interp.den ;

	//b(u)=
	// (1-u)^3 A
	// + 3 (1-u)^2 u CA
	// + 3 (1-u) u^2 CB
	// + u^3 B

	auto un = elem->interp.num;
	auto den = elem->interp.den;
	auto uni = den-un;

	// "Fast"

	// elem->cord.x =(
	// 	  seg.ax * uni * uni/den * uni
	// 	+ seg.cax * uni * uni/den * un
	// 	+ seg.cbx * uni * un/den * un
	// 	+ seg.bx * un * un/den * un
	// )/(den*den);

	// elem->cord.y =(
	// 	  seg.ay * uni * uni/den * uni
	// 	+ seg.cay * uni * uni/den * un
	// 	+ seg.cby * uni * un/den * un
	// 	+ seg.by * un * un/den * un
	// )/(den*den);

	// wtf why is this even faster?

	double und = (double) un/den;
	double unid = (double) uni/den;

	elem->cord.x =
		  (double) seg.ax * unid * unid * unid
		+ (double) seg.cax * unid * unid * und
		+ (double) seg.cbx * unid * und * und
		+ (double) seg.bx * und * und * und;

	elem->cord.y =
		  (double) seg.ay * unid * unid * unid
		+ (double) seg.cay * unid * unid * und
		+ (double) seg.cby * unid * und * und
		+ (double) seg.by * und * und * und;

}

inline std::string setostr(const InterpStackElem& elem) {
	return "[" + std::to_string(elem.cord.x) + "#" + std::to_string(elem.cord.y) + "  " + std::to_string(elem.interp.num) + "/" + std::to_string(elem.interp.den) + "]";
}

} // namespace

namespace imgc {

Rgraphics::Rgraphics() 
	: SimpleRenderable("IMGC::RGRAPHICS", false, false) {

}

Rgraphics::~Rgraphics() {}

torasu::ResultSegment* Rgraphics::renderSegment(torasu::ResultSegmentSettings* resSettings, torasu::RenderInstruction* ri) {

	if (resSettings->getPipeline() == TORASU_STD_PL_VIS) {
		torasu::tstd::Dbimg* base;
		if (auto* fmt = dynamic_cast<torasu::tstd::Dbimg_FORMAT*>(resSettings->getResultFormatSettings())) {
			base = new torasu::tstd::Dbimg(*fmt);
		}

		uint32_t width = base->getWidth();
		uint32_t height = base->getHeight();
		uint8_t* data = base->getImageData();

		std::chrono::steady_clock::time_point bench;

		// bench = std::chrono::steady_clock::now();
		// std::fill_n(reinterpret_cast<uint32_t*>(data), width*height, 0);
		// std::cout << "Init-Fill = " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count() << "[ms]" << std::endl;


		ProjectionSettings set = {width, height};
		
		// for (int i = 0; i < 1000; i++) {
		//  	data = base->getImageData();

		bench = std::chrono::steady_clock::now();

		double radius = 0.2;
		auto unit = 8*radius*4*(sqrt(2)-1)/3;

		std::vector<CurvedSegment> segments = {
			{
				{.5-radius,.5},
				{.5-radius,.5-unit},
				{.5-unit,.5-radius},
				{.5,.5-radius},
			},
			{
				{.5,.5-radius},
				{.5+unit,.5-radius},
				{.5+radius,.5-unit},
				{.5+radius,.5},
			},
			{
				{.5+radius,.5},
				{.5+radius,.5+unit},
				{.5+unit,.5+radius},
				{.5,.5+radius},
			},
			{
				{.5,.5+radius},
				{.5-unit,.5+radius},
				{.5-radius,.5+unit},
				{.5-radius,.5},
			},
		};

		std::vector<InterpStackElem> result;

		for (auto& seg : segments) {
			OptimisedCurvedSegment oseg(seg, set);
			{
				std::stack<InterpStackElem> interpolationStack;

				InterpStackElem cBegin = {{0,1}};
				eval(oseg, set, &cBegin);
				result.push_back(cBegin);

				InterpStackElem cEnd = {{1,1}};
				eval(oseg, set, &cEnd);
				interpolationStack.push(cEnd);

				bool overinterpolate = false;
				do {
					cEnd = interpolationStack.top();
					auto& a = cBegin.cord;
					auto& b = cEnd.cord;

					bool inRange; 
					{
						auto x = a.x - b.x;
						auto y = a.y - b.y;
						// (x/ABS_CORD_FAC)^2 + (y/ABS_CORD_FAC)^2 < sqrt(1) -> x^2 + y^2 < ABS_CORD_FAC^2
						inRange = (x*x)+(y*y) < ABS_CORD_FAC*ABS_CORD_FAC; 
					}

					// std::cout << " EVAL \t" << setostr(cBegin) << "-" << setostr(cEnd) << " IR " << inRange << " OI " << overinterpolate << std::endl;

					// if (cBegin.interp.den == 1073741824) {
					// 	cBegin = interpolationStack.top();
					// 	result.push_back(cBegin);
					// 	interpolationStack.pop();
						
					// } else

					if (inRange && overinterpolate) {
						overinterpolate = false;
						// std::cout << " POP OI \t" << setostr(interpolationStack.top()) << std::endl;
						interpolationStack.pop(); // remove overinterpolation overhead
						cBegin = interpolationStack.top();
						// result.push_back(cBegin.cord);
						result.push_back(cBegin);
						// std::cout << " TAKE/POP TOP \t" << setostr(cBegin) << std::endl;
						interpolationStack.pop();
					} else {
						cBegin.interp.num *= 2;
						cBegin.interp.den *= 2;
						InterpStackElem subElem = {cBegin.interp.num + 1, cBegin.interp.den};
						eval(oseg, set, &subElem);
						interpolationStack.push(subElem);
						overinterpolate = inRange;
						// std::cout << " ADD STACK \t" << setostr(subElem) << " OI " << overinterpolate << std::endl;
						continue;
					}

				} while (!interpolationStack.empty());

			}
		}

		std::cout << "Subdivision = " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count() << "[ms]" << std::endl;
		bench = std::chrono::steady_clock::now();

		ScanlineParticleStorage lineMapping(height);
		
		AbsoluteCoordinate prevCord;
		bool hasPrev = false;
		for (auto& vert : result) {
			auto cord = vert.cord;
			if (hasPrev) {
				
				int64_t yLow, yHigh;


				bool swap = cord.x < prevCord.x;

				// if (cord.y < 0 && prevCord.y < 0) continue;

				// std::cout << "SUBDIV " 
				// 	<< prevCord.x << "#" << prevCord.y << " // "
				// 	<< cord.x << "#" << cord.y << std::endl;


				bool risingY = cord.y > prevCord.y;
				if (risingY) {
					yLow = floor(prevCord.y);
					yHigh = floor(cord.y);
				} else {
					yLow = floor(cord.y);
					yHigh = floor(prevCord.y);
				}

				if (yLow >= height) continue;
				if (yHigh < 0) continue;
				if (yLow < 0) yLow = 0;
				if (yHigh >= height) yHigh = height-1;

				if (yLow == yHigh) {

					RasterizedFraction frac;
					if (swap) {
						frac = {cord, prevCord};
					} else {
						frac = {prevCord, cord};
					}
					// lineMapping.stor[yLow].insert(
					// 	std::pair<int64_t, RasterizedFraction>(frac.a.x, frac));

					lineMapping.fracs[yLow].push_back(frac);

					// std::cout << " SINGLE -> " 
					// 	<< frac.a.x << "#" << frac.a.y << " // "
					// 	<< frac.b.x << "#" << frac.b.y << std::endl;

				} else {

					double liftFact = (cord.x - prevCord.x)/(cord.y - prevCord.y);

					auto* fracLine = &lineMapping.fracs[yLow];

					for (int64_t y = yLow; y <= yHigh; y++) {
						
						double ya, yb;
						if (risingY) {
							ya = std::max((double)y, prevCord.y);
							yb = std::min((double)y+1, cord.y);
						} else {
							ya = std::min((double)y+1, prevCord.y);
							yb = std::max((double)y, cord.y);
						}

						double xa, xb;
						xa = (ya - prevCord.y)*liftFact + prevCord.x;
						xb = (yb - prevCord.y)*liftFact + prevCord.x;

						RasterizedFraction frac;
						if (swap) {
							frac = {{xb, yb}, {xa, ya}};
						} else {
							frac = {{xa, ya}, {xb, yb}};
						}
						// lineMapping.stor[y].insert(
						// 	std::pair<int64_t, RasterizedFraction>(frac.a.x, frac));
						
						fracLine->push_back(frac);

						// std::cout << " MULTI -> " 
						// 	<< frac.a.x << "#" << frac.a.y << " // "
						// 	<< frac.b.x << "#" << frac.b.y << std::endl;
						
						fracLine++;
					}
				}


			} else {
				hasPrev = true;
			}

			prevCord = vert.cord;
		}


		std::cout << "Remapping = " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count() << "[ms]" << std::endl;
		bench = std::chrono::steady_clock::now();

		size_t vertCount = 0;
		/*for (auto& vert : result) {
			Coordinate cord = {
				(double) vert.cord.x / ABS_CORD_FAC,
				(double) vert.cord.y / ABS_CORD_FAC
			};

			if (cord.x < 0 || cord.x >= width || cord.y < 0 || cord.y >= height) continue;
			
			auto* locPtr = data + ( ((((size_t)cord.y)*width)+((size_t)cord.x))*4);
			*locPtr = ((size_t) vert.interp.num*0xFF)/vert.interp.den;
			locPtr++;
			*locPtr = 0xFF*cord.x/width;
			locPtr++;
			*locPtr = 0xFF*cord.y/height;
			locPtr++;
			*locPtr = 0xFF;
			vertCount++;
		}*/

		std::list<RasterizedFraction> currFractions;

		// auto* fixedData = base->getImageData();
		
		for (uint32_t y = 0; y < height; y++) {
			
			// std::cout << "LINE " << y << std::endl;

			// auto& mappingEntry = lineMapping.stor[y];

			auto& fracs = lineMapping.getOrderedLine(y);
			auto readPtr = fracs.begin();
			uint32_t nextRelX = 0;
			int32_t containmentBalance = 0;
			double nextEnd = INFINITY; // Ressemles the lowest end-x-coordinate in the stack
			for (uint32_t x = 0; x < width; x++) {
				if (x >= nextRelX) {

					abs_cord_t lastX = x;
					bool fromList = true; // true: from list, false: from source
					auto frit = currFractions.begin();
					for (;;) {
						RasterizedFraction rf;
						if (fromList) {

							if (frit == currFractions.end()) { // If list has been fully read switch to source
								fromList = false;
								continue;
							}

							if (frit->b.x <= x) {
								throw std::logic_error("too old fraction still in list!");
							}

							rf = *frit;

							frit++;
						} else {

							if (readPtr == fracs.end() || floor(readPtr->a.x) > x ) {
								break;
							}

							rf = *readPtr;

							// std::cout << "[" << x << "#" << y << "] READ FRAC " 
							// 	<< readPtr->second.a.x << "#" << readPtr->second.a.y << " // "
							// 	<< readPtr->second.b.x << "#" << readPtr->second.b.y << std::endl;

							// {
							// 	auto* locPtr = fixedData + ( ((((size_t)y)*width)+((size_t)x))*4+0);
							// 	*locPtr = 0xFF;
							// }
							// {
							// 	auto* locPtr = fixedData + ( ((((size_t) /* readPtr->second.a.y */ y)*width)+((size_t)readPtr->second.a.x))*4+1);
							// 	*locPtr = 0xFF;
							// }
							// {
							// 	auto* locPtr = fixedData + ( ((((size_t) /* readPtr->second.b.y */ y)*width)+((size_t)readPtr->second.b.x))*4+2);
							// 	*locPtr = 0xFF;
							// }
							if (0 == readPtr->a.y - y) {
								containmentBalance++;
								// std::cout << "CNG BIL++ " << containmentBalance << std::endl;
							}
							if (0 == readPtr->b.y - y) {
								containmentBalance--;
								// std::cout << "CNG BIL-- " << containmentBalance << std::endl;
							}
							readPtr++;
							vertCount++;
						}

						if (rf.b.x > x+1) { // Check if inside on next round
							if (!fromList) {
								currFractions.push_back(rf);
							}
						} else {
							if (fromList) {
								frit--;
								frit = currFractions.erase(frit);
							}
						}

						*data = *data < 0xE0 ? *data+0x30 : 0xFF; // RED (0x30 for every call)
						
						// TODO Evalulate

					}
					
					data++;
					
					if (currFractions.empty()) {
						nextRelX = readPtr != fracs.end()
							? floor(readPtr->a.x)
							: width;
						*data = 0xFF; // GREEN
					} else {
						*data = 0x00; // NOT GREEN
					}

					data++;
					*data = 0xFF; // BLUE
				} else {
					size_t offset;
					if (nextRelX >= width) {
						offset = width-x;
					} else {
						offset = nextRelX-x;
					}
					
					x += offset-1; // -1 since it will be inrecmented on loop
					if (0 == containmentBalance%2) {
						data = fillOffset(data, offset, 0x00000000);
					} else {
						data = fillOffset(data, offset, 0xFF222222);
					}
					// data += offset * 4;
					continue;
					// *data = 0x00;
					// data++;
					// *data = 0x00;
					// data++;
					// *data = 0xFF;
				}

				data++;
				*data = 0xFF;
				data++;
			}

			currFractions.clear();

		}

		std::cout << "Raster = " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count() << "[ms]" << std::endl;


		std::cout << "Drawn " << vertCount << " verts" << std::endl;

		// }
		
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, base, true);
	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}

}

} // namespace imgc
