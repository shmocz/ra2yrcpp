#!/usr/bin/bash

# Example debugging script targeting container "game-0" of a running integration test.
# Generally winedbg is better suited for windows application debugging, but with docker
# seems to give weird crashes with watchpoints. Hence GDB is used.
#
PLAYER_ID="0"

function dcmd() {
	: ${user:="root"}
	cmd="$(printf 'WINEPREFIX=${HOME}/project/${BUILDDIR}/test_instances/${PLAYER_ID}/.wine %s' "$1")"
	docker-compose -f docker-compose.yml -f docker-compose.integration.yml exec --user "$user" \
		-w /home/user/project/build_docker/test_instances/player_${PLAYER_ID} \
		game-$PLAYER_ID bash -c "$cmd"
}

# x86_64-w64-mingw32-gdb myprogram.exe
#     (gdb) set solib-search-path ...directories with the DLLs used by the program...
#     (gdb) target extended-remote localhost:12345

dcmd 'x86_64-w64-mingw32-gdb -ex "target extended-remote localhost:12340" -ex "source /home/user/project/scripts/debug.gdb" -ex "set pagination off" -ex "set logging overwrite on" -ex "set logging on" -ex "c"'
#dcmd 'x86_64-w64-mingw32-gdb -ex "target extended-remote localhost:12340" -ex "c"'
