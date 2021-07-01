# libytdl Documentation

# NOTE: libytdl and its documentation is still very much incomplete. Please be patient or help by contributing!

## Why libytdl?
Other tools like `youtube-dl` don't provide developers with a simple embedding API and you need to resort to calling them from the commandline.

`libytdl` and its core `libytdlc` aims to provide a simple embeddable method for downloading and consuming youtube videos in your own applications. 

## What's includes?
- `libytdlc` which is the core library without any http implementations
- `libytdl` which uses its own http client implementation

You can pick to use `libytdlc` if your project uses and existing http implemenation.

## Features
- Small and embeddable
- Simple API

## Support
libytdl is tested on both GNU/Linux and macOS and should work without any issues on those platforms.

# Getting Started
- \ref buildlinux "Building on GNU/Linux"
- \ref buildmacos "Building on macOS"

# Dependencies

## Core dependencies (libytdlc)

### External
- [uriparser](https://uriparser.github.io/)

### Included (Packaged)
- libregexp & libunicode from [Fabrice Bellard's QuickJS engine](https://bellard.org/quickjs/)
- [yyjson](https://github.com/ibireme/yyjson)

## Additional dependencies (libytdl)

### External
- [OpenSSL](https://www.openssl.org/)
- [libuv](https://libuv.org/)

### Included (submodules)
- [llhttp](https://github.com/nodejs/llhttp)
