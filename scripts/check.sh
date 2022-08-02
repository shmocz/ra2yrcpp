#!/bin/bash

# Check that certain .cpp files don't contain extra includes
function check_include_headers() {
	pt_win="$(echo "$W32_FILES" | sed -E -e 's/\./\\./g' -e 's/\s+/|/g')"
	f="$(echo "$CPP_SOURCES" | sed -E -e 's/\s+/\n/g' | grep -E '\.cpp')"
		#grep -Ev 'tests/|dll_inject|debug_helpers|network\.cpp|state_parser\.cpp' |
	grep -En "#include" $f |
		grep -Ev "$pt_win" |
		grep -Ev 'errors.cpp.+windows|yrclient_dll|process\.cpp' | # windows sources are allowed to have some windows-specific includes
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
