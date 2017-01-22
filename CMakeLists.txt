# remove *.skeleton.cpp before run cmake

cmake_minimum_required(VERSION 2.8)
project(thrift-cpp-demo)


include_directories("/usr/include/boost/")

#Make sure gen-cpp files can be included
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/gen-cpp")


set(SERVER_SOURCE "server.cpp")
set(CLIENT_SOURCE "client.cpp")
set(THRIFT_STATIC_LIB "/usr/local/lib/libthrift.a")
set(THRIFT_NONBLOCK_LIB "/usr/local/lib/libthriftnb.a")
set(EVENT_LIB "/usr/local/Cellar/libevent/2.0.22/lib/libevent.a")
# find_package(libevent REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -pedantic -O3 -Wno-long-long -fno-omit-frame-pointer")

file(GLOB thrift_gen_cpp_SOURCES "gen-cpp/*.cpp")

message(STATUS "haha ${thrift_gen_cpp_SOURCES}")

add_library(thrift_gen_cpp STATIC ${thrift_gen_cpp_SOURCES})
target_link_libraries(thrift_gen_cpp ${THRIFT_STATIC_LIB})
target_link_libraries(thrift_gen_cpp pthread)


add_executable(server ${SERVER_SOURCE})
target_link_libraries(server thrift_gen_cpp)
target_link_libraries(server ${THRIFT_STATIC_LIB})
target_link_libraries(server ${THRIFT_NONBLOCK_LIB})
target_link_libraries(server pthread)
target_link_libraries(server ${EVENT_LIB})

add_executable(client ${CLIENT_SOURCE})
target_link_libraries(client thrift_gen_cpp)
target_link_libraries(client ${THRIFT_STATIC_LIB})
target_link_libraries(client pthread)
