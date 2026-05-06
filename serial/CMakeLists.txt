cmake_minimum_required(VERSION 3.10)

project(mcu_serial_receiver C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(mcu_serial STATIC
    src/mcu_serial.c
)

target_include_directories(mcu_serial PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(mcu_serial PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(mcu_serial_receiver
    src/cli.c
)

target_compile_options(mcu_serial_receiver PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(mcu_serial_receiver PRIVATE
    mcu_serial
)
