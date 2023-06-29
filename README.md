# ra2yrcpp

Library for interacting with Red Alert 2 Yuri's Revenge game process with protobuf based protocol over TCP. Inspired by [s2client-api](https://github.com/Blizzard/s2client-api).

## Usage

Copy `libra2yrcpp.dll` and patched `gamemd-spawn.exe` to the CnCNet installation folder (overwriting the original `gamemd-spawn.exe`), or ensure some other way that LoadLibrary can locate the DLL by it's base name.

This spawns a TCP server bound to port 14521 and WebSocket proxy to it on port 14525. To override these, set the environment variables `RA2YRCPP_PORT` and `RA2YRCPP_WS_PORT`.

### Recording game data

A callback is created to save game state at the beginning of each frame. To output these to a file, set the environment variable `RA2YRCPP_RECORD_PATH=<name>.pb.gz`. The states are stored as compressed consecutive serialized protobuf messages. After exiting the game, the recording can be dumped as lines of JSON strings with the tool `ra2yrcppcli.exe`. **WARNING**: the uncompressed recording can get very large. Consider downsampling or transforming it into less verbose format for further processing.

### Recording traffic

Set the environment variable `RA2YRCPP_RECORD_TRAFFIC=<name>.pb.gz`

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
git clone --recurse-submodules https://github.com/CnCNet/ra2yrcpp.git
cd ra2yrcpp
cp <CNCNET_FOLDER>/gamemd-spawn.exe .
```

For `clang-cl`, zlib sources might be needed:

```bash
git clone -b v1.2.8 https://github.com/madler/zlib.git 3rdparty/zlib
```

### Build with Docker (recommended)

For convenience, a Docker image is provided for both MinGW and clang-cl toolchains with all necessary dependencies to build the application and related components. MinGW toolchain is used by default.

```bash
make docker_build
```

### General build instructions

#### Obtain protobuf

> **Warning**
> Builds of libprotobuf lack compatibility across different compilers. Attempting to link a MinGW compiled library in MSVC/clang-cl toolchain, or vice versa, will result in errors.

##### Option 1

Copy files from docker image. If you built the main docker image then protobuf has already been built and you can copy the files from there:

```bash
mkdir -p opt/usr
docker-compose cp -L builder:/usr/i686-w64-mingw32 opt/usr
docker-compose cp -L builder:/usr/bin/protoc opt/bin
```

##### Option 2

Build protobuf using a docker container. Use either the "builder" or "clang-cl" docker image:

```bash
cfg=("clang-cl-msvc" "clang-cl")
# uncomment to use MinGW
# cfg=("mingw-w64-i686" "builder")
export CMAKE_TOOLCHAIN_FILE=toolchains/${cfg[0]}.cmake
export BUILDDIR="build-protobuf"
docker-compose run -e BUILDDIR -e CMAKE_TOOLCHAIN_FILE --rm -it "${cfg[1]}" make build_protobuf
opt="$(realpath -s opt)"
cd "$BUILDDIR/${cfg[0]}-Release/pkg"
find . -type f -exec install -D "{}" "$opt/{}" \;
```

##### Install iced-x86

iced-x86 Python library is used to disassemble instructions from game binary to automatically infer the number of bytes to copy when creating hooks. Make sure Python 3 is installed, then invoke:

```bash
pip install --user iced-x86
```

Pick a toolchain of your choice, release type and run make:

```bash
export CMAKE_TOOLCHAIN_FILE=<toolchain-path>
export CMAKE_RELEASE_TYPE=Release
make build
```

This performs the build and installation under `cbuild/<toolchain-id>-<release-type>`.

Alternatively invoke cmake directly:

```bash
mkdir -p build pkg
cmake \
  -DCMAKE_INSTALL_PREFIX=pkg \
  --toolchain <toolchain-path> \
  -S . -B build \
  cmake --build build --config Release --target all -j $(nproc) \
  cmake --build build --config Release --target install
```

The following build options are available:

- `RA2YRCPP_BUILD_MAIN_DLL` whether to build the main YRpp-dependent DLL and related utilities. Default: `ON`
- `RA2YRCPP_BUILD_TESTS` whether to build test executables. Default: `ON`
- `RA2YRCPP_DEBUG_LOG` enable debug logging even for non-debug targets. Default: `OFF`

### Build using clang-cl

Clang-cl requires MSVC SDK, which will be downloaded when building the `clang-cl-msvc.Dockerfile` image. Once downloaded, modify the toolchain file at `toolchains/clang-cl-msvc.cmake` to point to correct SDK paths.

Also get the static zlib library, and adjust `ZLIB_LIBRARY` in the toolchain file accordingly. On Linux systems the library might be present if MinGW cross compilation toolchain has been installed.

Execute build with:

```bash
export CMAKE_TOOLCHAIN_FILE=toolchains/clang-cl-msvc.cmake
make build
```

the build and install directories will be performed to `cbuild/<toolchain-name>-$CMAKE_RELEASE_TYPE`, under the names `build` and `pkg` respectively.

### Build using MinGW

The instructions are identical to `clang-cl`, consult the reference toolchain file at `mingw-w64-i686.cmake`.

```bash
export CMAKE_TOOLCHAIN_FILE=toolchains/mingw-w64-i686.cmake
make build
```

### Build core library natively

The core component and its related tests do not depend on Windows, and can be built natively using `RA2YRCPP_BUILD_MAIN_DLL=OFF` CMake option. You can specify additional compile/link options for test executables in the `RA2YRCPP_EXTRA_FLAGS` variable. For instance, to enable ASan and UBSan on GCC or Clang:

```cmake
set(RA2YRCPP_EXTRA_FLAGS -fsanitize=address -fsanitize=undefined)
```

### Running tests

It's recommended to run tests using docker. Execute regular tests with:

```bash
make docker_test
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
make test_integration
```

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
