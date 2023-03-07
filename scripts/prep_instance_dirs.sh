#!/usr/bin/env bash

INSTANCES="$1"
PLAYER_ID=${2:-""}

set -o nounset

# Path to directory containing RA2 game data files
p_main="$RA2YRCPP_GAME_DIR"
# ra2yrcpp library files (FXIME)
p_libs="$RA2YRCPP_PKG_DIR"

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

function get_spawn_ini_settings() {
    echo "[Settings]
AIPlayers=0
AlliesAllowed=False
AttackNeutralUnits=Yes
Bases=Yes
BridgeDestroy=False
BuildOffAlly=False
Color=$color
Credits=10000
FogOfWar=No
FrameSendRate=2
GameID=12850
GameMode=1
GameSpeed=0
Host=$is_host
IsSpectator=False
MCVRedeploy=True
MultiEngineer=False
Name=$name
PlayerCount=2
Port=$port
Protocol=0
Ra2Mode=False
Scenario=spawnmap.ini
Seed=1852262696
ShortGame=True
Side=6
SidebarHack=Yes
SuperWeapons=False
UIGameMode=Battle
UIMapName=[4] Dry Heat
UnitCount=0"
}

: ${PLAYERS_CONFIG:=test_data/envs.tsv}

IFS=$'\n' read -d '' -r -a cfgs <"$PLAYERS_CONFIG"
cfgs=("${cfgs[@]:1}")

function get_others() {
    for i in ${!cfgs[@]}; do
        c="${cfgs[$i]}"
        read -r name color side is_host is_observer port <<<$(echo "$c")
        if [ $i -eq $1 ]; then
            continue
        fi
        ix=$((i + 1))
        echo "[Other${ix}]
Name=$name
Side=$side
IsSpectator=False
Color=$color
Ip=0.0.0.0
Port=$port"
    done

}

function get_spawn_ini() {
    get_spawn_ini_settings
    echo "
[SpawnLocations]
Multi1=0
Multi2=1

[Tunnel]
Ip=0.0.0.0
Port=50000

"
    get_others "$1"
}

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

    cp test_data/spawnmap.ini "$ifolder"
    # write spawn.ini
    cfg="${cfgs[$i]}"
    read -r name color side is_host is_observer port <<<$(echo "$cfg")
    get_spawn_ini "$i" >"$ifolder/spawn.ini"
done
