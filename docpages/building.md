# Build libytdl

Guides for building on various platforms are given below:
- \subpage buildlinux GNU/Linux
- \subpage buildmacos macOS

\page buildlinux Building on GNU/Linux
# Prerequisites
This library uses `cmake` and a modern C compiler like `gcc` or `clang`. You can install it with your distribution's package manager.
- ArchLinux: `sudo pacman -S cmake gcc`
- Debian: `sudo apt install cmake gcc`

# Building
The build process is similar to other CMake projects
```
git clone https://github.com/Wykerd/libytdl.git
cd libytdl
mkdir build
cd build
cmake ..
make
```
Once built you can link other programs to either `libytdl.so` or `libytdlc.so`

# Building the documentation (optional)
You can make a local copy of the documentation by running:
```
doxygen Doxyfile
```
The resulting documentation is output in `docs/` directory

\page buildmacos Building on macOS
# Prerequisites 
This library uses `cmake` and `clang` to build. You can install it with [homebrew](https://brew.sh).
```
brew install cmake
```

Next install the external dependencies
```
brew install uriparser
```

# Building
The build process is similar to other CMake projects
```
git clone https://github.com/Wykerd/libytdl.git
cd libytdl
mkdir build
cd build
cmake ..
make
```
Once built you can link other programs to either `libytdl.dylib` or `libytdlc.dylib`

# Building the documentation (optional)
You can make a local copy of the documentation by running:
```
doxygen Doxyfile
```
The resulting documentation is output in `docs/` directory