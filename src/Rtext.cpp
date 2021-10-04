#include "../include/torasu/mod/imgc/Rtext.hpp"

#include <memory>
#include <queue>
#include <wchar.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <torasu/render_tools.hpp>

#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/property_names.hpp>
#include <torasu/std/Dnum.hpp>

#include <torasu/mod/imgc/Dgraphics.hpp>

#define DEBUG_HIDE_MAIN false
#define DEBUG_EXTRA_DATA false
#define DEBUG_SHOW_POINTS false

#if DEBUG_SHOW_POINTS
	#define DEBUG_EXTRA_DATA true
#endif

namespace {

// XXX Temporary padding for text, while rendering outside of ordinary cordinates is not possible
constexpr double padding = 0.35;

/**
 * @brief  Retrieves next Unicode-codepoint from utf-8
 * @param  utf8: Input string (utf-8/null-terminated) at position to read
 * 					- will be updated to next character to read
 * @retval The Unicode-codepoint that has been read (the first of the sequence),
 * 			0x00 if there is no more to read
 */
inline uint32_t nextCharUtf8(const char** utf8) {
	size_t i = 0;
	uint32_t buff = static_cast<uint8_t>(**utf8);
	if (buff == 0x00) return 0x00;
	for (;;) {
		(*utf8)++;
		i++;
		// characters that begin with binary 10xxxxxx... are continuations; all other
		// characters should begin a new utf32 char (assuming valid utf8 input)
		char nextVal = **utf8;
		if (i >= 4 || *utf8 == 0x00 || (nextVal & 0xc0) != 0x80) break;
		buff <<= 6;
		buff |= nextVal & 0x3F;
	}
	// Apply mask to each result
	switch (i) {
	case 2:
		buff &= 0x000007FF;
		break;
	case 3:
		buff &= 0x0000FFFF;
		break;
	case 4:
		buff &= 0x001FFFFF;
		break;
	default:
		break;
	}
	return buff;
}

struct PointInfo {
	imgc::Dgraphics::GCoordinate cord;
	bool isControl;
	bool isCubic;
#if DEBUG_EXTRA_DATA
	bool isInterpolation = false;
#endif

};

struct Character {
	std::vector<std::vector<PointInfo>> points;
	double width;
	double xPosition;

	inline size_t getMemorySize() const {
		return sizeof(Character) + points.size()*sizeof(PointInfo);
	}
};

class TextState : public torasu::ReadyState {
private:
	std::vector<torasu::Identifier>* ops;
	const torasu::RenderContextMask* rctxm;
protected:
	std::vector<Character> characters;
	double totalWidth;
	size_t stateSize = 0;

	TextState(std::vector<torasu::Identifier>* ops, const torasu::RenderContextMask* rctxm)
		: ops(ops), rctxm(rctxm) {}

public:
	~TextState() {
		delete ops;
		if (rctxm != nullptr) delete rctxm;
	}

	virtual const std::vector<torasu::Identifier>* getOperations() const override {
		return ops;
	}

	virtual const torasu::RenderContextMask* getContextMask() const override {
		return rctxm;
	}

	virtual size_t size() const override {
		return sizeof(TextState) + stateSize;
	}

	virtual ReadyState* clone() const override {
		throw new std::runtime_error("cloning currently unsupported");
	}

	friend imgc::Rtext;
};

} // namespace

