#include "../include/torasu/mod/imgc/Rgraphics.hpp"

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dbimg.hpp>

#include <stack>
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

		std::fill(data, data+(width*height*4), 0x00);

		ProjectionSettings set = {width, height};
		
		CurvedSegment seg = {
			{.2,.2},
			{1,2},
			{-1,-.5},
			{.8,.8}
		};
		OptimisedCurvedSegment oseg(seg, set);
		std::vector<InterpStackElem> result;
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
		
		size_t vertCount = 0;
		for (auto& vert : result) {
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
		}
		std::cout << "Drawn " << vertCount << " verts" << std::endl;

		/*for (uint32_t y = 0; y < width; y++) {
			
			
			for (uint32_t x = 0; x < height; x++) {
				
				*data = (0xF0 & y) | (0x0F & x);
				data++;
				*data = (0xF0 & x) | (0x0F & y);
				data++;
				*data = x*y;
				data++;
				*data = 0xFF;
				data++;
			}

		}*/
		
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_OK, base, true);
	} else {
		return new torasu::ResultSegment(torasu::ResultSegmentStatus_INVALID_SEGMENT);
	}

}

} // namespace imgc
