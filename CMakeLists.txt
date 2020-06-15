cmake_minimum_required(VERSION 3.0)
project(imgc VERSION 0.0.4)
set(CMAKE_CXX_STANDARD 20)

set(IMGC_SDL_EXAMPLE true)

if (NOT MINGW AND NOT APPLE)
    set(IMGC_DEBUG_ASAN true)
endif ()

if (APPLE)
    set(IMGC_SRC
            ${CMAKE_CURRENT_LIST_DIR}/src/Rimg_file.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/Rvideo_file.cpp thirdparty/lodepng.cpp thirdparty/lodepng.h src/VideoFileDeserializer.cpp src/VideoFileDeserializer.hpp)
else ()
    set(IMGC_SRC
            ${CMAKE_CURRENT_LIST_DIR}/src/Rimg_file.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/Rvideo_file.cpp)
endif ()
add_library(imgc SHARED ${IMGC_SRC})
if (IMGC_SDL_EXAMPLE)
    set(IMGC_EXAMPLE_SRC ${CMAKE_CURRENT_LIST_DIR}/example/sdl_example.cpp)
else ()
    set(IMGC_EXAMPLE_SRC
            ${CMAKE_CURRENT_LIST_DIR}/example/torasu-imgc-example.cpp)
endif ()

add_executable(imgc-example ${IMGC_EXAMPLE_SRC})

set_target_properties(imgc PROPERTIES LINK_FLAGS "-Wl,-rpath,/usr/local/lib")
set_target_properties(imgc-example PROPERTIES LINK_FLAGS "-Wl,-rpath,/usr/local/lib")

if (IMGC_DEBUG_ASAN)
    target_link_libraries(imgc asan)
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
    target_link_libraries(imgc torasu torasustd)
    target_link_libraries(imgc-example torasu torasustd imgc avcodec avformat swscale avutil)
    target_link_libraries(imgc lodepng)
endif ()
# Includes for others

if(IMGC_SDL_EXAMPLE)
    target_link_libraries(imgc-example SDL2)
    target_link_libraries(imgc-example SDL2_ttf)
    configure_file(assets/Roboto-Regular.ttf . COPYONLY)

endif()

include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")