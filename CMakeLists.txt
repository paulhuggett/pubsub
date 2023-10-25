cmake_minimum_required (VERSION 3.10)
project (pubsub CXX)

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package (Threads)
set (CMAKE_REQUIRED_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

add_executable (pubsub main.cpp pubsub.hpp)
set_target_properties (pubsub PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED Yes
)
target_link_libraries (pubsub PRIVATE ${CMAKE_THREAD_LIBS_INIT})
