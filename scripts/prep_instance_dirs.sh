#!/usr/bin/env bash

INSTANCES="$1"
PLAYER_ID=${2:-""}

set -o nounset

# Path to directory containing RA2 game data files
p_main="$RA2YRCPP_GAME_DIR"
# ra2yrcpp library files (FXIME)
p_libs="$RA2YRCPP_PKG_DIR"
p_map_path="$MAP_PATH"
p_ini_override="$INI_OVERRIDE"
: ${PLAYERS_CONFIG:=test_data/envs.tsv}

[ -d "$p_libs" ] || {
    echo "$p_libs not found"
    exit 1
}

paths=(
    BINKW32.DLL
    spawner.xdp
    ra2.mix
    ra2md.mix
    theme.mix
    thememd.mix
    langmd.mix
    language.mix
    expandmd01.mix
    mapsmd03.mix
    maps02.mix
    maps01.mix
    Ra2.tlb
    INI
    Maps
    RA2.INI
    RA2MD.ini
    ddraw.ini
    spawner2.xdp
    Blowfish.dll
    ddraw.dll)

IFS=$'\n' read -d '' -r -a cfgs <"$PLAYERS_CONFIG"
cfgs=("${cfgs[@]:1}")

function mklink() {
    it=("$@")
    params=("${@:1:$(($# - 1))}")
    target="${it[-1]}"
    for ii in "${params[@]}"; do
        [ ! -e "$ii" ] && {
            echo "$ii not found" && exit 1
        }
        ln -vfrns "$ii" "$target"
    done
}

for i in ${!cfgs[@]}; do
    if [ ! -z "$PLAYER_ID" ] && [[ "$PLAYER_ID" != "player_${i}" ]]; then
        continue
    fi
    cfg="${cfgs[$i]}"
    read -r name color side is_host is_observer port ai_difficulty <<<$(echo "$cfg")
    if [ "$ai_difficulty" != "-1" ]; then
        continue
    fi
    ifolder="$INSTANCES/player_${i}"
    mkdir -p "$ifolder"
    for p in ${paths[@]}; do
        mklink "$p_main/$p" "$ifolder/$p"
    done

    # link files
    cd "$p_libs"
    mklink "gamemd-spawn-patched.exe" "$ifolder"/gamemd-spawn.exe
    mklink ra2yrcppcli.exe *.dll "$ifolder"
    cd -

    cp -f "$p_map_path" "$ifolder/spawnmap.ini"
    if [ -f "$p_ini_override" ]; then
        cat "$p_ini_override" >>"$ifolder/spawnmap.ini"
    fi
    # write spawn.ini
    # get_spawn_ini "$i" >"$ifolder/spawn.ini"
    game_mode="yr"
    if [ "$RA2_MODE" == "True" ]; then
        game_mode="ra2"
    fi

    ./scripts/generate-spawnini.py \
        -i "$PLAYERS_CONFIG" \
        -f "$FRAME_SEND_RATE" \
        -pr "$PROTOCOL_VERSION" \
        -g "$game_mode" \
        -p "$i" \
        -s "$GAME_SPEED" \
        -t "$TUNNEL_ADDRESS" \
        -tp "$TUNNEL_PORT" \
        >"$ifolder/spawn.ini"
done
