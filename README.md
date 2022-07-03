# ra2yrcpp

Library for providing core functionality for interaction with Red Alert 2 Yuri's Revenge game process. Inspired by [s2client-api](https://github.com/Blizzard/s2client-api).

## Install

```
$ mkdir build
$ make build
```

## Install (Docker)

For convenience, a Docker image is provided with all necessary dependencies to build the application.

To build the Docker image:
```
$ make docker
```

To build the program:
```
$ make docker_build
```

## Tests

Invoke:
```
$ make test
```
