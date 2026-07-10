cmake_minimum_required(VERSION 3.16)

project(rk3506_tcp_receiver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(rk3506_tcp_protocol STATIC
    src/protocol.cpp
)

target_include_directories(rk3506_tcp_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(rk3506_tcp_protocol PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CXX_COMPILER_ID:GNU>:-Wno-psabi>
)

add_executable(rk3506_tcp_receiver
    src/tcp_receiver.cpp
)

target_compile_options(rk3506_tcp_receiver PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CXX_COMPILER_ID:GNU>:-Wno-psabi>
)

target_link_libraries(rk3506_tcp_receiver PRIVATE
    rk3506_tcp_protocol
)
