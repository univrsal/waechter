cmake_minimum_required(VERSION 3.16)

# Project definition
project(waechter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME}
    Source/main.cpp
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)

# link against libbpf
target_link_libraries(${PROJECT_NAME} PRIVATE bpf)
