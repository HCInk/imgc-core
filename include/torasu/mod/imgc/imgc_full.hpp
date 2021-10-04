#ifndef INCLUDE_TORASU_MOD_IMGC_IMGC_FULL_HPP_
#define INCLUDE_TORASU_MOD_IMGC_IMGC_FULL_HPP_

/*

		 ___ __  __  ____  ____
		|_ _|  \/  |/ ___|/ ___|
		| || |\/| | |  _| |
		| || |  | | |_| | |___
		|___|_|  |_|\____|\____|

			IMGC FULL-HEADER

	-> This includes all public headers of IMGC

!! Should only be used in situation when many components are used at once
!! Otherwise use the individual headers to save compiletime
!! Never use this in public headers!

*/

//
// NAMES
//

#include <torasu/mod/imgc/pipeline_names.hpp>

//
// DATA-TYPES
//

#include <torasu/mod/imgc/Dcropdata.hpp>
#include <torasu/mod/imgc/Dgraphics.hpp>

//
// RENDERABLES
//

// FILTERS
#include <torasu/mod/imgc/Rgain.hpp>

// TRANSFORM
#include <torasu/mod/imgc/Ralign2d.hpp>
#include <torasu/mod/imgc/Rauto_align2d.hpp>
#include <torasu/mod/imgc/Rcropdata_combined.hpp>
#include <torasu/mod/imgc/Rtransform.hpp>

// GRAPHICS
#include <torasu/mod/imgc/Rgraphics.hpp>
#include <torasu/mod/imgc/Rrothombus.hpp>
#include <torasu/mod/imgc/Rcolor.hpp>
#include <torasu/mod/imgc/Rtext.hpp>
#include <torasu/mod/imgc/Rlayer.hpp>

// MEDIA
#include <torasu/mod/imgc/Rimg_file.hpp>
#include <torasu/mod/imgc/Rmedia_file.hpp>
#include <torasu/mod/imgc/Rmedia_creator.hpp>

//
// OTHER IMGC-TOOLS
//

#include <torasu/mod/imgc/Scaler.hpp>
#include <torasu/mod/imgc/Transform.hpp>
#include <torasu/mod/imgc/ShapeRenderer.hpp>
#include <torasu/mod/imgc/FFmpegLogger.h>
#include <torasu/mod/imgc/MediaDecoder.hpp>
#include <torasu/mod/imgc/MediaEncoder.hpp>

#endif // INCLUDE_TORASU_MOD_IMGC_IMGC_FULL_HPP_
