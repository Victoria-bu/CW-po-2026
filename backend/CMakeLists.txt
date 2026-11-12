cmake_minimum_required(VERSION 3.16)
project(search_server CXX)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES src/*.cpp)
add_executable(search_server ${SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(search_server Threads::Threads)