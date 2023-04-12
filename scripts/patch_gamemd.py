#!/usr/bin/env python
import sys
import re
import os
import struct
from typing import List, Tuple
from dataclasses import dataclass
import argparse
import logging as lg
from iced_x86 import *


@dataclass
class Section:
    name: str
    size: int
    vaddr: int
    paddr: int


OFF_TEXT = 0x401000 - 0x1000
START_P_TEXT = 0xB7A000
OFF_P_TEXT = 0xB7A000 - 0x47E000


def patch(b, b1, offset):
    l = len(b)
    e = offset + len(b1)
    assert e <= l, f"l={l}, e={e}"
    b[offset:e] = b1


def get_section(sections: List[Section], vaddr: int) -> Section:
    """Locate seciton containing given vaddr"""
    for s in sections:
        if vaddr >= s.vaddr and vaddr <= s.vaddr + s.size:
            return s


def get_section_paddr(sections: List[Section], paddr: int) -> Section:
    """Locate seciton containing given paddr"""
    for s in sections:
        if paddr >= s.paddr and paddr <= s.paddr + s.size:
            return s


def map_vaddr(sections: List[Section], vaddr: int) -> int:
    """Map virtual address to physical address"""
    s = get_section(sections, vaddr)
    r = s.paddr + (vaddr - s.vaddr)
    assert r >= 0
    return r


def map_paddr(sections: List[Section], paddr: int) -> int:
    """Map physical address to virtual address"""
    s = get_section(sections, paddr)
    r = (paddr - s.paddr) + s.vaddr
    assert r >= 0
    return r


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
    binary: bytes,
    address: int,
    addr_detours: int,
    code: bytes,
    sections: List[Section] = None,
) -> int:
    """
    Parameters
    ----------
    binary : bytes
        PE binary
    address : int
        target virtual address
    addr_detours : int
        detours virtual address
    code : bytes
        code to write
    sections : List[Section], optional

    Returns
    -------
    int
        Size of newly created detour
    """
    paddr = map_vaddr(sections, address)
    section_detours = get_section(sections, addr_detours)
    offset_detours = addr_detours - section_detours.vaddr
    # determine numbers of instructions to disassemble
    c = decode_until(binary[paddr : (paddr + 256)], ip=address)

    bb = decode_bytes(binary[paddr : (paddr + 256)], ip=address, count=c)
    # FIXME: how to handle RET?
    to_copy = bytearray()
    if not all(list(str(x) in ["ret", "nop"] for x in bb)):
        to_copy = bytearray(binary[paddr : (paddr + c)])
    else:  # put another return val
        binary[(paddr + c) : (paddr + c + 1)] = b"\xc3"
        to_copy = bytearray(b"\x90" * 6)

    # make detour
    detour = to_copy + code + pushret(address + c)
    # copy bytes to trampoline area
    patch(binary, detour, section_detours.paddr + offset_detours)
    lg.info(
        "patch: target,detour,len(detour)=%x,%x,%d",
        address,
        addr_detours,
        len(detour),
    )
    pr_1 = pushret(addr_detours)
    assert c - len(pr_1) >= 0
    b = pr_1 + bytearray([0x90] * (c - len(pr_1)))
    # patch original binary
    patch(binary, b, paddr)
    return len(detour)


def create_detour_trampolines(
    binary: bytes,
    addresses: List[int],
    patches: List[Tuple[str, int, bytes]] = None,
    sections: List[Section] = None,
    detour_address: int = 0xB7E6AC,
):
    patches = patches or []
    addr_detours = detour_address
    # Create simple detours to target addresses
    for addr in addresses:
        addr_detours = addr_detours + create_detour(
            binary, addr, addr_detours, b"", sections
        )

    # Create custom detours and raw patches
    for ptype, addr, code in patches:
        if ptype == "d":
            addr_detours = addr_detours + create_detour(
                binary, addr, addr_detours, code, sections
            )
        elif ptype == "r":
            patch(binary, code, map_vaddr(sections, addr))
        else:
            raise RuntimeError(f"Invalid patch type {ptype}")


def write_out(b):
    fp = sys.stdout.buffer
    fp.write(b)
    fp.flush()


def auto_int(x):
    return int(x, 0)


def parse_args():
    a = argparse.ArgumentParser(
        description="Patch gamemd executable and output result to stdout",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    a.add_argument(
        "-s",
        "--sections",
        action="append",
        help="Section information for virtual address conversion in format: <name>:<size>:<vaddr>:<paddr>",
    )
    a.add_argument(
        "-d",
        "--detour-address",
        type=auto_int,
        default=0xB7E6AC,
        help="Virtual address of section where to write detours",
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
    for ptype, s_addr, path in [
        re.match(r"(d|r)0x([a-fA-F0-9]+):(.+)", p).groups() for p in patches
    ]:
        addr = int(s_addr, base=16)
        if os.path.exists(path):
            with open(path, "rb") as f:
                yield (ptype, addr, f.read())
        else:
            yield (ptype, addr, bytearray.fromhex(path))


def get_sections(sections: List[str]) -> List[Section]:
    res = []
    for s in sections:
        z = s.split(":")
        res.append(Section(z[0], *[int(x, 0) for x in z[1:]]))
    return res


def main():
    lg.basicConfig(level=lg.INFO)
    a = parse_args()
    addrs = get_hook_addresses(get_source())

    with open(a.input, "rb") as f:
        b = bytearray(f.read())
        patches = list(get_patches(a.patches))
        sections = get_sections(a.sections)
        create_detour_trampolines(b, addrs, patches, sections, a.detour_address)
        write_out(b)


if __name__ == "__main__":
    main()
