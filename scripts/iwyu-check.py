#!/usr/bin/env python3

import re
import sys

data = None
with open(sys.argv[1]) as f:
    data = f.read()


def parse_entries(entries):
    for e in entries:
        # m = re.search(r"(?s)^(.+?)\s+should add these lines:(.+?)\n\n", e)
        lines_add = ""
        lines_remove = ""
        fname = ""
        m = re.search(r"(?m)^(.+?)\s+should add these lines", e)
        if not m:
            continue
        fname = m[1]
        m = re.search(r"(?s)should add these lines:(.*?)\n\n", e)
        if m:
            lines_add = m[1].strip()
            assert lines_add.find("should") == -1
        m = re.search(r"(?s)should remove these lines:(.*?)\n\n", e)
        if m:
            lines_remove = m[1].strip()
        s = f"{fname}\nadd:\n{lines_add}\nremove:\n{lines_remove}"
        yield s.strip()


print("\n\n".join(parse_entries(re.split(r"---", data))))
