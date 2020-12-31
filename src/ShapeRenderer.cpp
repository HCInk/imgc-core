#include "../include/torasu/mod/imgc/ShapeRenderer.hpp"

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dbimg.hpp>

#include <cmath>
#include <stack>
#include <list>
#include <iostream>

// Sanity-checks. Recommended for debugging. Stop the code once something seems doesn't meet the requirements
#define IMGC_RGRAPHICS_DBG_SANITY_CHECKS false
// Image-Debug mode - 0: Normal/Disabled 1: R[VertCount]G[StripEnd]B[Result]
#define IMGC_RGRAPHICS_DBG_IMAGE_MODE 0	
// 0: Disabled, != 0: Run n-times sequentially
#define IMGC_RGRAPHICS_DBG_BENCH_LOOP 0 

namespace {

struct ProjectionSettings {
	uint32_t width, height;
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

	OptimisedCurvedSegment(const imgc::Dgraphics::GSegment& seg, const ProjectionSettings& set) 
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

torasu::tstd::Dbimg* ShapeRenderer::render(torasu::tstd::Dbimg_FORMAT fmt, Dgraphics::GShape shape) {

	torasu::tstd::Dbimg* base = new torasu::tstd::Dbimg(fmt);

	uint32_t width = base->getWidth();
	uint32_t height = base->getHeight();
	uint8_t* data = base->getImageData();


	//
	// Subdivision
	//

	std::chrono::steady_clock::time_point bench;

	// bench = std::chrono::steady_clock::now();
	// std::fill_n(reinterpret_cast<uint32_t*>(data), width*height, 0);
	// std::cout << "Init-Fill = " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count() << "[ms]" << std::endl;


	ProjectionSettings set = {width, height};

#if IMGC_RGRAPHICS_DBG_BENCH_LOOP != 0
	for (int benchIndex = 0; benchIndex < IMGC_RGRAPHICS_DBG_BENCH_LOOP; benchIndex++) {
		data = base->getImageData();
#endif


	ScanlineParticleStorage lineMapping(height);

	for (auto& section : shape.sections) {
		bench = std::chrono::steady_clock::now();

		std::vector<InterpStackElem> result;

		for (auto& seg : section.segments) {
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

#if IMGC_RGRAPHICS_DBG_SANITY_CHECKS
					if (cBegin.interp.den >= 1073741824) {
						// cBegin = interpolationStack.top();
						// result.push_back(cBegin);
						// interpolationStack.pop();
						throw std::logic_error("Reached stack-index-overflow when subdividing!");
					} else
#endif
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
		
		//
		// Remapping
		//

		bench = std::chrono::steady_clock::now();
		
		AbsoluteCoordinate prevCord;
		bool hasPrev = false;
		for (auto& vert : result) {
			auto cord = vert.cord;
			if (hasPrev) {
				
				int64_t yLow, yHigh;

				bool swap = cord.x < prevCord.x;

				// std::cout << "SUBDIV " 
				// 	<< prevCord.x << "#" << prevCord.y << " // "
				// 	<< cord.x << "#" << cord.y << std::endl;


				bool risingY = cord.y > prevCord.y;
				if (risingY) {
					yLow = floor(prevCord.y);
					yHigh = ceil(cord.y-1);
				} else {
					yLow = floor(cord.y);
					yHigh = ceil(prevCord.y-1);
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

#if IMGC_RGRAPHICS_DBG_SANITY_CHECKS					
						if (frac.a.x == frac.b.x && frac.a.y == frac.b.y) {
							throw std::runtime_error("Calculated non-line! @ " + std::to_string(frac.a.y) + "#" + std::to_string(frac.a.y));
						}
#endif
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
			
	}

	size_t vertCount = 0;

	//
	// Rasterisation
	//

	std::list<RasterizedFraction> currFractions;

	// auto* fixedData = base->getImageData();
	
	for (uint32_t y = 0; y < height; y++) {
		
		// std::cout << "LINE " << y << std::endl;

		// auto& mappingEntry = lineMapping.stor[y];

		auto& fracs = lineMapping.getOrderedLine(y);
		auto readPtr = fracs.begin();
		uint32_t nextRelX = 0;
		int32_t containmentBalance = 0;
		double nextEnd = INFINITY; // Ressembles the lowest end-x-coordinate in the currFractions
		for (uint32_t x = 0; x < width; x++) {
			if (x >= nextRelX) {

				double fillState = 0;

#if IMGC_RGRAPHICS_DBG_IMAGE_MODE != 0
				*data = 0x00;
#endif

				abs_cord_t lastX = x;
				for (;;) {
					bool hasNew = readPtr != fracs.end() && floor(readPtr->a.x) <= x;

#if IMGC_RGRAPHICS_DBG_SANITY_CHECKS
					if (hasNew) {
						if (readPtr->a.y < y) {
							throw std::logic_error("Praticle not in range! (" + std::to_string(y) + " !< " + std::to_string(y) + " < " + std::to_string(y+1));
						} else if (readPtr->a.y > y+1) {
							throw std::logic_error("Praticle not in range! (" + std::to_string(y) + " < " + std::to_string(y) + " !< " + std::to_string(y+1));
						}
					}
#endif
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

#if IMGC_RGRAPHICS_DBG_IMAGE_MODE == 1
					*data = *data < 0xE0 ? *data+0x30 : 0xFF; // RED (0x30 for every call)
#endif

					double newBegin; // x-coordinate of new element
					if (hasNew) {
						newBegin = readPtr->a.x;
					}

					bool end = false; // If true end the loop afterwards, also stack doesnt have to be cleaned since the range is capped before the next end
					bool insertNew = false;
					double evalTo;
					for (;;) {

						// Determine if rather existing elements shall be processed 
						// or (if available) a new element should be inserted
						if (hasNew) insertNew = nextEnd >= newBegin;

						if (insertNew) {
							// Evaluate until beginning of element to be inserted
							evalTo = newBegin; 
						} else {
							// Evalulate until the next end of the existing elements
							if (nextEnd > x+1) {
								end = true;
								evalTo = x+1;
							} else {
								evalTo = nextEnd;
							}
						}

						// Evalulate column 

						double segFill;
						if (currFractions.size() >= 1) {

							double midPos = (lastX+evalTo)/2;

							auto& frac = *currFractions.begin();
							double liftFac = (frac.b.y - frac.a.y) / (frac.b.x-frac.a.x);
							segFill = liftFac*(midPos - frac.a.x)+(frac.a.y-y);
							
#if IMGC_RGRAPHICS_DBG_SANITY_CHECKS
							// std::cout << " SEGFILL" << segFill << std::endl;						
							if (segFill <= 0 || segFill > 1) {
								throw std::logic_error("Invalid seg-fill " + std::to_string(segFill));
							}
#endif
						} else {
							segFill = 1;
						}

						if (0 == containmentBalance%2) segFill = 1-segFill; // Invert seg-fill if bottom is empty

						fillState += segFill*(evalTo-lastX);

						lastX = evalTo;

						if (insertNew) {
							// Insert new element after area to it has been analyzed

							if (0 == readPtr->a.y - y) {
								containmentBalance++;
								// std::cout << "CNG BIL++ " << containmentBalance << std::endl;
							}

							currFractions.push_back(*readPtr);
						} else if (end) {
							break;
						}

						nextEnd = INFINITY;
						for (auto cleanIt = currFractions.begin(); cleanIt != currFractions.end(); ) {
							if (cleanIt->b.x <= evalTo) {

								if (0 == cleanIt->b.y - y) {
									containmentBalance--;
									// std::cout << "CNG BIL-- " << containmentBalance << std::endl;
								}
								cleanIt = currFractions.erase(cleanIt);
								continue;
							}

							if (nextEnd > cleanIt->b.x) nextEnd = cleanIt->b.x;

							cleanIt++;
						}

						// Also break with inner loop after insert, so a new element to be inserted can be fetched
						if (insertNew) break;
					}

					if (!hasNew) break;
					
					readPtr++;
					vertCount++;
				}

#if IMGC_RGRAPHICS_DBG_IMAGE_MODE == 1
				data++;
#endif
				if (currFractions.empty()) {
					nextRelX = readPtr != fracs.end()
						? floor(readPtr->a.x)
						: width;
#if IMGC_RGRAPHICS_DBG_IMAGE_MODE == 1
					*data = 0xFF; // GREEN
#endif
				} 
#if IMGC_RGRAPHICS_DBG_IMAGE_MODE == 1
				else {
					*data = 0x00; // NOT GREEN
				}

				data++;
				*data = 0xFF*fillState; // BLUE
				data++;
				*data = 0xFF; // ALPHA
				data++;
#else
				*reinterpret_cast<uint32_t*>(data) = 0xFFFFFFFF;
				data += 3;
				*data *= fillState;
				data++;
#endif

			} else {
				size_t offset;
				if (nextRelX >= width) {
					offset = width-x;
				} else {
					offset = nextRelX-x;
				}
				
				x += offset-1; // -1 since it will be inrecmented on loop

#if IMGC_RGRAPHICS_DBG_IMAGE_MODE == 1
				if (0 == containmentBalance%2) {
					data = fillOffset(data, offset, 0xFF000000);
				} else {
					data = fillOffset(data, offset, 0xFFFF2222);
				}
#else
				if (0 == containmentBalance%2) {
					data = fillOffset(data, offset, 0x00FFFFFF);
				} else {
					data = fillOffset(data, offset, 0xFFFFFFFF);
				}
#endif
			}

		}

		currFractions.clear();

	}

	std::cout << "Raster = " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - bench).count() << "[ms]" << std::endl;


	std::cout << "Drawn " << vertCount << " verts" << std::endl;

#if IMGC_RGRAPHICS_DBG_BENCH_LOOP != 0
	}
#endif
		
	return base;

}

} // namespace imgc
