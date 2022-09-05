#!/bin/bash

: ${WINE_CMD:="wine"}
exec $WINE_CMD build/tests/${1}.exe --gtest_color=disable ${@:2}
