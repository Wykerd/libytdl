# libytdl

# NOTE: libytdl and its documentation is still very much incomplete. Please be patient or help by contributing!

A C library for downloading youtube videos.

`libytdl` and its core `libytdlc` aims to provide a simple embeddable method for downloading and consuming youtube videos in your own applications. 

## What's includes?
- `libytdlc` which is the core library without any http implementations
- `libytdl` which uses its own http client implementation

You can pick to use `libytdlc` if your project uses and existing http implemenation.

# Features
- Small and embeddable
- Simple API

# Dependencies

## Included (Packaged)
- libregexp & libunicode from [Fabrice Bellard's QuickJS engine](https://bellard.org/quickjs/)
- [yyjson](https://github.com/ibireme/yyjson)