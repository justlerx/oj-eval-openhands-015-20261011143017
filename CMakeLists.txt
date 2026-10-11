cmake_minimum_required(VERSION 3.10)
project(FileStorage)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(code main.cpp)

set_target_properties(code PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)