
project(profane_analyser)

find_package(SDL2 REQUIRED)
find_package(SDL2_IMAGE REQUIRED)
find_package(SDL2_TTF REQUIRED)

include_directories(
	${SDL2_INCLUDE_DIRS}
	${SDL2_IMAGE_INCLUDE_DIRS}
	${SDL2_TTF_INCLUDE_DIRS}
	../c++11-tracer/include)

add_executable(profane_analyser
	config.h
	histogram_view.cpp
	histogram_view.h
	main.cpp
	pch.cpp
	pch.h
	text_renderer.cpp
	text_renderer.h
	time_scale_view.cpp
	time_scale_view.h
	workload.cpp
	workload.h)

target_link_libraries(profane_analyser
	${SDL2_LIBRARIES}
	${SDL2_IMAGE_LIBRARIES}
	${SDL2_TTF_LIBRARIES})
