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

Copy `libyrclient.dll` to the CnCNet installation folder (containing `gamemd-spawn.exe`), or ensure some other way that LoadLibrary can locate the DLL by it's base name. Run the main tool:

```bash
$ ra2yrcppcli.exe
```

the tool waits for `gamemd-spawn.exe` process to appear, then injects and initializes the main DLL into game process. It's also possible (though not recommended) to do the initialization into an already active game. The above is essentially a shorthand for:

```
$ ra2yrcppcli.exe -g 0
$ ra2yrcppcli.exe -n CreateHooks
$ ra2yrcppcli.exe -n CreateCallbacks
```

This spawns a TCP server inside gamemd process bound to port 14520 by default.

### Recording game data

By default, a callback is created to save game state at the beginning of each frame. The states are stored as consecutive serialized protobuf messages into a file with name `record.<timestamp>.pb.gz`. After exiting the game, the recording can be dumped as lines of JSON strings with the tool `recordtool.exe`. **WARNING**: the uncompressed recording can get very large. Consider downsampling or transforming it into less verbose format for further processing.

## Acknowledgements

[Phobos](https://github.com/Phobos-developers/Phobos) project for providing resources and general help about the game internals.

## Legal

This project has no direct association to Electronic Arts Inc. Command and Conquer, Yuri's Revenge, Red Alert, Westwood Studios, EA GAMES, the EA GAMES logo and Electronic Arts are trademarks or registered trademarks of Electronic Arts Inc. in the U.S. and/or other countries. All rights reserved.
