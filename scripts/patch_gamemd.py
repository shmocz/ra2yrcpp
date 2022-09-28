#!/usr/bin/env python
import sys
import re
import struct
from typing import List
from iced_x86 import *

OFF_TEXT = 0x401000 - 0x1000
START_P_TEXT = 0xB7A000
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


def eprint(*args):
    print(*args, file=sys.stderr)


def get_paddr(addr: int):
    if addr - START_P_TEXT < 0:
        return addr - OFF_TEXT
    return addr - OFF_P_TEXT


def get_source():
    with open("src/commands_yr.cpp", "r") as f:
        return f.read()


def get_hook_addresses(source_file: str):
    """Parse hook addresses from .cpp files and return their physical offsets"""
    hooks_s = re.search("g_hooks_ng([^;]+);", source_file, re.S)[0]
    return [int(x, base=16) for x in re.findall("(0x[a-zA-Z0-9]+)", hooks_s)]


def pushret(address):
    return bytearray([0x68]) + struct.pack("<I", address) + b"\xc3"


def decode_until(b: bytes, ip=0, max_ins=10):
    d = Decoder(32, b, ip=ip)
    for inst, _ in zip(d, range(max_ins)):
        if (inst.ip - ip) >= 6:
            return inst.ip - ip


def create_detour_trampolines(binary: bytes, addresses: List[int]):
    # for each address
    addr_detours = 0xB7E6AC - OFF_P_TEXT
    for addr in addresses:
        paddr = get_paddr(addr)
        # determine numbers of instructions to disassemble
        c = decode_until(binary[paddr : (paddr + 256)], ip=addr)

        # make detour
        detour = bytearray(binary[paddr : (paddr + c)]) + pushret(addr + c)
        # copy bytes to trampoline area
        patch(binary, detour, addr_detours)
        eprint(
            "PATCH",
            hex(addr),
            "DETOUR",
            hex(addr_detours + OFF_P_TEXT),
            "SIZE",
            len(detour),
        )
        pr_1 = pushret(addr_detours + OFF_P_TEXT)
        assert c - len(pr_1) >= 0
        b = pr_1 + bytearray([0x90] * (c - len(pr_1)))
        patch(binary, b, paddr)
        addr_detours = addr_detours + len(detour)


def write_out(b):
    fp = sys.stdout.buffer
    fp.write(b)
    fp.flush()


def main():
    addrs = get_hook_addresses(get_source())

    with open(sys.argv[1], "rb") as f:
        b = bytearray(f.read())
        create_detour_trampolines(b, addrs)
        write_out(b)


if __name__ == "__main__":
    main()
