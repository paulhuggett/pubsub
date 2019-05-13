cmake_minimum_required (VERSION 3.10)
project (pubsub CXX)

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package (Threads)
set (CMAKE_REQUIRED_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

add_executable (pubsub
	       publish_and_subscribe/main.cpp
	       publish_and_subscribe/pubsub.cpp
	       publish_and_subscribe/pubsub.hpp
	       )
target_link_libraries (pubsub PRIVATE ${CMAKE_THREAD_LIBS_INIT})
