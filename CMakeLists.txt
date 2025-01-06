cmake_minimum_required(VERSION 3.12)
project(cellular-spectrum-analyzer)

cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0075 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

include(FindLibHackRF)
include(FindLibUSB)

message(STATUS "LIBHACKRF_INCLUDE_DIRS - ${LIBHACKRF_INCLUDE_DIR}")
message(STATUS "LIBHACKRF_LIBRARIES - ${LIBHACKRF_LIBRARIES}")

message(STATUS "LIBUSB_INCLUDE_DIR - ${LIBUSB_INCLUDE_DIR}")
message(STATUS "LIBUSB_LIBRARIES - ${LIBUSB_LIBRARIES}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LIBHACKRF_INCLUDE_DIR})
include_directories(${LIBUSB_INCLUDE_DIR})

add_subdirectory(libs/hackrf_sweeper)
include_directories(libs/hackrf_sweeper/include)

add_executable(cellular-spectrum-analyzer src/main.cpp)

target_link_libraries(cellular-spectrum-analyzer PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets ${LIBHACKRF_LIBRARIES} ${LIBUSB_LIBRARIES} hackrf_sweeper)
