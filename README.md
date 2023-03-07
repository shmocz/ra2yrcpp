# ra2yrcpp

Library for interacting with Red Alert 2 Yuri's Revenge game process with protobuf based protocol over TCP. Inspired by [s2client-api](https://github.com/Blizzard/s2client-api).

## Building

`ra2yrcpp` depends on following software:

- argparse
- asio
- cmake
- fmt
- protobuf
- python3 (iced-x86 library used for patching)
- websocketpp
- wine (Optional: see section about cross-compilation and protoc)
- xbyak
- zlib

All dependencies except cmake, python, zlib and wine are already included as submodules.

Get the sources and submodules and place `gamemd-spawn.exe` from CnCNet distribution into the project source directory.

```bash
$ git clone --recurse-submodules https://github.com/CnCNet/ra2yrcpp.git
$ cd ra2yrcpp
$ cp <CNCNET_FOLDER>/gamemd-spawn.exe .
```

For `clang-cl`, zlib sources are also needed:

```bash
$ git clone -b v1.2.8 https://github.com/madler/zlib.git 3rdparty/zlib
```

### Build with Docker (recommended)

For convenience, a Docker image is provided for both MinGW and clang-cl toolchains with all necessary dependencies to build the application and related components. MinGW toolchain is used by default.

```bash
$ make docker_build
```

### General build instructions

Make sure Python 3 is installed, then install iced-x86:

```bash
$ pip install --user iced-x86
```

Pick a toolchain of your choice, release type and run make:

```bash
$ export CMAKE_TOOLCHAIN_FILE=<toolchain-path>
$ export CMAKE_RELEASE_TYPE=Release
$ make build
```

This performs the build and installation under `cbuild/<toolchain-id>-<release-type>`.

Alternatively invoke cmake directly:

```bash
$ mkdir -p build pkg
$ cmake \
  -DCMAKE_INSTALL_PREFIX=pkg \
  --toolchain <toolchain-path> \
  -S . -B build \
  cmake --build build --config Release --target all -j $(nproc) \
  cmake --build build --config Release --target install
```

### Build using clang-cl

clang-cl is the preferred compiler for release packages, as there's no additional runtime DLL dependencies aside from Windows's CRT. You still need MSVC SDK, which will be downloaded when building the `clang-cl-msvc.Dockerfile` image. Once downloaded, modify the toolchain file at `toolchains/clang-cl-msvc.cmake` to point to correct SDK paths.

Also get the static zlib library, and adjust `ZLIB_LIBRARY` in the toolchain file accordingly. On Linux systems the library might be present if MinGW cross compilation toolchain has been installed.

Execute build with:

```bash
$ export CMAKE_TOOLCHAIN_FILE=toolchains/clang-cl-msvc.cmake
$ make build
```

the build and install directories will be performed to `cbuild/<toolchain-name>-$CMAKE_RELEASE_TYPE`, under the names `build` and `pkg` respectively.

### Build using MinGW

The instructions are identical to `clang-cl`, consult the reference toolchain file at `mingw-w64-i686.cmake`.

```bash
$ export CMAKE_TOOLCHAIN_FILE=toolchains/mingw-w64-i686.cmake
$ make build
```

### Cross-compilation on Linux and protobuf compiler

The protobuf compiler is obtained as part of the build process as Windows executable, which will only work on Linux if wine is installed. Alternatively you can grab a pre-built native binary from https://github.com/protocolbuffers/protobuf/tags or build one by yourself and adjust `PROTOC_PATH` in your toolchain file, **provided your external protoc binary matches the version used by the library**.

### Build core library natively

The core component of the library isn't bound to YRpp or Windows and can be built natively with `BUILD_MAIN_LIBRARY=OFF` cmake option. Specify additional compile/link options for test executables in `EXTRA_FLAGS` variable. For example, the following enables ASan and UBSan for GCC:

```cmake
set(EXTRA_FLAGS -fsanitize=address -fsanitize=undefined)
```

### Running tests

It's recommended to run tests using docker. Execute regular tests with:

```bash
$ make docker_test
```

For integration tests, set the path for `gamedata` volume in `docker-compose.yml` to RA2/CnCNet directory.

```yaml
volumes:
  gamedata:
    driver: local
    driver_opts:
      type: none
      device: PATH_TO_CNCNET_FILES
      o: bind
```

And run the test. VNC view will be available at (http://127.0.0.1:6081/vnc.html?autoconnect=1&reconnect=1)

```bash
$ make test_integration
```

## Usage

Copy `libra2yrcpp.dll` and patched `gamemd-spawn.exe` to the CnCNet installation folder (overwriting the original `gamemd-spawn.exe`), or ensure some other way that LoadLibrary can locate the DLL by it's base name.

This spawns a TCP server bound to port 14520 and WebSocket proxy to it on port 14525. To override these, set the environment variables `RA2YRCPP_PORT` and `RA2YRCPP_WS_PORT`.

### Recording game data

By default, a callback is created to save game state at the beginning of each frame. The states are stored as consecutive serialized protobuf messages into a file with name `record.<timestamp>.pb.gz`. After exiting the game, the recording can be dumped as lines of JSON strings with the tool `recordtool.exe`. **WARNING**: the uncompressed recording can get very large. Consider downsampling or transforming it into less verbose format for further processing.

## Acknowledgements

- [Phobos](https://github.com/Phobos-developers/Phobos) and YRpp contributors

## Legal and license

MIT License

Copyright (c) 2022 shmocz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

This project has no direct association to Electronic Arts Inc. Command and Conquer, Yuri's Revenge, Red Alert, Westwood Studios, EA GAMES, the EA GAMES logo and Electronic Arts are trademarks or registered trademarks of Electronic Arts Inc. in the U.S. and/or other countries. All rights reserved.