namespace imgc {

Rtext::Rtext(torasu::tstd::StringSlot text)
	: SimpleDataElement(false, true), textRnd(text) {}

Rtext::~Rtext() {}

torasu::Identifier Rtext::getType() {
	return "IMGC::RTEXT";
}

void Rtext::ready(torasu::ReadyInstruction* ri) {
	if (textRnd.get() == nullptr) throw std::logic_error("Text renderable set yet!");

	torasu::tools::RenderHelper rh(ri);

	// Get Text
	std::string str;
	torasu::ResultSettings stringRs(TORASU_STD_PL_STRING, torasu::tools::NO_FORMAT);
	std::unique_ptr<torasu::RenderResult> rndRes(rh.runRender(textRnd, &stringRs));

	auto fetchedRes = rh.evalResult<torasu::tstd::Dstring>(rndRes.get());

	if (fetchedRes) {
		str = fetchedRes.getResult()->getString();
	} else {
		if (rh.mayLog(torasu::WARN)) {
			rh.lrib.logCause(torasu::WARN, "Failed fetching target text!", fetchedRes.takeInfoTag());
		}
		rh.lrib.hasError = true;
	}

	// Register State
	auto* textState = new TextState(
	new std::vector<torasu::Identifier>({
		TORASU_STD_PL_VIS, TORASU_PROPERTY(TORASU_STD_PROP_IMG_RAITO)}),
	rh.takeResMask());
	ri->setState(textState);

	// Create graphics
	bool debugLog = rh.mayLog(torasu::DEBUG);

	FT_Error error;
	FT_Library  library;   /* handle to library     */
	FT_Face     face;      /* handle to face object */


	error = FT_Init_FreeType( &library );
	if ( error ) {
		throw std::runtime_error("Error initializing freetype: " + std::to_string(error));
	}

	error = FT_New_Face( library,
						 //  "/usr/share/fonts/adobe-source-han-sans/SourceHanSansJP-Bold.otf",
						 "/usr/share/fonts/noto/NotoSans-BlackItalic.ttf",
						 0,
						 &face );
	if ( error == FT_Err_Cannot_Open_Resource ) {
		throw std::runtime_error("Error opening font frace: Cannot open resource");
	} else if ( error == FT_Err_Unknown_File_Format ) {
		throw std::runtime_error("Error opening font frace: Unknown format");
	} else if ( error ) {
		throw std::runtime_error("Error opening font frace: " + std::to_string(error));
	}

	// FT_Size_RequestRec sizeRequest = {
	// 	FT_Size_Request_Type::FT_SIZE_REQUEST_TYPE_MAX,
	// 	1000,
	// 	1000,
	// 	1000,
	// 	1000
	// };
	// error = FT_Request_Size(
	//   face,
	//   &sizeRequest );
	// if ( error ) {
	// 	throw std::runtime_error("Error requesting size: " + std::to_string(error));
	// }


	// error = FT_Set_Pixel_Sizes(
	//   face,   /* handle to face object */
	//   0,      /* pixel_width           */
	//   1000 );   /* pixel_height          */

	error = FT_Set_Char_Size(
				face,    /* handle to face object           */
				0,       /* char_width in 1/64th of points  */
				1000*64,   /* char_height in 1/64th of points */
				1000,     /* horizontal device resolution    */
				1000 );   /* vertical device resolution      */
	constexpr int sacleFactor = 64*1000*10;

	if ( error ) {
		throw std::runtime_error("Error requesting size: " + std::to_string(error));
	}

	double cursorX = 0;

	// std::vector<std::vector<PointInfo>> toDraw;

	auto& characters = textState->characters;

	const char* cstr = str.c_str();

	for (size_t gi = 0;; gi++) {
		auto nextChar = nextCharUtf8(&cstr);
		if (nextChar == 0x00) break;

		FT_UInt  glyph_index;
		/* retrieve glyph index from character code */
		glyph_index = FT_Get_Char_Index( face, nextChar/* cstr[i] */ );

		/* load glyph image into the slot (erase previous one) */
		error = FT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT );
		if ( error ) {
			throw std::runtime_error("Error loading glyph " + std::to_string(gi) + ": " + std::to_string(error));
		}

		Character& character = characters.emplace_back();
		auto glyph = face->glyph;

		if (debugLog)
			rh.li.logger->log(torasu::DEBUG, "Loaded glyph " + std::to_string(gi));

		character.xPosition = cursorX;
		character.width = glyph->advance.x;
		auto outline = glyph->outline;

		std::vector<std::vector<PointInfo>>& characterPoints = character.points;

		size_t currentOffset = 0;
		// size_t currentOffset = outline.contours[0]+1;
		size_t contourCount = outline.n_contours;
		// size_t contourCount = 1;
		for (size_t ci = 0; ci < contourCount; ci++) {
			std::vector<PointInfo>& drawGroup = characterPoints.emplace_back();

			size_t nextOffset = outline.contours[ci]+1;
			// int nextOffset = outline.n_points;

			int consecutiveControls = 0;
			PointInfo lastPoint;
			PointInfo nextPoint;
			for (size_t pi = currentOffset; pi < nextOffset; pi++) {
				{
					// Reading next point
					lastPoint = nextPoint;
					auto ftPoint = outline.points[pi];
					nextPoint.cord = {static_cast<double>(ftPoint.x)/sacleFactor, static_cast<double>(ftPoint.y)/sacleFactor*-1+1};
					auto tag = outline.tags[pi];
					// If bit~0 is unset, the point is 'off' the curve, i.e., a Bezier
					// control point, while it is 'on' if set.
					nextPoint.isControl = !(tag & 0x1);
					// Bit~1 is meaningful for 'off' points only.  If set, it indicates a
					// third-order Bezier arc control point; and a second-order control
					// point if unset.
					nextPoint.isCubic = tag & 0x2;

					if (debugLog) {
						std::string label = nextPoint.isControl ? (nextPoint.isCubic ? "CTL-3" : "CTL-2") : "DIRECT";
						rh.li.logger->log(torasu::DEBUG, "next-pt \t" + std::to_string(pi) + "\t" + std::to_string(ftPoint.x) + "\t" + std::to_string(ftPoint.y) + "\t" + label);
					}
				}


				if (nextPoint.isControl) {
					if (pi == 0) {
						nextPoint.isControl = false;
					} else {
						consecutiveControls++;
						// Interpolate if neccesary
						if (nextPoint.isCubic ? consecutiveControls > 2 : consecutiveControls > 1) {
							PointInfo interpolation;
							interpolation.cord = {
								(lastPoint.cord.x + nextPoint.cord.x)/2,
								(lastPoint.cord.y + nextPoint.cord.y)/2
							};
							interpolation.isControl = false;
#if DEBUG_EXTRA_DATA
							interpolation.isInterpolation = true;
#endif
							drawGroup.push_back(interpolation);
							if (debugLog) {
								rh.li.logger->log(torasu::DEBUG, "outline-pt \t" + std::to_string(interpolation.cord.x) + "\t" + std::to_string(interpolation.cord.y) + "\tINTER");
							}
							consecutiveControls = 1;
						}
					}
				} else {
					consecutiveControls = 0;
				}

				if (debugLog) {
					std::string label = nextPoint.isControl ? (nextPoint.isCubic ? "CTL-3" : "CTL-2") : "DIRECT";
					rh.li.logger->log(torasu::DEBUG, "outline-pt \t" + std::to_string(nextPoint.cord.x) + "\t" + std::to_string(nextPoint.cord.y) + "\t" + label);
				}

				// nextPoint.isControl = false;
				drawGroup.push_back(nextPoint);
			}

			if (!drawGroup.empty()) {
				drawGroup.push_back(drawGroup.front());
			}

			currentOffset = nextOffset;
		}

		cursorX += static_cast<double>(character.width)/sacleFactor;
	}

