find_package(Threads REQUIRED)

file(GLOB tests "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(recursion_promise_api ${tests})
target_link_libraries(recursion_promise_api ${CMAKE_THREAD_LIBS_INIT})