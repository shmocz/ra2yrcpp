#!/bin/bash

[ ! -f "$1" ] && exit 1

# TODO(shmocz): avoid creating temporary file
grep -P "warning" "$1" | grep -Pv '(Wunknown-pragmas|Wcomment|#pragma\s+warning|proto|xbyak\.h)' >err.log

e="$(cat err.log)"

[ -n "$e" ] && {
    echo "build check failed:"
    echo "$e"
    exit 1
}

exit 0
