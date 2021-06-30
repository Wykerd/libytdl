# libytdl

# NOTE: libytdl and its documentation is still very much incomplete. Please be patient or help by contributing!

A C library for downloading youtube videos.

Included is two libraries:
- `libytdlc` which is the core library without any http implementations
- `libytdl` which uses curl as a http client implementation

You can pick to use `libytdlc` if your project uses and existing http implemenation.

# Features
- Small and embeddable
- Simple API

# Dependencies

## External
- [uriparser](https://uriparser.github.io/)

## Included (Packaged)
- libregexp & libunicode from [Fabrice Bellard's QuickJS engine](https://bellard.org/quickjs/)
- [yyjson](https://github.com/ibireme/yyjson)