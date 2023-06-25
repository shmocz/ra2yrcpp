#!/bin/bash

set -o nounset

export WINEPREFIX="$RA2YRCPP_TEST_INSTANCES_DIR/$PLAYER_ID/.wine"
# Initialize wine env if not done
if [ ! -d "$WINEPREFIX" ]; then
    mkdir -p "$WINEPREFIX"
    wineboot -ik
    wine regedit test_data/env.reg
    wineboot -s
    # need to wait to avoid losing inserted registry entries
    wineserver -w
fi

: ${WINE_CMD:="wine"}
idir="$RA2YRCPP_TEST_INSTANCES_DIR/$PLAYER_ID"
mkdir -p "$idir"

# Prepare instance directory
{
    RA2YRCPP_PKG_DIR="$DEST_DIR/bin" ./scripts/prep_instance_dirs.sh "$RA2YRCPP_TEST_INSTANCES_DIR" "$PLAYER_ID"
    cd "$idir"
    WINEDEBUG="+loaddll" $WINE_CMD gamemd-spawn.exe -SPAWN 2>err.log
} >"$idir/out.log"
