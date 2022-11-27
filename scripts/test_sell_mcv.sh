#!/bin/bash

set -o nounset

export UID
export GID=$(id -g)

type_classes="./$BUILDDIR/tc.json"
: ${RA2YRCPP_PORT:=14521}

function get_total_state() {
    printf '{"state": %s, "tc": %s}' "$2" "$(cat "$1")" | jq -r -f test_data/get_objects.jq
}

function r2cli() {
    set -x
    ./$BUILDDIR/bin/ra2yrcppcli.exe -p $RA2YRCPP_PORT "$@"
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

# TODO: properly wait for succesful connection
function sell_mcv_test() {
    player="player_0"

    while true; do
        x="$(r2c "GetGameState" | jq -r '.body.result.result.state.stage')"
        if [[ ! -z "$x" ]]; then
            break
        fi
        echo "wait initialization to be ready"
        sleep 1s
    done

    while true; do
        x="$(r2c "GetGameState" | jq -r '.body.result.result.state.stage')"
        if [[ "$x" == "STAGE_INGAME" ]]; then
            break
        fi
        echo "wait game to start up... stage $x"
        sleep 1s
    done

    r2cli -n "yrclient.commands.GetTypeClasses" >"$type_classes"

    # Get MCV pointer
    p_obj=$(get_total_state \
        "$type_classes" \
        "$(r2c "GetGameState")" |
        jq -r '.[] | select(.player=="'$player'" and (select(.name | test("Yard|Vehicle")))) | .pointerSelf')

    if [ -z "$p_obj" ]; then
        echo "zero MCV pointer"
        return
    fi

    # select
    set -e
    r2c "UnitCommand" -a "$(unit_action "$p_obj" "SELECT")"

    # deploy
    r2c "ClickEvent" -a "$(click_event "$p_obj" "Deploy")"
    set +e

    # wait until we have conyard
    while true; do
        p_obj=$(get_total_state "$type_classes" "$(r2c "GetGameState")" |
            jq -r '.[] | select(.player=="'$player'" and (select(.name | test("Yard")))) | .pointerSelf')
        if [ ! -z "$p_obj" ]; then
            break
        fi
        sleep 1s
    done

    # Get conyard pointer
    p_obj=$(get_total_state "$type_classes" "$(r2c "GetGameState")" |
        jq -r '.[] | select(.player=="'$player'" and (select(.name | test("Yard")))) | .pointerSelf')

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

sell_mcv_test
