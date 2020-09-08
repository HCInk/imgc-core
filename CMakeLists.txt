cmake_minimum_required(VERSION 3.0)
project(imgc VERSION 0.0.4)
set(CMAKE_CXX_STANDARD 20)

option(IMGC_SDL_EXAMPLE "Generate SDL example" ON)

option(IMGC_TESTS "Generate test Executable" ON)
if (NOT MINGW AND NOT APPLE)
    set(IMGC_DEBUG_ASAN true)
endif ()

include_directories(${CMAKE_CURRENT_LIST_DIR}/thirdparty/lodepng)

set(IMGC_SRC
		${CMAKE_CURRENT_LIST_DIR}/thirdparty/lodepng/lodepng.cpp
		${CMAKE_CURRENT_LIST_DIR}/src/Rimg_file.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/MediaDecoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Scaler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rmedia_file.cpp
		${CMAKE_CURRENT_LIST_DIR}/src/Ralign2d.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rgain.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Dcropdata.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/Rcropdata.cpp)

add_library(imgc SHARED ${IMGC_SRC})

set(IMGC_EXAMPLE_SRC
		${CMAKE_CURRENT_LIST_DIR}/example/torasu-imgc-example.cpp)

if (IMGC_SDL_EXAMPLE)
	set(IMGC_EXAMPLE_SRC ${IMGC_EXAMPLE_SRC}
		${CMAKE_CURRENT_LIST_DIR}/example/sdl_example.cpp)
	add_definitions( -DIMGC_SDL_EXAMPLE )
endif ()

add_executable(imgc-example ${IMGC_EXAMPLE_SRC})

set_target_properties(imgc PROPERTIES LINK_FLAGS "-Wl,-rpath,/usr/local/lib")
set_target_properties(imgc-example PROPERTIES LINK_FLAGS "-Wl,-rpath,/usr/local/lib")

if (IMGC_DEBUG_ASAN)
    target_link_libraries(imgc-example asan)
endif ()

if (APPLE)
    add_subdirectory(thirdparty/FFmpeg)
    include_directories(/usr/local/include)
    target_link_directories(imgc PUBLIC /usr/local/lib)
    target_link_directories(imgc-example PUBLIC /usr/local/lib)
    target_link_libraries(imgc torasu torasustd FFmpeg)
    target_link_libraries(imgc-example torasu torasustd imgc FFmpeg)
else ()
    target_link_libraries(imgc torasu torasustd avcodec avformat swscale avutil)
    target_link_libraries(imgc-example torasu torasustd imgc)
#    target_link_libraries(imgc lodepng)
endif ()
# Includes for others

if(IMGC_SDL_EXAMPLE)
    target_link_libraries(imgc-example SDL2)
    target_link_libraries(imgc-example SDL2_ttf)
    configure_file(assets/Roboto-Regular.ttf . COPYONLY)
endif()

if(IMGC_TESTS)
    add_executable(imgc-tests ${CMAKE_CURRENT_LIST_DIR}/tests/extraction.cpp)
    if(APPLE)
    target_link_libraries(imgc-tests torasu torasustd imgc avcodec avformat swscale avutil)
    else()
        target_link_libraries(imgc-tests asan torasu torasustd imgc avcodec avformat swscale avutil)

    endif()
    set_target_properties(imgc-tests PROPERTIES LINK_FLAGS "-Wl,-rpath,/usr/local/lib")


endif()
include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")


# Installation

install(TARGETS imgc DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/torasu DESTINATION include)