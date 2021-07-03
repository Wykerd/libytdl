cmake_minimum_required(VERSION 3.10)

project(ytdl LANGUAGES C VERSION 0.0.0)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Compiler Config
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)  

# Build Options
option(BUILD_SHARED_LIBS "Build shared libraries (rather than static ones)" ON)
option(YTDL_BUILD_HTTP   "Build the http client (libytdl)" ON)
option(YTDL_BUILD_CLI    "Build the commandline interface" ON)
option(YTDL_BUILD_REGEXC "Build the RegExp compile tool" OFF)

# Find dependencies
find_package(uriparser REQUIRED)
find_package(UV REQUIRED)
find_package(Threads REQUIRED)
if(APPLE)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
    find_package(OpenSSL REQUIRED)
else()
    find_package(OpenSSL REQUIRED)
endif()

# Targets
add_library(regexp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libunicode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libregexp.c
)

target_include_directories(regexp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(regexp PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(ytdlc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/yyjson.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net.c
)

target_include_directories(ytdlc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ytdlc PRIVATE
    regexp 
    uriparser::uriparser
)

if (YTDL_BUILD_REGEXC)
    add_executable(regexc 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/regexc.c
    )

    target_link_libraries(regexc PRIVATE regexp)
endif()

if (YTDL_BUILD_HTTP)
    add_library(ytdl
        ${CMAKE_CURRENT_SOURCE_DIR}/src/http/http.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/http/net.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/http/tcp.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dl.c
    ) 

    target_include_directories(ytdl PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include 
        ${UV_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/llhttp/build
        ${OPENSSL_INCLUDE_DIR}
    )

    target_link_libraries(ytdl PRIVATE
        ytdlc

        ${CMAKE_CURRENT_SOURCE_DIR}/deps/llhttp/build/libllhttp.a
        OpenSSL::SSL 
        OpenSSL::Crypto 
        
        Threads::Threads
    )

    target_link_libraries(ytdl PUBLIC ${UV_LIBRARIES})

    if (YTDL_BUILD_CLI)
        add_executable(ytdlcli ${CMAKE_CURRENT_SOURCE_DIR}/src/cli.c)
        target_link_libraries(ytdlcli PRIVATE ytdl)
    endif()
endif()
