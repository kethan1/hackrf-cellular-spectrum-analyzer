cmake_minimum_required(VERSION 3.12)
project(cellular-spectrum-analyzer)

cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0075 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)

set(QCUSTOMPLOT_VERSION 2.1.1 CACHE STRING "")
set(QCUSTOMPLOT_URL "https://www.qcustomplot.com/release/${QCUSTOMPLOT_VERSION}/QCustomPlot-source.tar.gz" CACHE STRING "")

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui Widgets REQUIRED)

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets REQUIRED)

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

message(STATUS "LIBHACKRF_INCLUDE_DIR - ${LIBHACKRF_INCLUDE_DIR}")
message(STATUS "LIBHACKRF_LIBRARIES - ${LIBHACKRF_LIBRARIES}")
message(STATUS "LIBUSB_INCLUDE_DIR - ${LIBUSB_INCLUDE_DIR}")
message(STATUS "LIBUSB_LIBRARIES - ${LIBUSB_LIBRARIES}")

add_subdirectory(libs/hackrf_sweeper EXCLUDE_FROM_ALL)

add_subdirectory(libs/QCustomPlot-library)

add_executable(${PROJECT_NAME} src/main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${LIBHACKRF_INCLUDE_DIR} ${LIBUSB_INCLUDE_DIR} libs/hackrf_sweeper/include)
target_link_libraries(${PROJECT_NAME} PRIVATE hackrf_sweeper Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Widgets qcustomplot ${LIBHACKRF_LIBRARIES} ${LIBUSB_LIBRARIES})
