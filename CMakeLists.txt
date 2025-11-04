cmake_minimum_required(VERSION 3.16)

project(waechter VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WAECHTER_WITH_IPQUERY_IO_API "Enable ipquery.io API integration" ON)

add_subdirectory(Thirdparty)
add_subdirectory(Source)

string(TIMESTAMP TODAY "%Y.%m.%d %H:%M")

execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

target_compile_definitions(waechterd PRIVATE
        GIT_BRANCH="${GIT_BRANCH}   "
        GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
        WAECHTER_VERSION="${CMAKE_PROJECT_VERSION}"
        BUILD_TIME="${TODAY}"
)

target_compile_definitions(waechter PUBLIC
        GIT_BRANCH="${GIT_BRANCH}   "
        GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
        WAECHTER_VERSION="${CMAKE_PROJECT_VERSION}"
        BUILD_TIME="${TODAY}"
        ICON_PATH="${CMAKE_SOURCE_DIR}/Meta/Icon.png"
        FONT_PATH="${CMAKE_SOURCE_DIR}/Thirdparty/jetbrains-mono/JetBrainsMono-Regular.ttf"
)
