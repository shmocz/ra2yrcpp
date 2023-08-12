# ra2yrcpp

Library for interacting with Red Alert 2 Yuri's Revenge game process with protobuf based protocol over TCP. Inspired by [s2client-api](https://github.com/Blizzard/s2client-api).

## Usage

Copy `libra2yrcpp.dll` and patched `gamemd-spawn.exe` to the CnCNet installation folder (overwriting the original `gamemd-spawn.exe`), or ensure some other way that LoadLibrary can locate the DLL by it's base name. When the game is started, the DLL will be loaded and a with WebSocket server bound to port 14521.

The following environment variables control the behaviour of the service:

- `RA2YRCPP_ALLOWED_HOSTS_REGEX`: Regex matching the hosts allowed to connect (default: "0.0.0.0|127.0.0.1")
- `RA2YRCPP_PORT`: The server port (default: 14521)
- `RA2YRCPP_RECORD_PATH`: Path to state record file (disabled by default)
- `RA2YRCPP_RECORD_TRAFFIC`: Path to traffic record file (disabled by default)

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
git clone --recurse-submodules https://github.com/shmocz/ra2yrcpp.git
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

- `RA2YRCPP_BUILD_MAIN_DLL` Whether to build the main YRpp-dependent DLL and related utilities. Default: `ON`
- `RA2YRCPP_BUILD_TESTS` Whether to build test executables. Default: `ON`
- `RA2YRCPP_DEBUG_LOG` Enable debug logging even for non-debug targets. Default: `OFF`
- `RA2YRCPP_SYSTEM_PROTOBUF` Use system protobuf headers instead of the submodule. Useful when working on native builds. Default: `OFF`

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

### Build core library natively without YRpp

The core component and it's related tests do not depend on Windows, and can be built natively using `RA2YRCPP_BUILD_MAIN_DLL=OFF` CMake option. This is useful when debugging code of the main service. You can specify additional compile/link options for test executables in the `RA2YRCPP_EXTRA_FLAGS` variable. For instance, to enable ASan and UBSan on GCC or Clang:

```cmake
set(RA2YRCPP_EXTRA_FLAGS -fsanitize=address -fsanitize=undefined)
```

### Using different protobuf version

> **Warning**
> Currently supported only for native builds (`RA2YRCPP_BUILD_MAIN_DLL=OFF`)

Newer versions of protobuf use abseil library and may require additional linking flags. These can be specified in the toolchain file like this:

```cmake
set(PROTOBUF_EXTRA_LIBS absl_status absl_log_internal_check_op absl_log_internal_message)
```

Exact list of libraries may vary across systems and protobuf versions.

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

## Troubleshooting

### The game doesn't start

This can happen if ra2yrcpp cannot load zlib DLL. Ensure that `zlib1.dll` is placed in the same folder as `gamemd-spawn.exe`.

## Credits

- **shmocz**
- **[CnCNet](https://github.com/CnCNet) Contributors**
- **[Phobos](https://github.com/Phobos-developers/Phobos) and YRpp contributors**

## Legal and license

[![GPL v3](https://www.gnu.org/graphics/gplv3-127x51.png)](https://opensource.org/licenses/GPL-3.0)

This project has no direct association to Electronic Arts Inc. Command and Conquer, Yuri's Revenge, Red Alert, Westwood Studios, EA GAMES, the EA GAMES logo and Electronic Arts are trademarks or registered trademarks of Electronic Arts Inc. in the U.S. and/or other countries. All rights reserved.
