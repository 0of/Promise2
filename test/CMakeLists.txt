cmake_minimum_required(VERSION 2.8.0)

project(promise_test)

include(ExternalProject)
find_package(Git REQUIRED)

ExternalProject_Add(
    fido
    PREFIX ${CMAKE_BINARY_DIR}/deps
    GIT_REPOSITORY https://github.com/0of/fido.git
    TIMEOUT 10
    UPDATE_COMMAND ${GIT_EXECUTABLE} pull
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
)

ExternalProject_Get_Property(fido source_dir)
include_directories(${source_dir})
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/base/")

if("$ENV{COVERAGE_REPORT}" STREQUAL "1")
  set(COVERAGE_REPORT ON)
else()
  set(COVERAGE_REPORT OFF)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
endif()

if(${COVERAGE_REPORT})
  message("-- coverage report turned ON")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
endif()

add_subdirectory(base/promise2_api)

if(UNIX)
add_subdirectory(base/ocpp_mixing)
endif()

add_subdirectory(base/promise2_ref_pointer_types)

add_custom_target(run_test COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh")

if(${COVERAGE_REPORT})
  set(raw_coverage_file "raw_coverage.info")
  set(coverage_file "coverage.info")

  add_custom_command(TARGET run_test 
  					POST_BUILD
  					COMMAND lcov --directory . --capture --output-file ${raw_coverage_file}
  					COMMENT "-- collect coverage files...")

  add_custom_command(TARGET run_test 
          POST_BUILD
          COMMAND lcov --remove ${raw_coverage_file} 'usr/include/*' 'v1/*' 'src/test/*' --output-file ${coverage_file}
          COMMENT "-- strip coverage info...")

  add_custom_command(TARGET run_test 
  					POST_BUILD
  					COMMAND coveralls-lcov --repo-token $ENV{COVERALLS_TOKEN} ${coverage_file}
  					COMMENT "-- report to coverall.io...")
endif()




