#!/bin/bash

set -o nounset

# Initialize wine env if not done
if [ ! -d "$WINEPREFIX" ]; then
    mkdir -p "$WINEPREFIX"
    wineboot -ik
    wine regedit test_data/env.reg
    wineboot -s
    # need to wait to avoid losing inserted registry entries
    wineserver -w
fi

# Prepare instance directory
{
    CMAKE_RUNTIME_OUTPUT_DIRECTORY="$BUILDDIR/bin" BUILDDIR="$BUILDDIR" ./scripts/prep_instance_dirs.sh "$RA2YRCPP_TEST_INSTANCES_DIR" "$PLAYER_ID"
    cd "$RA2YRCPP_TEST_INSTANCES_DIR/$PLAYER_ID"
    # Inject and start game
    ./ra2yrcppcli.exe -p "$RA2YRCPP_PORT" &
    wine gamemd-spawn.exe -SPAWN 2>err.log
} >"$RA2YRCPP_TEST_INSTANCES_DIR/$PLAYER_ID/out.log"
