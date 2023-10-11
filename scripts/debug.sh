#!/usr/bin/bash

set -o nounset

PLAYER_ID="0"
HOMEDIR="/home/user/project"
: ${BUILDDIR:="cbuild_docker"}
TOOLCHAIN="$(echo $CMAKE_TOOLCHAIN_FILE | sed -E 's/.+\/(.+)\.cmake/\1/g')-${CMAKE_BUILD_TYPE}"
: ${TARGET:="localhost:12340"}
: ${INTEGRATION_TEST_TARGET:="./pyra2yr/test_sell_mcv.py"}

# Executable to be passed to wine and it's args, example:
# EXE="$HOMEDIR/$BUILDDIR/$TOOLCHAIN/pkg/bin/test_dll_inject.exe --gtest_repeat=-1 --gtest_filter=*IServiceDLL*"
: ${EXE:="$HOMEDIR/$BUILDDIR/$TOOLCHAIN/pkg/bin/test_dll_inject.exe --gtest_repeat=-1 --gtest_filter=*IServiceDLL*"}
GDB_COMMAND='gdb'
: ${GDB_SCRIPT:="$HOMEDIR/scripts/debug.gdb"}

function dcmd_generic() {
	: ${user:="root"}
	docker-compose -f docker-compose.yml exec --user "$user" \
		-w "$HOMEDIR"/build_docker \
		$BUILDER bash -c "$1"
}

function dcmd_integration() {
	: ${user:="root"}
	: ${it=""}
	docker exec $it --user "$user" -w "$HOMEDIR"/$BUILDDIR/test_instances/player_${PLAYER_ID} \
		game-0-$PLAYER_ID bash -c "$1"
}

function gdb_connect() {
	it="-it" dcmd_integration "$(printf '%s %s' "$GDB_COMMAND" "$1")"
}

function debug_integration_test() {
	# get target PID
	game_pid="$(dcmd_integration "pgrep -f gamemd-spawn-ra2yrcpp.exe")"
	a="$(printf -- '-p %s %s "source "%s""' "$game_pid" "-ex" "$GDB_SCRIPT")"

	# attach (and load symbols)
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
	python ./scripts/run-gamemd.py \
		-b $BUILDDIR/test_instances \
		-B $BUILDDIR/mingw-w64-i686-Release/pkg/bin \
		-S maingame/gamemd-spawn-n.exe \
		run-docker-instance \
		-e pyra2yr/test_sell_mcv.py &
	sleep 5
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
