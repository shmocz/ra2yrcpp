#!/bin/bash

W="${WINE_CMD:-wine}"
$W build/tests/${1}.exe ${@:2}
