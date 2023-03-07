#!/usr/bin/bash

# Example debugging script targeting container "game-0" of a running integration test.
# Generally winedbg is better suited for windows application debugging, but with docker
# seems to give weird crashes with watchpoints. Hence GDB is used.
#
PLAYER_ID="0"
HOMEDIR="/home/user/project"
: ${BUILDDIR:="cbuild_docker"}

function dcmd_generic() {
	: ${user:="root"}
	docker-compose -f docker-compose.yml exec --user "$user" \
		-w "$HOMEDIR"/build_docker \
		builder bash -c "$1"
}

function dcmd_integration() {
	: ${user:="root"}
	cmd="$(printf 'WINEPREFIX=${HOME}/project/%s/test_instances/${PLAYER_ID}/.wine %s' "$BUILDDIR" "$1")"
	docker-compose -f docker-compose.yml -f docker-compose.integration.yml exec --user "$user" \
		-w "$HOMEDIR"/$BUILDDIR/test_instances/player_${PLAYER_ID} \
		game-$PLAYER_ID bash -c "$cmd"
}

GDB_COMMAND='x86_64-w64-mingw32-gdb -ex "target extended-remote localhost:12340" -ex "set pagination off" -ex "set logging overwrite on" -ex "set logging on" -ex "set disassembly-flavor intel" -ex c'

function gdb_connect() {
	dcmd_integration "$(printf '%s %s' "$GDB_COMMAND" "$1")"
}

function debug_integration_test() {
	a="$(printf '%s "source "%s""' "-ex" "$HOMEDIR/scripts/debug.gdb")"
	gdb_connect "$a"
}

# CARGS="-d"
# non-debug option
CARGS="--abort-on-container-exit"
TOOLCHAIN="mingw-w64-i686-docker-Debug"
# TOOLCHAIN="clang-cl-msvc-Debug"
# BUILDER="clang-cl"
BUILDER="builder"
EXE="$HOMEDIR/$BUILDDIR/$TOOLCHAIN/pkg/bin/test_is_stress_test.exe"

function debug_integration() {
	WINE_CMD="wine" $COMPOSE_CMD up --abort-on-container-exit $BUILDER
}

function debug_testcase() {
	WINE_CMD="wine"
	if [ "$CARGS" == "-d" ]; then
		WINE_CMD="wine Z:/usr/share/win32/gdbserver.exe localhost:12340"
	fi
	export UID=$(id -u)
	export GID=$(id -g)
	docker-compose down --remove-orphans -t 1
	# FIXME: put abort stuff to cargs
	COMMAND="$WINE_CMD $EXE --gtest_filter='*'" docker-compose up $CARGS vnc "$BUILDER"
	if [[ "$CARGS" == "-d" ]]; then
		sleep 2
		P='x86_64-w64-mingw32-gdb '"$EXE"' -ex "set solib-search-path "'"$HOMEDIR/$BUILDDIR/$TOOLCHAIN"'/pkg/bin"" -ex "target extended-remote localhost:12340"'
		dcmd_generic "$P"
	fi
}

# x86_64-w64-mingw32-gdb myprogram.exe
#     (gdb) set solib-search-path ...directories with the DLLs used by the program...
#     (gdb) target extended-remote localhost:12345

debug_integration_test

# function dcmd_gen() {
# 	: ${user:="user"}
# 	docker-compose down --remove-orphans
# 	COMMAND="$@" docker-compose -f docker-compose.yml up --abort-on-container-exit vnc builder
# }
#
# debug_testcase
# debug_integration

# gdb_connect
# dcmd_gen "$@"

# COMMAND="sh -c 'BUILDDIR=$(BUILDDIR) make BUILDDIR=$(BUILDDIR) DEST_DIR=$(DEST_DIR) TESTS=$$f test'" docker-compose up --abort-on-container-exit vnc builder; done
