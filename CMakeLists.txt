cmake_minimum_required(VERSION 3.10)

project(ytdl LANGUAGES C VERSION 0.0.0)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(regexp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libunicode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libregexp.c
)

target_include_directories(regexp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
)

add_executable(regexc 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/regexc.c
)

find_package(uriparser REQUIRED)

add_library(ytdlc SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/yyjson.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net.c
)

add_executable(ytdl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli.c
)

target_link_libraries(regexc regexp)
target_link_libraries(ytdlc regexp uriparser::uriparser)
target_link_libraries(ytdl ytdlc)