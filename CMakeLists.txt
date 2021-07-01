cmake_minimum_required(VERSION 3.10)

project(ytdl LANGUAGES C VERSION 0.0.0)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(uriparser REQUIRED)
find_package(UV REQUIRED)
find_package(Threads REQUIRED)
if(APPLE)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
    find_package(OpenSSL REQUIRED)
else()
    find_package(OpenSSL REQUIRED)
endif()

add_library(regexp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libunicode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libregexp.c
)

add_executable(regexc 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/regexc.c
)

add_library(ytdlc SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/yyjson.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net.c
)

add_library(ytdl SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/http/http.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/http/net.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/http/tcp.c
)

add_executable(ytdlcli
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli.c
)

# Properties
set_target_properties(ytdlc PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

set_target_properties(ytdl PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

set_target_properties(ytdlcli PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(regexp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
)

target_include_directories(ytdlc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
)

target_include_directories(ytdl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/llhttp/build
    ${OPENSSL_INCLUDE_DIR}
    ${UV_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(regexc 
    regexp
)

target_link_libraries(ytdlc 
    regexp 
    uriparser::uriparser
)

target_link_libraries(ytdl 
    ytdlc

    ${CMAKE_CURRENT_SOURCE_DIR}/deps/llhttp/build/libllhttp.a
    OpenSSL::SSL 
    OpenSSL::Crypto 
    ${UV_LIBRARIES} 
    
    Threads::Threads
)

target_link_libraries(ytdlcli
    ytdl
)