	FT_Done_FreeType(library);

	textState->totalWidth = cursorX + (padding*2);

}


torasu::RenderResult* Rtext::render(torasu::RenderInstruction* ri) {
	torasu::tools::RenderHelper rh(ri);
	if (rh.matchPipeline(TORASU_STD_PL_VIS)) {

		// Check format

		if (!rh.getFormat<Dgraphics_FORMAT>()) {
			return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_FORMAT);
		}

		// Draw characters from state
		auto* textState = static_cast<TextState*>(ri->getReadyState());

		double xScale = (1.0)/textState->totalWidth;

		std::vector<imgc::Dgraphics::GSection> sections;
		for (Character glyph : textState->characters) {
			const auto& characterPoints = glyph.points;
			double xOffset = glyph.xPosition + padding;

			for (const auto& drawGroup : characterPoints) {
				std::vector<imgc::Dgraphics::GSegment> segments;
				imgc::Dgraphics::GSegment currSegment;

				int procState = 0;
				size_t pointCount = drawGroup.size();
				const PointInfo* pointData = drawGroup.data();
				for (size_t pi = 0; pi < pointCount; pi++) {
					PointInfo point = pointData[pi];
					point.cord.x += xOffset;
					point.cord.x *= xScale;
					point.cord.y += padding;
					point.cord.y /= 1+padding*2;

					if (procState <= 0) { // set-up start segment
						currSegment.a = point.cord;
						procState = 1;
						continue;
					}

					if (point.isControl) {
#if DEBUG_SHOW_POINTS
						sections.push_back(Dgraphics::GSection::fromPolys({
							{point.cord.x, point.cord.y+0.01},
							{point.cord.x+0.01, point.cord.y},
							{point.cord.x-0.01, point.cord.y},
							{point.cord.x, point.cord.y-0.01},
						}));
#endif
						if (point.isCubic) {
							if (procState >= 2) {
								currSegment.cb = point.cord;
								procState = 3;
							} else {
								currSegment.ca = point.cord;
								procState = 2;
							}
						} else {
							// Get next cord: This works since the last point is never a control point, guarteed at read-time
							currSegment.b = pointData[pi+1].cord;
							currSegment.b.x += xOffset;
							currSegment.b.x *= xScale;
							currSegment.b.y += padding;
							currSegment.b.y /= 1+padding*2;

							// Convert quadratic to cubic
							currSegment.ca = {
								currSegment.a.x + 2.0/3.0*(point.cord.x - currSegment.a.x),
								currSegment.a.y + 2.0/3.0*(point.cord.y - currSegment.a.y)
							};
							currSegment.cb = {
								currSegment.b.x + 2.0/3.0*(point.cord.x - currSegment.b.x),
								currSegment.b.y + 2.0/3.0*(point.cord.y - currSegment.b.y)
							};

							procState = 2;
						}
					} else {
#if DEBUG_SHOW_POINTS
						if (point.isInterpolation) {
							sections.push_back(Dgraphics::GSection::fromPolys({
								{point.cord.x+0.005, point.cord.y+0.01},
								{point.cord.x-0.005, point.cord.y+0.01},
								{point.cord.x+0.005, point.cord.y-0.01},
								{point.cord.x-0.005, point.cord.y-0.01},
							}));
						} else {
							sections.push_back(Dgraphics::GSection::fromPolys({
								{point.cord.x+0.01, point.cord.y+0.01},
								{point.cord.x-0.01, point.cord.y+0.01},
								{point.cord.x+0.01, point.cord.y-0.01},
								{point.cord.x-0.01, point.cord.y-0.01},
							}));
						}
#endif
						currSegment.b = point.cord;
						if (procState == 1) {
							// Set control points if there has no control point been there yet to write it
							currSegment.ca = currSegment.a;
							currSegment.cb = currSegment.b;
						}
						procState = 1;
						segments.push_back(currSegment);
						currSegment.a = currSegment.b;
					}
				}
#if !DEBUG_HIDE_MAIN
				sections.push_back(imgc::Dgraphics::GSection(segments));
#endif
			}
		}

		auto* graphics = new Dgraphics({
			Dgraphics::GObject(
			Dgraphics::GShape(sections, {{0,0}, {0,1}, {1,1}, {1,0}})
			)
		});

		return rh.buildResult(graphics);
	} else if (rh.matchPipeline(TORASU_PROPERTY(TORASU_STD_PROP_IMG_RAITO))) {
		auto* textState = static_cast<TextState*>(ri->getReadyState());
		return rh.buildResult(new torasu::tstd::Dnum(textState->totalWidth/(1+padding*2)));
	} else {
		return new torasu::RenderResult(torasu::RenderResultStatus_INVALID_SEGMENT);
	}
}

torasu::ElementMap Rtext::getElements() {
	torasu::ElementMap elems;
	elems["text"] = textRnd.get();
	return elems;
}

void Rtext::setElement(std::string key, torasu::Element* elem) {
	if (torasu::tools::trySetRenderableSlot("text", &textRnd, false, key, elem)) return;
	throw torasu::tools::makeExceptSlotDoesntExist(key);
}


} // namespace imgc
