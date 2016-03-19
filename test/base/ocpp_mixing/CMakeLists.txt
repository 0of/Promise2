find_package(Threads REQUIRED)

file(GLOB tests "${CMAKE_CURRENT_SOURCE_DIR}/*.mm")
add_executable(ocpp_mixing ${tests})

if(APPLE)
  target_link_libraries(ocpp_mixing
    "-framework Cocoa"
    "-framework AppKit"
    "-framework CoreData"
    "-framework Foundation"
  )
else()
  execute_process(COMMAND gnustep-config "--variable=GNUSTEP_SYSTEM_HEADERS"
                                  OUTPUT_VARIABLE gnustep_includes 
                                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND gnustep-config "--variable=GNUSTEP_SYSTEM_LIBRARIES"
                                  OUTPUT_VARIABLE gnustep_libs 
                                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND gnustep-config "--objc-flags" 
  				  OUTPUT_VARIABLE flags 
  				  OUTPUT_STRIP_TRAILING_WHITESPACE)

  set(CMAKE_CXX_FLAGS ${flags})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fblocks -fobjc-arc")

  include_directories(${gnustep_includes})
  link_directories(${gnustep_libs})

  target_link_libraries(ocpp_mixing
    ${CMAKE_THREAD_LIBS_INIT}
    dispatch
    objc
    gnustep-base
  )
endif()
