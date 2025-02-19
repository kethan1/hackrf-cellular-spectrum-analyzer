cmake_minimum_required(VERSION 3.12)
project(cellular-spectrum-analyzer)

cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0075 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui Widgets OpenGL Concurrent Svg PrintSupport REQUIRED)

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets OpenGL Concurrent Svg PrintSupport REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

include(FindLibHackRF)
include(FindLibUSB)

# Check for libraries
if (NOT LIBHACKRF_INCLUDE_DIR OR NOT LIBHACKRF_LIBRARIES)
    message(FATAL_ERROR "Could not find HackRF libraries. Please make sure FindLibHackRF.cmake is available.")
endif()

if (NOT LIBUSB_INCLUDE_DIR OR NOT LIBUSB_LIBRARIES)
    message(FATAL_ERROR "Could not find libusb libraries. Please make sure FindLibUSB.cmake is available.")
endif()

add_subdirectory(libs/hackrf_sweeper EXCLUDE_FROM_ALL)

set(qwt_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/qwt)

file(GLOB QWT_SOURCES ${qwt_SOURCE_DIR}/src/*.cpp)
file(GLOB QWT_HEADERS ${qwt_SOURCE_DIR}/src/*.h)
file(GLOB QWT_CLASS_HEADERS ${qwt_SOURCE_DIR}/classincludes/*)

add_library(qwt ${QWT_SOURCES})

target_include_directories(qwt PUBLIC ${qwt_SOURCE_DIR}/src/ ${qwt_SOURCE_DIR}/classincludes/)
target_link_libraries(qwt PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::OpenGL Qt${QT_VERSION_MAJOR}::Concurrent Qt${QT_VERSION_MAJOR}::Svg Qt${QT_VERSION_MAJOR}::PrintSupport)

add_executable(${PROJECT_NAME} src/main.cpp src/hackrf_controller.cpp include/hackrf_controller.hpp src/main_window.cpp include/main_window.hpp src/dataset_spectrum.cpp include/dataset_spectrum.hpp src/waterfall_raster_data.cpp include/waterfall_raster_data.hpp)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${LIBHACKRF_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR} libs/hackrf_sweeper/include include)
target_link_libraries(${PROJECT_NAME} PRIVATE hackrf_sweeper Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Widgets qwt ${LIBHACKRF_LIBRARIES} ${LIBUSB_LIBRARIES})
