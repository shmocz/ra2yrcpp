#!/usr/bin/bash

# Example debugging script targeting container "game-0" of a running integration test.
# Generally winedbg is better suited for windows application debugging, but with docker
# seems to give weird crashes with watchpoints. Hence GDB is used.
#
PLAYER_ID="0"

function dcmd_generic() {
	: ${user:="root"}
	docker-compose -f docker-compose.yml exec --user "$user" \
		-w /home/user/project/build_docker \
		builder bash -c "$1"
}

function dcmd_integration() {
	: ${user:="root"}
	cmd="$(printf 'WINEPREFIX=${HOME}/project/${BUILDDIR}/test_instances/${PLAYER_ID}/.wine %s' "$1")"
	docker-compose -f docker-compose.yml -f docker-compose.integration.yml exec --user "$user" \
		-w /home/user/project/build_docker/test_instances/player_${PLAYER_ID} \
		game-$PLAYER_ID bash -c "$cmd"
}

GDB_COMMAND='x86_64-w64-mingw32-gdb -ex "target extended-remote localhost:12340" -ex "set pagination off" -ex "set logging overwrite on" -ex "set logging on" -ex "set disassembly-flavor intel"'

function gdb_connect() {
	dcmd_integration "$(printf '%s %s' "$GDB_COMMAND" "$1")"
}

function debug_integration_test() {
	gdb_connect '-ex "source /home/user/project/scripts/debug.gdb"'
}

function debug_testcase() {
	WINE_CMD="wine Z:/usr/share/win32/gdbserver.exe localhost:12340"
	export UID=$(id -u)
	export GID=$(id -g)
	COMMAND="$WINE_CMD build_docker/bin/test_dll_inject.exe --gtest_filter='*BasicLoad*'" docker-compose up --abort-on-container-exit vnc builder
	# In a separate terminal, connect to gdb instance using the command:
	# dcmd_generic 'x86_64-w64-mingw32-gdb -ex "target extended-remote localhost:12340"'
}

# x86_64-w64-mingw32-gdb myprogram.exe
#     (gdb) set solib-search-path ...directories with the DLLs used by the program...
#     (gdb) target extended-remote localhost:12345

debug_integration_test
