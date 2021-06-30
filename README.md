# libytdl

A C library for downloading youtube videos.

Included is two libraries:
- `libytdlc` which is the core library without any http implementations
- `libytdl` which uses curl as a http client implementation

You can pick to use `libytdlc` if your project uses and existing http implemenation.

# Dependencies

## External
- (uriparser)[https://uriparser.github.io/]

## Included (Packaged)
- libregexp & libunicode from Fabrice Bellard's QuickJS engine
- yyjson