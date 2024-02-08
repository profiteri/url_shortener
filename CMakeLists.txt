cmake_minimum_required(VERSION 3.16)
project(cbdp)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wall -Wextra -Werror -fsanitize=address")
set(protobuf_MODULE_COMPATIBLE ON CACHE BOOL "")

#link protobuf
find_package(protobuf REQUIRED)
protobuf_generate_cpp(pb_src pb_hdr proto/requests.proto)
add_library(pb_lib ${pb_hdr} ${pb_src})

target_include_directories(pb_lib INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(pb_lib PUBLIC protobuf::libprotobuf)

#create target
add_executable(node src/main.cpp)

#add include dirs path
target_include_directories(node PRIVATE src)

#link http server library
find_library(LIB_TO_INCLUDE httpserver /usr/local/lib)
target_link_libraries(node PRIVATE ${LIB_TO_INCLUDE})

#link protobuf library
target_link_libraries(node PRIVATE pb_lib)

#add subdirs
add_subdirectory(src/http_server)
add_subdirectory(src/node)
add_subdirectory(src/raft)
add_subdirectory(src/storage)

