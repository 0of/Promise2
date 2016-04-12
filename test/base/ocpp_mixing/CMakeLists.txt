if(APPLE)
  file(GLOB tests "${CMAKE_CURRENT_SOURCE_DIR}/*.mm")
  add_executable(ocpp_mixing ${tests})

  target_link_libraries(ocpp_mixing
    "-framework Cocoa"
    "-framework AppKit"
    "-framework CoreData"
    "-framework Foundation"
  )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc")

elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  find_package(Threads REQUIRED)

  set(default_objc2 "${CMAKE_BINARY_DIR}/deps/objc2")

  ExternalProject_Add(
    libobjc2
    PREFIX ${CMAKE_BINARY_DIR}/deps
    GIT_REPOSITORY https://github.com/gnustep/libobjc2.git
    TIMEOUT 10
    UPDATE_COMMAND ${GIT_EXECUTABLE} pull
    CMAKE_ARGS -DBUILD_STEP=install -DCMAKE_INSTALL_PREFIX=${default_objc2}
    LOG_DOWNLOAD ON
  )

  file(GLOB tests "${CMAKE_CURRENT_SOURCE_DIR}/*.mm")
  add_executable(ocpp_mixing ${tests})

  execute_process(COMMAND gnustep-config "--installation-domain-for=libobjc2"
                  OUTPUT_VARIABLE install_type 
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(install_type)
    # install_type may be equals to `SYSTEM`
    execute_process(COMMAND gnustep-config "--variable=GNUSTEP_SYSTEM_HEADERS"
                    OUTPUT_VARIABLE gnustep_includes 
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(COMMAND gnustep-config "--variable=GNUSTEP_SYSTEM_LIBRARIES"
                    OUTPUT_VARIABLE gnustep_libs 
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(COMMAND gnustep-config "--variable=GNUSTEP_${install_type}_HEADERS"
                    OUTPUT_VARIABLE install_gnustep_includes 
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(COMMAND gnustep-config "--variable=GNUSTEP_${install_type}_LIBRARIES"
                    OUTPUT_VARIABLE install_gnustep_libs 
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    include_directories(${gnustep_includes} ${install_gnustep_includes})
    link_directories(${gnustep_libs} ${install_gnustep_libs})
  else()
    execute_process(COMMAND gnustep-config "--variable=GNUSTEP_SYSTEM_HEADERS"
                    OUTPUT_VARIABLE gnustep_includes 
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(COMMAND gnustep-config "--variable=GNUSTEP_SYSTEM_LIBRARIES"
                    OUTPUT_VARIABLE gnustep_libs 
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

    include_directories(${gnustep_includes} ${default_objc2}/include)
    link_directories(${gnustep_libs} ${default_objc2}/lib)
  endif()

  execute_process(COMMAND gnustep-config "--objc-flags"
                    OUTPUT_VARIABLE gnustep_flags
                    OUTPUT_STRIP_TRAILING_WHITESPACE)

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${gnustep_flags} -fnext-runtime -fblocks -fobjc-arc")

  target_link_libraries(ocpp_mixing
    ${CMAKE_THREAD_LIBS_INIT}
    dispatch
    objcxx
    gnustep-base
  )
endif()
