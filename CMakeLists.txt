cmake_minimum_required(VERSION 4.0)
project(untitled)

set(CMAKE_CXX_STANDARD 20)

find_package(SQLite3 REQUIRED)      # modern CMake on many systems
add_executable(untitled main.cpp)
target_link_libraries(untitled PRIVATE SQLite::SQLite3)