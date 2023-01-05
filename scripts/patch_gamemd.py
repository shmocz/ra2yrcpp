#!/usr/bin/env python
import sys
import re
import os
import struct
from typing import List, Tuple
import argparse
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
    hooks_s = re.search("gg_hooks([^;]+);", source_file, re.S)[0]
    return [int(x, base=16) for x in re.findall("(0x[a-zA-Z0-9]+)", hooks_s)]


def pushret(address):
    return bytearray([0x68]) + struct.pack("<I", address) + b"\xc3"


def decode_bytes(b: bytes, ip=0, count=0):
    d = Decoder(32, b, ip=ip)
    for inst, _ in zip(d, range(count)):
        yield inst


def decode_until(b: bytes, ip=0, max_ins=10):
    for inst in decode_bytes(b, ip=ip, count=max_ins):
        if (inst.ip - ip) >= 6:
            return inst.ip - ip


def create_detour(
    binary: bytes, address: int, addr_detours: int, code: bytes
) -> int:
    paddr = get_paddr(address)
    # determine numbers of instructions to disassemble
    c = decode_until(binary[paddr : (paddr + 256)], ip=address)

    bb = decode_bytes(binary[paddr : (paddr + 256)], ip=address, count=c)
    # FIXME: how to handle RET?
    to_copy = bytearray()
    if not all(list(str(x) in ["ret", "nop"] for x in bb)):
        to_copy = bytearray(binary[paddr : (paddr + c)])
    else:  # put another return val
        eprint("fixing ret")
        binary[(paddr + c) : (paddr + c + 1)] = b"\xc3"
        to_copy = bytearray(b"\x90" * 6)

    # make detour
    detour = to_copy + code + pushret(address + c)
    # copy bytes to trampoline area
    patch(binary, detour, addr_detours)
    eprint(
        "PATCH",
        hex(address),
        "DETOUR",
        hex(addr_detours + OFF_P_TEXT),
        "SIZE",
        len(detour),
    )
    pr_1 = pushret(addr_detours + OFF_P_TEXT)
    assert c - len(pr_1) >= 0
    b = pr_1 + bytearray([0x90] * (c - len(pr_1)))
    patch(binary, b, paddr)
    return len(detour)


def create_detour_trampolines(
    binary: bytes,
    addresses: List[int],
    patches: List[Tuple[str, int, bytes]] = None,
):
    patches = patches or []
    addr_detours = 0xB7E6AC - OFF_P_TEXT
    for addr in addresses:
        addr_detours = addr_detours + create_detour(
            binary, addr, addr_detours, b""
        )

    for ptype, addr, code in patches:
        if ptype == "d":
            addr_detours = addr_detours + create_detour(
                binary, addr, addr_detours, code
            )
        elif ptype == "r":
            patch(binary, code, get_paddr(addr))
        else:
            raise RuntimeError(f"Invalid patch type {d}")


def write_out(b):
    fp = sys.stdout.buffer
    fp.write(b)
    fp.flush()


def parse_args():
    a = argparse.ArgumentParser(
        description="Patch gamemd executable and output result to stdout",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    a.add_argument(
        "-p",
        "--patches",
        action="append",
        help="Extra patches to apply in format <type><address>:<path>. <type> can be 'd' (detour) or 'r' (raw) Address is in hex.",
    )
    a.add_argument(
        "-i", "--input", default="gamemd-spawn.exe", help="gamemd path"
    )
    return a.parse_args()


def get_patches(patches: List[str]):
    eprint("patches", patches)
    for ptype, s_addr, path in [
        re.match(r"(d|r)0x([a-fA-F0-9]+):(.+)", p).groups() for p in patches
    ]:
        addr = int(s_addr, base=16)
        if os.path.exists(path):
            with open(path, "rb") as f:
                yield (ptype, addr, f.read())
        else:
            yield (ptype, addr, bytearray.fromhex(path))


def main():
    a = parse_args()
    addrs = get_hook_addresses(get_source())

    with open(a.input, "rb") as f:
        b = bytearray(f.read())
        patches = list(get_patches(a.patches))
        create_detour_trampolines(b, addrs, patches)
        write_out(b)


if __name__ == "__main__":
    main()
