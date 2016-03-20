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
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/base/")

if($ENV{COVERAGE_REPORT} EQUAL 1)
  find_package(CURL REQUIRED)
  set(COVERAGE_REPORT ON)
else()
  set(COVERAGE_REPORT OFF)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if(${COVERAGE_REPORT})
  message("-- coverage report turned ON")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
endif()

add_subdirectory(base/promise2_api)

if(UNIX)
add_subdirectory(base/ocpp_mixing)
endif()

add_custom_target(run_test COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh")

if(${COVERAGE_REPORT})
  set(coverage_file "coverage.info")

  add_custom_command(TARGET run_test 
  					POST_BUILD
  					COMMAND lcov --directory . --capture --output-file ${coverage_file}
  					COMMENT "-- collect coverage files...")

  add_custom_command(TARGET run_test 
  					POST_BUILD
  					COMMAND coveralls-lcov --repo-token $ENV{COVERALLS_TOKEN} ${coverage_file}
  					COMMENT "-- report to coverall.io...")
endif()




