cmake_minimum_required(VERSION 3.16)

project(hellowWorld LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    ftxui
    SOURCE_DIR /home/carver/Documents/FTXUI
)

FetchContent_MakeAvailable(ftxui)

file(GLOB SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(hellowWorld ${SRC})

target_include_directories(hellowWorld
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(hellowWorld
    PRIVATE
        -Wall
        -Wextra
        -pedantic-errors
        -Wconversion
        -Wsign-conversion
        -Werror
)

target_link_libraries(hellowWorld
    PRIVATE
        ftxui::screen
        ftxui::dom
        ftxui::component
)
