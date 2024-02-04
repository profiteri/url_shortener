cmake_minimum_required(VERSION 3.16)
project(cbdp)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wall -Wextra -Werror -fsanitize=address")
#-Wconversion

#create target
add_executable(node src/main.cpp)

#add include dirs path
target_include_directories(node PRIVATE src)

#link http server library
find_library(LIB_TO_INCLUDE httpserver /usr/local/lib)
target_link_libraries(node ${LIB_TO_INCLUDE})

#add subdirs
add_subdirectory(src/http_server)
add_subdirectory(src/node)
add_subdirectory(src/raft)

