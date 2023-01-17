# ra2yrcpp

Library for interacting with Red Alert 2 Yuri's Revenge game process with protobuf based protocol over TCP. Inspired by [s2client-api](https://github.com/Blizzard/s2client-api).

## Building

Get the sources and submodules and place `gamemd-spawn.exe` from CnCNet distribution into the project source directory.

```bash
$ git clone --recurse-submodules https://github.com/CnCNet/ra2yrcpp.git
$ cd ra2yrcpp
$ cp CNCNET_FOLDER/gamemd-spawn.exe .
```

### Build with Docker (recommended)

For convenience, a Docker image is provided with all necessary dependencies to build the application and related components.

```bash
$ make BUILDDIR=build_docker docker_build
```

### Regular build

Set up a suitable toolchain for building WIN32 executables. Execute:

```bash
$ make build
```

This performs the build under `build` folder with `Release` profile. Adjust the settings to your likings, e.g.:

```bash
$ make BUILDDIR=build_debug CMAKE_BUILD_TYPE=Debug build
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

Copy `libyrclient.dll` and patched `gamemd-spawn.exe` to the CnCNet installation folder (overwriting the original `gamemd-spawn.exe`), or ensure some other way that LoadLibrary can locate the DLL by it's base name.

This spawns a TCP server inside gamemd process bound to port 14520 by default. To override this, set environment variable `RA2YRCPP_PORT`.

### Recording game data

By default, a callback is created to save game state at the beginning of each frame. The states are stored as consecutive serialized protobuf messages into a file with name `record.<timestamp>.pb.gz`. After exiting the game, the recording can be dumped as lines of JSON strings with the tool `recordtool.exe`. **WARNING**: the uncompressed recording can get very large. Consider downsampling or transforming it into less verbose format for further processing.

## Acknowledgements

[Phobos](https://github.com/Phobos-developers/Phobos) project for providing resources and general help about the game internals.

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
