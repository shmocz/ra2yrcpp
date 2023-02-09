#!/usr/bin/bash
# Helper script to build and test various build setups.

export BUILDDIR=cbuild_docker

for type in Release; do
    echo clang-cl-msvc "$type" "clang-cl"
    echo mingw-w64-i686-docker "$type" "builder"
done | while read tc tt cont; do
    export CMAKE_TOOLCHAIN_FILE="toolchains/$tc.cmake"
    export CMAKE_BUILD_TYPE="$tt"
    export BUILDER="$cont"
    export NPROC=8
    set -e
    make docker_build </dev/null
    make docker_test </dev/null
    make test_integration </dev/null
    set +e
done
