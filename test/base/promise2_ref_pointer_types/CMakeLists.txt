find_package(Threads REQUIRED)

file(GLOB headers "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB tests "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(promise2_ref_pointer_types ${tests} ${headers})
target_link_libraries(promise2_ref_pointer_types ${CMAKE_THREAD_LIBS_INIT})