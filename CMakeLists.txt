cmake_minimum_required(VERSION 3.0)
project(imgc VERSION 0.0.4)
set(CMAKE_CXX_STANDARD 20)

option(IMGC_SDL_EXAMPLE "Generate SDL example" OFF)

option(IMGC_TESTS "Generate test Executable" ON)
if (NOT MINGW AND NOT APPLE AND NOT EMSCRIPTEN)
    set(IMGC_DEBUG_ASAN true)
endif ()

include_directories(${CMAKE_CURRENT_LIST_DIR}/thirdparty/lodepng)

if (EMSCRIPTEN)
	include_directories(~/.emscripten_cache/sysroot/include)
	link_directories(~/.emscripten_cache/sysroot/lib)
	include_directories(${CMAKE_CURRENT_LIST_DIR}/thirdparty/ffmpeg-wasm-build/build/include)
	link_directories(${CMAKE_CURRENT_LIST_DIR}/thirdparty/ffmpeg-wasm-build/build/lib)
endif()

set(IMGC_SRC
		${CMAKE_CURRENT_LIST_DIR}/src/imgc-index.cpp
		${CMAKE_CURRENT_LIST_DIR}/thirdparty/lodepng/lodepng.cpp
		${CMAKE_CURRENT_LIST_DIR}/src/Rimg_file.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/FFmpegLogger.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/MediaDecoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/MediaEncoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Scaler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rmedia_file.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rmedia_creator.cpp
		${CMAKE_CURRENT_LIST_DIR}/src/Ralign2d.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rgain.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Dcropdata.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rcropdata.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rcropdata_combined.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rauto_align2d.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rgraphics.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Dgraphics.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/ShapeRenderer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rrothumbus.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rcolor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rtext.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rlayer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rtransform.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Transform.cpp)

add_library(imgc SHARED ${IMGC_SRC})

set(IMGC_EXAMPLE_SRC
		${CMAKE_CURRENT_LIST_DIR}/example/torasu-imgc-example.cpp)

if(IMGC_SDL_EXAMPLE)
	set(IMGC_EXAMPLE_SRC ${IMGC_EXAMPLE_SRC}
		${CMAKE_CURRENT_LIST_DIR}/example/sdl_example.cpp)
	add_definitions( -DIMGC_SDL_EXAMPLE )
endif()

add_executable(imgc-example ${IMGC_EXAMPLE_SRC})


if(IMGC_DEBUG_ASAN)
    target_link_libraries(imgc-example asan)
endif()

if (EMSCRIPTEN)
	set_target_properties(imgc PROPERTIES LINK_FLAGS "-s USE_FREETYPE=1 -fexceptions")
	set_target_properties(imgc PROPERTIES COMPILE_FLAGS "-fexceptions")
	set_target_properties(imgc-example PROPERTIES LINK_FLAGS "-s USE_FREETYPE=1")
    target_link_libraries(imgc torasu torasustd avcodec avformat swscale avutil)
    target_link_libraries(imgc-example torasu torasustd imgc)
else()
	set_target_properties(imgc PROPERTIES LINK_FLAGS "-Wl,-rpath,/usr/local/lib")
	set_target_properties(imgc-example PROPERTIES LINK_FLAGS "-Wl,-rpath,/usr/local/lib")
	if(APPLE)
		add_subdirectory(thirdparty/FFmpeg)
		include_directories(/usr/local/include)
		include_directories(/opt/homebrew/Cellar/freetype/2.11.0/include/freetype2)
		target_link_directories(imgc PUBLIC /usr/local/lib /opt/homebrew/opt/freetype/lib)
		target_link_directories(imgc-example PUBLIC /usr/local/lib)
		target_link_libraries(imgc torasu torasustd FFmpeg freetype)
		target_link_libraries(imgc-example torasu torasustd imgc FFmpeg)
	else ()
		include_directories(/usr/include/freetype2)
		target_link_libraries(imgc torasu torasustd avcodec avformat swscale avutil)
		target_link_libraries(imgc-example torasu torasustd imgc)
	#    target_link_libraries(imgc lodepng)
	endif ()
endif ()
# Includes for others

if(IMGC_SDL_EXAMPLE)
    include_directories(/opt/homebrew/include)
    target_link_directories(imgc-example PUBLIC /opt/homebrew/lib)
    target_link_libraries(imgc-example SDL2)
    target_link_libraries(imgc-example SDL2_ttf)
    configure_file(assets/Roboto-Regular.ttf . COPYONLY)
endif()

if(IMGC_TESTS)
    add_executable(imgc-tests ${CMAKE_CURRENT_LIST_DIR}/tests/extraction.cpp)
	if (EMSCRIPTEN)
		set_target_properties(imgc-tests PROPERTIES LINK_FLAGS "-s USE_FREETYPE=1")
		target_link_libraries(imgc-tests torasu torasustd imgc avcodec avformat swscale avutil)
    else()
   		set_target_properties(imgc-tests PROPERTIES LINK_FLAGS "-Wl,-rpath,/usr/local/lib")
		if(APPLE)
			target_link_libraries(imgc-tests torasu torasustd imgc avcodec avformat swscale avutil)
		else()
			target_link_libraries(imgc-tests asan torasu torasustd imgc avcodec avformat swscale avutil)
		endif()
	endif()


endif()
include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")


# Installation

install(TARGETS imgc DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/torasu DESTINATION include)
