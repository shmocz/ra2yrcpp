#!/bin/bash

perl -ne '
my ($level, $tid, $ts, $msg) = ($_ =~ m/(DEBUG|ERROR):\s+\[thread\s+(\d+)\s+TS:\s+(\d+)\]:\s*(.+)/g);
if ($msg) {
	$msg =~ s/\s+$//g;
	$ts = $ts / 1e9;
	print join("\t", $level, $tid, $ts, $msg) . "\n";
}
'
