#!/bin/bash

./scripts/prep_instance_dirs.sh

set -o nounset
set -e
# Checks
which pycncnettunnel
set +e

type_classes=./.tmp/tc.json
port_base=14521

function get_total_state() {
	printf '{"state": %s, "tc": %s}' "$2" "$(cat "$1")" | jq -r -f test_data/get_objects.jq
}

function r2cli() {
	set -x
	./ra2yrcppcli.exe -p $port_base "$@"
	set +x
}

function r2c() {
	r2cli -n "yrclient.commands.${1}" "${@:2}"
}

function unit_action() {
	printf '{"object_addresses": [%s], "action": "ACTION_%s"}' "$1" "$2"
}

function click_event() {
	printf '{"object_addresses": [%s], "event": "NETWORK_EVENT_%s"}' "$1" "$2"
}

function begin() {
	killall -w pycncnettunnel
	killall -w gamemd-spawn.exe
	# start tunnel
	pycncnettunnel &

	IFS=$'\n' read -d '' -r -a cfgs <"$PLAYERS_CONFIG"
	cfgs=("${cfgs[@]:1}")
	# start main game
	for i in ${!cfgs[@]}; do
		c="${cfgs[$i]}"
		read -r name color side is_host is_observer port <<<$(echo "$c")
		d="$RA2YRCPP_TEST_INSTANCES_DIR/$name"
		cd "$d"
		export WINEPREFIX="$d/.wine"
		./ra2yrcppcli.exe -p $((port_base + i)) &
		wine gamemd-spawn.exe -SPAWN 2>gamemd.err.log &
		cd -
	done
}

function end() {
	killall -w pycncnettunnel
	killall -w gamemd-spawn.exe

	IFS=$'\n' read -d '' -r -a cfgs <"$PLAYERS_CONFIG"
	cfgs=("${cfgs[@]:1}")
	# kill sessions
	for i in ${!cfgs[@]}; do
		c="${cfgs[$i]}"
		read -r name color side is_host is_observer port <<<$(echo "$c")
		DISPLAY=:1 WINEPREFIX="$RA2YRCPP_TEST_INSTANCES_DIR/$name/.wine" wineboot -e
	done
}

function errhandle() {
	end
	exit 1
}

trap "errhandle" ERR INT TERM

function sell_mcv_test() {
	# wait until initialization starts
	while [[ "$(pgrep ra2yrcppcli.exe | wc -l)" == "0" ]]; do
		echo "wait init to start"
		sleep 1s
	done

	# wait until initialization complete
	while [[ "$(pgrep ra2yrcppcli.exe | wc -l)" != "0" ]]; do
		echo "wait init to finish"
		sleep 1s
	done

	# wait until game has started
	while true; do
		x="$(r2c "GetGameState" | jq -r '.body.result.result.state.stage')"
		if [[ "$x" == "STAGE_INGAME" ]]; then
			break
		fi
		echo "wait to start up... stage $x"
		sleep 1s
	done

	r2cli -n "yrclient.commands.GetTypeClasses" >"$type_classes"

	# Get MCV pointer
	p_obj=$(get_total_state \
		"$type_classes" \
		"$(r2c "GetGameState")" |
		jq -r '.[] | select(.player=="player_0" and (select(.name | test("Yard|Vehicle")))) | .pointerSelf')

	if [ -z "$p_obj" ]; then
		echo "zero MCV pointer"
		exit 1
	fi

	# select
	r2c "UnitCommand" -a "$(unit_action "$p_obj" "SELECT")"

	# deploy
	r2c "ClickEvent" -a "$(click_event "$p_obj" "Deploy")"

	# wait until we have conyard
	while true; do
		p_obj=$(get_total_state "$type_classes" "$(r2c "GetGameState")" |
			jq -r '.[] | select(.player=="player_0" and (select(.name | test("Yard")))) | .pointerSelf')
		if [ ! -z "$p_obj" ]; then
			break
		fi
		sleep 1s
	done

	# Get conyard pointer
	p_obj=$(get_total_state "$type_classes" "$(r2c "GetGameState")" |
		jq -r '.[] | select(.player=="player_0" and (select(.name | test("Yard")))) | .pointerSelf')

	# sell
	r2c "ClickEvent" -a "$(click_event "$p_obj" "Sell")"

	# wait until game exits
	# FIXME: STAGE_EXIT_GAME not set in MP setting
	frame0=0
	while true; do
		frame1="$(r2c "GetGameState" | jq -r '.body.result.result.state.currentFrame')"
		if [ $frame1 -eq $frame0 ]; then
			break
		fi
		frame0=$frame1
		sleep 1s
	done
}

begin
sell_mcv_test
end
