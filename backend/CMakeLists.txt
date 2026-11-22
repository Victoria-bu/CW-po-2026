cmake_minimum_required(VERSION 3.16)
project(search_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(search_server
    src/main.cpp
    src/WebApi.cpp
    src/WebSocketApi.cpp
    src/SearchServer.cpp
    src/InvertedIndex.cpp
    src/ThreadPool.cpp
    src/DataLoader.cpp
    src/OllamaClient.cpp
)
target_link_libraries(search_server PRIVATE Threads::Threads)

add_executable(search src/cli.cpp)
target_link_libraries(search PRIVATE Threads::Threads)

