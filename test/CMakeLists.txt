cmake_minimum_required(VERSION 2.8.0)

project(promise_test)

include(ExternalProject)
find_package(Git REQUIRED)

ExternalProject_Add(
    LTest
    PREFIX ${CMAKE_BINARY_DIR}/deps
    GIT_REPOSITORY https://github.com/0of/LTest.git
    TIMEOUT 10
    UPDATE_COMMAND ${GIT_EXECUTABLE} pull
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
)

ExternalProject_Get_Property(LTest source_dir)
include_directories(${source_dir})
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")

file(GLOB tests "${CMAKE_CURRENT_SOURCE_DIR}/base/*.cpp")
add_executable(promise_test ${tests})

