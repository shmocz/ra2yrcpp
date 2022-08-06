#!/usr/bin/env python
import sys

OFF_TEXT = 0x401000 - 0x1000
OFF_P_TEXT = 0xB7A000 - 0x47E000


def x(s):
    return bytes(
        int(s[(i * 2) : ((i + 1) * 2)], base=16) for i in range(int(len(s) / 2))
    )


def patch(b, b1, offset):
    l = len(b)
    e = offset + len(b1)
    assert e <= l, f"l={l}, e={e}"
    b[offset:e] = b1


# Not sure about publishing these, so for now, it's left as an exercise to the reader.
def main():
    b = None
    l0 = 0
    data = [
        ("3bc7891584eda8006887de5500c3", 0xDEADBEEF),
        ("68ace6b700c39090", 0xDEADBEEF),
        ("68bce6b700c3", 0xDEADBEEF),
        ("6768cce6b700c39090", 0xDEADBEEF),
        ("a0e0fbb000536768b6df7200c3", 0xDEADBEEF),
        ("8a0de0d4810083c4106768d3cd4800c3", 0xDEADBEEF),
        ("68dce6b700c39090909090", 0xDEADBEEF),
        ("a06beda80081ecdc010000689bae6900c3", 0xDEADBEEF),
    ]

    with open(sys.argv[1], "rb") as f:
        b = bytearray(f.read())
        l0 = len(b)
        for val, offset in data:
            patch(b, x(val), offset)
    assert len(b) == l0

    fp = sys.stdout.buffer
    fp.write(b)
    fp.flush()


if __name__ == "__main__":
    main()
