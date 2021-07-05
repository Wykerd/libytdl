# libytdl Documentation

**NOTE: libytdl and its documentation is still very much incomplete. Please be patient or help by contributing!**

## Why libytdl?
Other tools like `youtube-dl` don't provide developers with a simple embedding API and you need to resort to calling them from the commandline.

libytdl aims to provide a simple embeddable method for downloading and consuming youtube videos in your own applications. 

## What's includes?
libytdl is split into 3 seperate libraries under the same prefix
- `libytdlcore` - the core library
    - Video info extraction
    - Format url deciphering
    - HTTP request generation
- `libytdlhttp` - the http client library
    - Video downloader
- `libytdlav` - video and audio file handling library
    - matroska (mkv) remuxer to merge audio and video streams

Additionally a cli is being implemented to use the library called `ytdlcli`.

## Features
- Small and embeddable
- Simple API

## Support
libytdl is tested on both GNU/Linux and macOS and should work without any issues on those platforms.

# Getting Started
- \ref buildlinux "Building on GNU/Linux"
- \ref buildmacos "Building on macOS"

# Dependencies

## Core dependencies (libytdlcore)

### Included (Packaged)
- [QuickJS](https://bellard.org/quickjs/)
- [yyjson](https://github.com/ibireme/yyjson)

## Http dependencies (libytdlhttp)

### External
- [OpenSSL](https://www.openssl.org/)
- [libuv](https://libuv.org/)

### Included (submodules)
- [llhttp](https://github.com/nodejs/llhttp)

## AV dependencies (libytdlav)

### External
- [FFmpeg](http://ffmpeg.org/) (libavformat & libavcodec)
