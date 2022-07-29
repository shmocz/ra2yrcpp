#!/bin/bash

# Check that certain .cpp files don't contain extra includes
function check_include_headers() {
	f="$(echo "$CPP_SOURCES" | sed -E -e 's/\s+/\n/g' | grep -E '\.cpp')"
	grep -En "#include" $f |
		grep -Ev 'tests/|dll_inject|dllitool|recordtool|debug_helpers|network\.cpp|process\.cpp|state_parser\.cpp|ra2yrcppcli\.cpp' |
		grep -Ev 'errors.cpp.+windows|yrclient_dll' | # windows-related
		perl -ne \
			'my ($f, $l, $s) = ($_ =~ m/src\/([^:]+\.cpp):(\d+):.*#include\s+[<"]([^">]+)[>"].*/);
			my $d = $s =~ s/.hpp/.cpp/gr;
			if ($f ne $d) {
				print $_;
			}
			'
}

{
	git diff $CM_FILES
	check_include_headers
} >err.log

e="$(cat err.log)"

[ -n "$e" ] && {
	echo "$e"
	exit 1
}

exit 0
