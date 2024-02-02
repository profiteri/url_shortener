cmake_minimum_required(VERSION 3.16)
project(cbdp)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wall -Wextra -Werror -fsanitize=address")
#-Wconversion

add_executable(node
        src/node.cpp)

