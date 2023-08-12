#!/usr/bin/bash

set -o nounset
set -e

TC_ID="$(basename "$CMAKE_TOOLCHAIN_FILE" .cmake)"
BASE_DIR="$BUILDDIR/$TC_ID-$CMAKE_BUILD_TYPE"
BUILD_DIR="$BASE_DIR/build"
build_log="$BASE_DIR/build.log"
: ${TAG_NAME:=""}

if [ ! -z "$TAG_NAME" ]; then
	TAG_NAME="ok-$(git rev-parse --short HEAD)"
	echo "using tag $TAG_NAME"
fi

# Check formatting and run linter
make check

# Run cppcheck
make cppcheck

# Build
set -o pipefail
mkdir -p "$BASE_DIR"
make build_cpp 2>&1 | tee "$build_log"
set +o pipefail

# Check build logs
./scripts/check-build.sh "$build_log"

# Execute tests
make test

# Execute integration tests (if applicable)

# Prepare package
rp="$(realpath "$BASE_DIR")"
# Remove any old package and MinGW DLLs because we cant distribute them
rm -f "$rp/ra2yrcpp.zip" "$BASE_DIR/pkg/bin/"{libgcc_s_dw2-1.dll,libstdc++-6.dll,libwinpthread-1.dll}
cd "$BASE_DIR/pkg/bin" && 7z a "$rp/ra2yrcpp.zip" . && cd -

# Create OK status tag if needed
if [ ! -z "$TAG_NAME" ]; then
	git tag "$TAG_NAME"
fi
