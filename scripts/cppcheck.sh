#!/bin/sh

out="cppcheck.log"

# example override: cppcheck="docker run --user $UID:$UID --rm -v "$(pwd)":/mnt -w /mnt shmocz/cppcheck-action cppcheck"
: ${CPPCHECK:="cppcheck"}

$CPPCHECK -q --platform=win32W \
	--enable=warning,style,performance,portability,unusedFunction \
	-I src/ \
	--template='{file}:{line},{severity},{id},{message}' \
	--inline-suppr \
	--suppress=passedByValue \
	--suppress=noExplicitConstructor:src/utility/scope_guard.hpp \
	--suppress=unusedStructMember:src/commands_yr.cpp \
	--suppress=unusedFunction:tests/*.cpp \
	--suppress=uninitMemberVar:src/ra2/objects.cpp \
	--suppress=shadowVar:src/commands_yr.cpp \
	src/ tests/ 2>"$out"

[ "$(cat "$out" | wc -l)" != 0 ] && { cat "$out" && exit 1; }

exit 0
