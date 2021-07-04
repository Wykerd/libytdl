cmake_minimum_required(VERSION 3.10)

project(ytdl LANGUAGES C VERSION 0.0.0)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Compiler Config
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)  

# Build Options
option(BUILD_SHARED_LIBS "Build shared libraries (rather than static ones)" ON)
option(YTDL_BUILD_HTTP   "Build the http client" ON)
option(YTDL_BUILD_CLI    "Build the commandline interface" ON)
option(YTDL_BUILD_MUX    "Build the small ffmpeg based muxer library" ON)
option(YTDL_BUILD_REGEXC "Build the RegExp compile tool" OFF)

# Check build option validity
if(YTDL_BUILD_CLI AND NOT YTDL_BUILD_HTTP)
    message(FATAL_ERROR "Cannot build cli without building http client")
endif()
if(YTDL_BUILD_CLI AND NOT YTDL_BUILD_MUX)
    message(FATAL_ERROR "Cannot build cli without building muxer")
endif()

# Find dependencies
find_package(Threads REQUIRED)
find_package(UV REQUIRED)
if(APPLE)
    if(NOT OPENSSL_ROOT_DIR)
        set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
    endif()
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

add_library(ytdlcore
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/yyjson.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/url_parser.c
)

target_include_directories(ytdlcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ytdlcore PRIVATE
    regexp 
)

if (YTDL_BUILD_REGEXC)
    add_executable(regexc 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/regexc.c
    )

    target_link_libraries(regexc PRIVATE regexp)
endif()

if (YTDL_BUILD_MUX)
    find_package(FFMPEG REQUIRED COMPONENTS avformat avcodec)
    add_library(ytdlmux
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mux.c
    )
    target_include_directories(ytdlmux PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include 
    )
    target_include_directories(ytdlmux PRIVATE 
        ${FFMPEG_avcodec_INCLUDE_DIRS}
        ${FFMPEG_avformat_INCLUDE_DIRS}
    )
    target_link_libraries(ytdlmux PRIVATE 
        FFMPEG::avformat
        FFMPEG::avcodec
    )
endif() 

if (YTDL_BUILD_HTTP)
    add_library(ytdlhttp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/http/http.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/http/net.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/http/tcp.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dl.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/hashmap.c
    ) 

    target_include_directories(ytdlhttp PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include 
        ${UV_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/llhttp/build
        ${OPENSSL_INCLUDE_DIR}
    )

    target_link_libraries(ytdlhttp PRIVATE
        ytdlcore

        ${CMAKE_CURRENT_SOURCE_DIR}/deps/llhttp/build/libllhttp.a
        OpenSSL::SSL 
        OpenSSL::Crypto 
        
        Threads::Threads
    )

    target_link_libraries(ytdlhttp PUBLIC ${UV_LIBRARIES})

    if (YTDL_BUILD_CLI)
        add_executable(ytdlcli 
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cli.c
        )
        target_link_libraries(ytdlcli PRIVATE ytdlhttp ytdlcore ytdlmux)
    endif()
endif()
