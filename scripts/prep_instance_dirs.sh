#!/usr/bin/env bash

set -o nounset

paths=(BINKW32.DLL
    spawner.xdp
    ra2.mix
    ra2md.mix
    theme.mix
    thememd.mix
    langmd.mix
    language.mix
    expandmd01.mix
    expandmd02.mix
    mapsmd03.mix
    maps02.mix
    maps01.mix
    Ra2.tlb
    INI
    Maps
    RA2.INI
    RA2MD.ini
    thememd.INI
    ddraw.ini
    Keyboard.ini
    KeyboardMD.ini
    Blowfish.tlb
    spawner2.xdp
    Blowfish.dll
    libgcc_s_dw2-1.dll
    libwinpthread-1.dll
    libstdc++-6.dll
    qres32.dll
    PATCHW32.DLL
    DRVMGT.DLL
    libyrclient.dll
    zlib1.dll
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
FrameSendRate=7
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
Protocol=2
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

p_main="$RA2YRCPP_GAME_DIR"
for i in ${!cfgs[@]}; do
    ifolder="$RA2YRCPP_TEST_INSTANCES_DIR/player_${i}"
    mkdir -p "$ifolder"
    for p in ${paths[@]}; do
        ln -fnrs "$p_main/$p" "$ifolder/$p"
    done

    ln -sfr ./ra2yrcppcli.exe "$ifolder"
    ln -sfr "$p_main/gamemd-spawn-patched.exe" "$ifolder"/gamemd-spawn.exe
    cp test_data/spawnmap.ini "$ifolder"
    # write spawn.ini
    cfg="${cfgs[$i]}"
    read -r name color side is_host is_observer port <<<$(echo "$cfg")
    get_spawn_ini "$i" >"$ifolder/spawn.ini"
done
