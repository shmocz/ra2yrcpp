#!/usr/bin/bash

set -o nounset
set -e
: ${BUILDDIR="cbuild"}
: ${BUILDER:="builder"}
: ${CMAKE_BUILD_TYPE:="Release"}
: ${CMAKE_EXTRA_ARGS=""}
: ${CMAKE_TARGET:="all"}
: ${CMAKE_TOOLCHAIN_FILE="toolchains/mingw-w64-i686.cmake"}
: ${CXXFLAGS:="-Wall -Wextra"}
: ${NPROC:=$(nproc)}
: ${TAG_NAME:=""}

REPO_FILES="$(git ls-tree -r --name-only HEAD)"
CM_FILES="$(echo "$REPO_FILES" | grep 'CMakeLists.txt')"
CPP_SOURCES="$(echo "$REPO_FILES" | grep -P '\.(cpp|hpp|c|h)$')"
TC_ID="$(basename "$CMAKE_TOOLCHAIN_FILE" .cmake)"
BASE_DIR="$BUILDDIR/$TC_ID-$CMAKE_BUILD_TYPE"
BUILD_DIR="$BASE_DIR/build"
VERSION="SOFT_VERSION-$(git rev-parse --short HEAD)"
set +e

export CXXFLAGS NPROC CMAKE_TOOLCHAIN_FILE BUILDDIR TAG_NAME CMAKE_BUILD_TYPE CMAKE_TARGET CMAKE_EXTRA_ARGS

function lint() {
    cpplint \
        --recursive \
        --exclude=src/utility/scope_guard.hpp \
        --filter=-build/include_order,-build/include_subdir,-build/c++11,-legal/copyright,-build/namespaces,-readability/todo,-runtime/int,-runtime/string,-runtime/printf \
        src/ tests/
}

function lintfix() {
    I="$(lint 2>&1)"

    # Fix "already" included
    echo "$I" | grep -Po '^\K[^\s]+(?=:\s+.+\[build/include\].*)' |
        tr ':' '\t' | while read fname line; do
        sed -i "${line}d" $fname
    done

    # Fix c-style cast
    echo "$I" | grep -P '\[readability/casting\].*' | while read line; do
        # get fname, pattern and sub
        echo $line | perl -p -e 's/([^:]+):(\d+):.+\s+(\w+)_cast<(.+)>.+$/$1 $2 $4 $3/g'
    done | while read fname line patt cast_t; do
        patt="$(echo "$patt" | sed 's/\*/\\*/g')"
        S='s/(.+)\(('"$patt"')\)([^;,]+)(.+)/$1'"$cast_t"'_cast<$2>($3)$4/g if $. == '"$line"
        perl -i -pe "$S" "$fname"
    done

    # Single param ctors
    echo "$I" | grep -P '\[runtime/explicit\].*' | while read line; do
        # get fname and lineno
        echo $line | perl -p -e 's/([^:]+):(\d+):.+$/$1 $2/g'
    done | while read fname line; do
        S='s/(\s*)(\w+)(\(.+)/$1explicit $2$3/g if $. == '"$line"
        perl -i -pe "$S" "$fname"
    done

    # No newline
    echo "$I" | grep -P '\[whitespace/ending_newline\].*' | while read line; do
        # get fname and lineno
        echo $line | perl -p -e 's/([^:]+):(\d+):.+$/$1 $2/g'
    done | while read fname line; do
        S='s/(\s*)(\w+)(\(.+)/$1explicit $2$3/g if $. == '"$line"
        G="$(cat "$fname")"
        printf "%s\n" "$G" >"$fname"
    done
}

function cmake-config() {
    mkdir -p "$1"
    CMAKE_TOOLCHAIN_FILE="$(realpath "$CMAKE_TOOLCHAIN_FILE")" \
        cmake \
        -DCMAKE_INSTALL_PREFIX="$BASE_DIR/pkg" \
        -DRA2YRCPP_VERSION=$VERSION \
        $CMAKE_EXTRA_ARGS \
        -G "Unix Makefiles" \
        -S . -B "$1"
}

function build-docs() {
    doxygen Doxyfile
}

function build-cpp() {
    # TODO: if using custom targets on mingw, the copied libs arent marked as deps!
    if [ ! -d "$BUILD_DIR" ]; then
        cmake-config "$BUILD_DIR"
    fi
    cmake --build $BUILD_DIR --config $CMAKE_BUILD_TYPE --target $CMAKE_TARGET -j $NPROC
    cmake --build $BUILD_DIR --config $CMAKE_BUILD_TYPE --target install/fast
}

