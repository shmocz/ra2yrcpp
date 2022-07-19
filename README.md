# ra2yrcpp

Library for providing core functionality for interaction with Red Alert 2 Yuri's Revenge game process using a protobuf defined protocol sent through TCP socket. Inspired by [s2client-api](https://github.com/Blizzard/s2client-api).

## Building 

Set up a suitable toolchain for building WIN32 executables, then run:

```
$ mkdir build
$ make build
```

## Building (Docker)

For convenience, a Docker image is provided with all necessary dependencies to build the application.

To build the Docker image:
```
$ make docker
```

To build the program:
```
$ make docker_build
```

## Setup

The main game executable needs to be patched for the tool to work in multiplayer setting. Locate gamemd-spawn.exe executable (this should be inside CnCNet installation directory) and patch it with the `patch_gamemd.py` script.

```
$ python ./scripts/patch_gamemd.py gamemd-spawn.exe > gamemd-spawn.patched.exe
```

## Usage

Copy `libyrclient.dll` to the same folder as `gamemd-spawn.exe`, or ensure some other way that LoadLibrary can locate the DLL by it's base name. Run the main tool:

```
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

## Tests

Invoke:

```
$ make test
```

## Acknowledgements

[Phobos](https://github.com/Phobos-developers/Phobos) project for providing resources and general help about the game internals.

## Legal

This project has no direct association to Electronic Arts Inc. Command and Conquer, Yuri's Revenge, Red Alert, Westwood Studios, EA GAMES, the EA GAMES logo and Electronic Arts are trademarks or registered trademarks of Electronic Arts Inc. in the U.S. and/or other countries. All rights reserved.
