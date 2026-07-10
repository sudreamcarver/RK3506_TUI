cmake_minimum_required(VERSION 3.16)

project(rk3506_tcp_test_sender LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(tcp_sensor_sender
    src/tcp_sensor_sender.cpp
    ../rk3506_tcp/src/protocol.cpp
)

target_include_directories(tcp_sensor_sender PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../rk3506_tcp/include
)

target_compile_options(tcp_sensor_sender PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CXX_COMPILER_ID:GNU>:-Wno-psabi>
)