function build-protobuf() {
    build_dir="$1"
    mkdir -p "$build_dir"
    CMAKE_TOOLCHAIN_FILE="$(realpath "$CMAKE_TOOLCHAIN_FILE")" \
        cmake \
        -DCMAKE_INSTALL_PREFIX="$DEST_DIR" \
        "$CMAKE_EXTRA_ARGS" \
        -G "Unix Makefiles" \
        -Dprotobuf_BUILD_LIBPROTOC=ON \
        -Dprotobuf_WITH_ZLIB=ON \
        -DProtobuf_USE_STATIC_LIBS=ON \
        -Dprotobuf_MSVC_STATIC_RUNTIME=OFF \
        -Dprotobuf_BUILD_EXAMPLES=OFF \
        -Dprotobuf_INSTALL=ON \
        -Dprotobuf_BUILD_TESTS=OFF \
        -DZLIB_LIB=/usr/i686-w64-mingw32/lib \
        -DZLIB_INCLUDE_DIR=/usr/i686-w64-mingw32/include \
        -S "3rdparty/protobuf" -B "$BUILD_DIR"
    cmake --build "$BUILD_DIR" -j $NPROC
    cmake --build "$BUILD_DIR" --config "$CMAKE_BUILD_TYPE" --target install
}

function check-build() {
    [ ! -f "$1" ] && exit 1

    e="$(grep -P 'warning' "$1" | grep -Pv '(Wunknown-pragmas|Wcomment|#pragma\s+warning|proto|xbyak\.h)')"

    [ -n "$e" ] && {
        echo "build check failed:"
        echo "$e"
        exit 1
    }
}

function check-other() {
    {
        git diff $CM_FILES
        git diff $CPP_SOURCES
    } >err.log

    e="$(cat err.log)"

    [ -n "$e" ] && {
        echo "$e"
        exit 1
    }
}

function cmakefiles-format() {
    cmake-format -c .cmake-format.yml -i $CM_FILES
}

function cpp-format() {
    echo "$CPP_SOURCES" | xargs -n 1 clang-format -i
}

function cpp-check() {
    out="cppcheck.log"

    # example override: cppcheck="docker run --user $UID:$UID --rm -v "$(pwd)":/mnt -w /mnt shmocz/cppcheck-action cppcheck"
    : ${CPPCHECK:="cppcheck"}
    : ${CPPCHECK_BUILD_DIR:="cppcheck-work"}

    mkdir -p "$CPPCHECK_BUILD_DIR"

    $CPPCHECK -q --platform=win32W \
        --cppcheck-build-dir="$CPPCHECK_BUILD_DIR" \
        --enable=warning,style,performance,portability,unusedFunction \
        -I src/ \
        --template='{file}:{line},{severity},{id},{message}' \
        --inline-suppr \
        --suppress=passedByValue \
        --suppress=noExplicitConstructor:src/utility/scope_guard.hpp \
        --suppress=unusedStructMember:src/commands_yr.cpp \
        --suppress=unusedFunction:tests/*.cpp \
        --suppress=uninitMemberVar:src/ra2/objects.cpp \
        --suppress=shadowVar:src/commands_yr.cpp \
        --suppress=constParameter:src/utility/sync.hpp \
        src/ tests/ 2>"$out"

    status=$?

    [ $status -ne 0 ] || [ "$(cat "$out" | wc -l)" != 0 ] && { cat "$out" && exit 1; }
}

function run-tests() {
    tests="$(find tests/ -iname 'test_*.cpp')"
    for t in $tests; do
        wineboot -s
        p="${t/tests/bin}"
        p="${p/.cpp/.exe}"
        wine "$BASE_DIR"/pkg/$p
    done
}

function create-release() {
    build_log="$BASE_DIR/build.log"
    export CPPCHECK_BUILD_DIR="$BASE_DIR/cppcheck-work"

    if [ ! -z "$TAG_NAME" ]; then
        TAG_NAME="ok-$(git rev-parse --short HEAD)"
        echo "using tag $TAG_NAME"
    fi

    # Check formatting
    cmakefiles-format
    cpp-format
    check-other

    set -e
    lint
    set +e
    cpp-check

    # Build
    set -o pipefail
    set -e
    mkdir -p "$BASE_DIR"
    build-cpp 2>&1 | tee "$build_log"
    set +e
    set +o pipefail

    # Check build logs
    check-build "$build_log"

    # Execute tests
    run-tests

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
}

function compose-cmd() {
    docker-compose run \
        -u $UID:$UID \
        --rm \
        -e BUILDDIR \
        -e CMAKE_BUILD_TYPE \
        -e CMAKE_EXTRA_ARGS \
        -e CMAKE_TARGET \
        -e CMAKE_TOOLCHAIN_FILE \
        -e CXXFLAGS \
        -e NPROC \
        -e TAG_NAME \
        $BUILDER \
        "$@"
}

function docker-release() {
    compose-cmd ./scripts/tools.sh create-release
}

function docker-build() {
    compose-cmd ./scripts/tools.sh build-cpp
}

$1
