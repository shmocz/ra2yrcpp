#!/usr/bin/bash

set -o nounset

PLAYER_ID="0"
HOMEDIR="/home/user/project"
: ${BUILDDIR:="cbuild_docker"}
TOOLCHAIN="$(echo $CMAKE_TOOLCHAIN_FILE | sed -E 's/.+\/(.+)\.cmake/\1/g')-${CMAKE_BUILD_TYPE}"
: ${TARGET:="localhost:12340"}

# Executable to be passed to wine and it's args, example:
# EXE="$HOMEDIR/$BUILDDIR/$TOOLCHAIN/pkg/bin/test_dll_inject.exe --gtest_repeat=-1 --gtest_filter=*IServiceDLL*"
: ${EXE:="$HOMEDIR/$BUILDDIR/$TOOLCHAIN/pkg/bin/test_dll_inject.exe --gtest_repeat=-1 --gtest_filter=*IServiceDLL*"}
GDB_COMMAND='x86_64-w64-mingw32-gdb -iex "set pagination off" -ex "target extended-remote '"$TARGET"'" -ex "set pagination off" -ex "set logging overwrite on" -ex "set logging on" -ex "set disassembly-flavor intel"'
: ${GDB_SCRIPT:="$HOMEDIR/scripts/debug.gdb"}

function dcmd_generic() {
	: ${user:="root"}
	docker-compose -f docker-compose.yml exec --user "$user" \
		-w "$HOMEDIR"/build_docker \
		$BUILDER bash -c "$1"
}

function dcmd_integration() {
	: ${user:="root"}
	cmd="$(printf 'WINEPREFIX=${HOME}/project/%s/test_instances/${PLAYER_ID}/.wine %s' "$BUILDDIR" "$1")"
	docker-compose -f docker-compose.yml -f docker-compose.integration.yml exec --user "$user" \
		-w "$HOMEDIR"/$BUILDDIR/test_instances/player_${PLAYER_ID} \
		game-$PLAYER_ID bash -c "$cmd"
}

function gdb_connect() {
	dcmd_integration "$(printf '%s %s' "$GDB_COMMAND" "$1")"
}

function debug_integration_test() {
	a="$(printf '%s "source "%s""' "-ex" "$GDB_SCRIPT")"
	gdb_connect "$a"
}

CARGS="-d"
# non-debug option
# CARGS="--abort-on-container-exit"

function tmux_send_keys() {
	: ${TMUX_TARGET="yr:0.1"}
	tmux send-keys -t "$TMUX_TARGET" "$@"
}

function debug_integration() {
	make INTEGRATION_TEST_TARGET="$INTEGRATION_TEST_TARGET" test_integration &
	sleep 3
	tmux_send_keys C-c
	tmux_send_keys "CMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE CMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE DEBUG_FN=debug_integration_test ./scripts/debug.sh" C-m
	wait
}

function debug_testcase() {
	WINE_CMD="wine"
	if [ "$CARGS" == "-d" ]; then
		WINE_CMD="wine Z:/usr/share/win32/gdbserver.exe $TARGET"
	fi
	export UID=$(id -u)
	export GID=$(id -g)
	docker-compose down --remove-orphans -t 1

	COMMAND="$WINE_CMD $EXE" docker-compose up $CARGS vnc "$BUILDER"
	if [[ "$CARGS" == "-d" ]]; then
		sleep 2
		P='x86_64-w64-mingw32-gdb -ex "set solib-search-path "'"$HOMEDIR/$BUILDDIR/$TOOLCHAIN"'/pkg/bin"" -ex "target extended-remote '"$TARGET"'" --args '"$EXE"
		dcmd_generic "$P"
	fi
}

function docker_run() {
	export UID=$(id -u)
	export GID=$(id -g)
	docker-compose down --remove-orphans -t 1
	docker-compose run --rm -it $BUILDER $EXE
}

# x86_64-w64-mingw32-gdb myprogram.exe
#     (gdb) set solib-search-path ...directories with the DLLs used by the program...
#     (gdb) target extended-remote localhost:12345

# debug_integration_test

$DEBUG_FN

# gdb_connect
# dcmd_gen "$@"

# COMMAND="sh -c 'BUILDDIR=$(BUILDDIR) make BUILDDIR=$(BUILDDIR) DEST_DIR=$(DEST_DIR) TESTS=$$f test'" docker-compose up --abort-on-container-exit vnc builder; done
