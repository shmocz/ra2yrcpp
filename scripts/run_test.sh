#!/bin/bash

W="${WINE_CMD:-wine}"
$W build/tests/${1}.exe --gtest_color=disable ${@:2}
