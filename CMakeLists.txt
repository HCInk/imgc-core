cmake_minimum_required (VERSION 3.0)
project(imgc VERSION 0.0.4)
set(CMAKE_CXX_STANDARD 20)

if (NOT MINGW AND NOT APPLE)
	set(IMGC_DEBUG_ASAN true)
endif()

set(IMGC_SRC
	${CMAKE_CURRENT_LIST_DIR}/src/Rimg_file.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/Rvideo_file.cpp)
add_library(imgc SHARED ${IMGC_SRC})

set(IMGC_EXAMPLE_SRC
	${CMAKE_CURRENT_LIST_DIR}/example/torasu-imgc-example.cpp)
	
add_executable(imgc-example ${IMGC_EXAMPLE_SRC})

set_target_properties(imgc PROPERTIES LINK_FLAGS "-Wl,-rpath,/usr/local/lib")
set_target_properties(imgc-example PROPERTIES LINK_FLAGS "-Wl,-rpath,/usr/local/lib")

if (IMGC_DEBUG_ASAN)
	target_link_libraries(imgc asan)
	target_link_libraries(imgc-example asan)
endif()


target_link_libraries(imgc torasu torasustd)
target_link_libraries(imgc-example torasu torasustd imgc avcodec avformat swscale avutil)

target_link_libraries(imgc lodepng)

# Includes for others

include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")