#!/bin/bash

#I="$(cat <"$1")"
I="$(make lint 2>&1)"

# Fix "already" included
echo "$I" | grep -Po '^\K[^\s]+(?=:\s+.+\[build/include\].*)' |
	tr ':' '\t' | while read fname line; do
	sed -i "${line}d" $fname
done

# Fix c-style cast
echo "$I" | grep -P '\[readability/casting\].*' | while read line; do
	# get fname, pattern and sub
	echo $line | perl -p -e 's/([^:]+):(\d+):.+\s+(\w+)_cast<(.+)>.+$/$1 $2 $4 $3/g'
done | while read fname line patt cast_t; do
	patt="$(echo "$patt" | sed 's/\*/\\*/g')"
	S='s/(.+)\(('"$patt"')\)([^;,]+)(.+)/$1'"$cast_t"'_cast<$2>($3)$4/g if $. == '"$line"
	perl -i -pe "$S" "$fname"
done

# Single param ctors
echo "$I" | grep -P '\[runtime/explicit\].*' | while read line; do
	# get fname and lineno
	echo $line | perl -p -e 's/([^:]+):(\d+):.+$/$1 $2/g'
done | while read fname line; do
	S='s/(\s*)(\w+)(\(.+)/$1explicit $2$3/g if $. == '"$line"
	perl -i -pe "$S" "$fname"
done

# <pat1>(<T>)<var><pat2>
# <pat1>reinterpret_cast\<T\>(<var>)<pat2>
