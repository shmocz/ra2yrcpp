#!/usr/bin/bash

# Replace bug causing "--"
cat build/compile_commands.json | jq '[ .[] | .command |= sub("-c\\s+--"; "-c") ]' >.c.json
cp .c.json build/compile_commands.json
../iwyu/include-what-you-use/iwyu_tool.py -j 16 -p build src tests -- -Xiwyu --mapping_file="$(pwd)/.tmp/srv.imp" | tee iwyu.out
./scripts/iwyu-check.py iwyu.out |
    grep -Pv "vcruntime_new|gtest-|websocketpp|net/proto2|xtree|xtr1common|type_traits.+for\s+move|system_error.+for\s+error_code" |
    tee iwyu.filt.out
