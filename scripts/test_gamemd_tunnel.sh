#!/bin/bash

set -o nounset

export UID
export GID=$(id -g)

function compose_cmd() {
	docker-compose -f docker-compose.yml -f docker-compose.integration.yml "$@"
}

# Start containers. Each container will create and initialize their respective wine environments.
function begin_docker() {
	end_docker
	COMMAND='sh -c "WINEPREFIX=${HOME}/project/${BUILDDIR}/test_instances/${PLAYER_ID}/.wine ./scripts/run_gamemd.sh"' compose_cmd up -d tunnel wm vnc novnc game-0 game-1
}

function end_docker() {
	compose_cmd down --remove-orphans
}

function end() {
	end_docker
}

function errhandle() {
	end
	exit 1
}

trap "errhandle" ERR INT TERM

begin_docker
COMMAND_CONTROLLER='./scripts/test_sell_mcv.sh' compose_cmd up controller
end_docker
