find_package(Threads REQUIRED)

file(GLOB tests "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(promise2_api ${tests})
target_link_libraries(promise2_api ${CMAKE_THREAD_LIBS_INIT})