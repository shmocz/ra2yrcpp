#!/usr/bin/env python
import sys
import re
import os
import struct
import subprocess
import shutil
import tempfile
from pathlib import Path
from typing import List, Any, Iterable
from dataclasses import dataclass
import argparse
import logging as lg


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


def read_file(p, mode="r"):
    with open(p, mode) as f:
        return f.read()


def get_hook_addresses(source_file: str):
    """Parse hook addresses from .cpp files and return their physical offsets"""
    hooks_s = re.search("gg_hooks([^;]+);", source_file, re.S)[0]
    return [int(x, base=16) for x in re.findall("(0x[a-zA-Z0-9]+)", hooks_s)]


def pushret(address):
    return bytearray([0x68]) + struct.pack("<I", address) + b"\xc3"


def decode_bytes(b: bytes, ip=0, count=0):
    from iced_x86 import Decoder

    d = Decoder(32, b, ip=ip)
    for inst, _ in zip(d, range(count)):
        yield inst


def decode_until(b: bytes, ip=0, max_ins=10):
    for inst in decode_bytes(b, ip=ip, count=max_ins):
        if (inst.ip - ip) >= 6:
            return inst.ip - ip


@dataclass
class Hook:
    vaddr: int
    paddr: int
    size: int


@dataclass
class Patch:
    vaddr: int
    paddr: int
    size: int
    code: bytearray = None

    @classmethod
    def from_address(cls, b: bytearray, sections, vaddr: int):
        paddr = map_vaddr(sections, vaddr)
        c = decode_until(b[paddr : (paddr + 256)], ip=paddr)
        return Patch(vaddr, paddr, c)

    def to_patch_string(self):
        if not self.code:
            return f"d0x{self.vaddr:02x}:s{self.size}"
        return f"d0x{self.vaddr:02x}:{self.code.hex()}:s{self.size}"


@dataclass
class Handle:
    binary: bytearray
    args: Any
    patches: List[Patch] = None
    sections: List[Section] = None


def make_detour(
    binary: bytes,
    h: Patch,
    addr_detours: int,
    sections: List[Section] = None,
    code=None,
):
    code = code or bytearray()
    section_detours = get_section(sections, addr_detours)
    offset_detours = addr_detours - section_detours.vaddr
    detour = (
        binary[h.paddr : (h.paddr + h.size)] + code + pushret(h.vaddr + h.size)
    )
    patch(binary, detour, section_detours.paddr + offset_detours)
    pr_1 = pushret(addr_detours)
    pr_2 = pr_1 + bytearray([0x90] * (h.size - len(pr_1)))
    patch(binary, pr_2, h.paddr)
    return len(detour)


def write_out(b, path=None):
    fp = sys.stdout.buffer
    if path:
        fp = open(path, "wb")
    fp.write(b)
    fp.flush()
    fp.close()


def auto_int(x):
    return int(x, 0)


def parse_args():
    a = argparse.ArgumentParser(
        description="Patch gamemd executable.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    a.add_argument(
        "-b",
        "--build-dir",
        type=Path,
        help="ra2yrcpp build dir",
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
        default=[],
        help="Extra patches to apply in format <type><address>:<path>. <type> can be 'd' (detour) or 'r' (raw) Address is in hex.",
    )
    a.add_argument(
        "-r",
        "--raw",
        type=Path,
        default=Path("data") / "patches.txt",
        help="Apply raw patches specified in given file",
    )
    a.add_argument(
        "-f",
        "--source-file",
        default=os.path.join("src", "hooks_yr.cpp"),
        help="C++ source from which to parse hook addresses",
    )
    a.add_argument(
        "-D",
        "--dump-patches",
        action="store_true",
        help="Don't output patched binary. Rather all patches to be applied in a format suitable for -p parameter",
    )
    a.add_argument(
        "-i", "--input", default="gamemd-spawn.exe", help="gamemd path"
    )
    a.add_argument(
        "-o",
        "--output",
        default=None,
        help="Output file. If unspecified, write to stdout",
    )
    a.add_argument(
        "-a",
        "--auto-patch",
        action="store_true",
        help="Auto patch spawner",
    )
    return a.parse_args()


def parse_patches(h: Handle, patches) -> Iterable[Patch]:
    for ptype, s_addr, path in [
        re.match(r"(d|r)0x([a-fA-F0-9]+):(.+)", p).groups() for p in patches
    ]:
        addr = int(s_addr, base=16)
        if os.path.exists(path):
            p = Patch.from_address(h.binary, h.sections, addr)
            p.code = bytearray(read_file(path, "rb"))
            yield p
        elif m := re.match(r"s(\d+)", path):
            yield Patch(addr, map_vaddr(h.sections, addr), int(m[1]))
        elif m := re.match(r"([a-fA-F0-9]+):s(\d+)", path):
            code = bytearray.fromhex(m[1])
            yield Patch(addr, map_vaddr(h.sections, addr), int(m[2]), code=code)
        elif ptype == "r":
            code = bytearray.fromhex(path)
            yield Patch(addr, map_vaddr(h.sections, addr), len(code), code=code)
        else:
            raise RuntimeError(f"invalid patch type: {ptype} {path}")


def get_sections(sections: List[str]) -> List[Section]:
    res = []
    for z in (s.split(":") for s in sections):
        res.append(Section(z[0], *[int(x, 0) for x in z[1:]]))
    return res


def do_patching(h: Handle) -> bytearray:
    addr_detours = h.args.detour_address
    binary_patched = h.binary.copy()
    for p in h.patches:
        addr_detours += make_detour(
            binary_patched, p, addr_detours, h.sections, p.code
        )
    return binary_patched


def get_handle(a) -> Handle:
    H = Handle(binary=bytearray(read_file(a.input, "rb")), args=a)
    H.sections = get_sections(a.sections)
    H.patches = list(parse_patches(H, H.args.patches))
    if a.raw:
        raw_patches = re.split(r"\s+", read_file(H.args.raw))
        H.patches.extend(list(parse_patches(H, raw_patches)))
    else:
        addr_hooks = get_hook_addresses(read_file(a.source_file))
        H.patches.extend(
            Patch.from_address(H.binary, H.sections, x) for x in addr_hooks
        )
    return H


def prun(args, **kwargs):
    cmdline = [str(x) for x in args]
    lg.info("exec: %s", cmdline)
    return subprocess.run(cmdline, **kwargs)


def cmd_gamemd_patch():
    return [
        "python3",
        "./scripts/patch_gamemd.py",
        "-s",
        ".p_text:0x00004d66:0x00b7a000:0x0047e000",
        "-s",
        ".text:0x003df38d:0x00401000:0x00001000",
    ]


def cmd_addscn(addscn_path, dst):
    return [addscn_path, dst, ".p_text2", "0x1000", "0x60000020"]


def add_section(addscn_path, src, dst):
    # Append new section to and write PE section info to text file
    o = prun(cmd_addscn(addscn_path, src), check=True, capture_output=True)
    pe_info_d = str(o.stdout, encoding="utf-8").strip()

    # wait wine to exit
    if sys.platform != "win32":
        prun(["wineserver", "-w"], check=True)
    shutil.copy(src, dst)
    return pe_info_d


def auto_patch(args):
    td = tempfile.TemporaryDirectory()
    b_temp = Path(td.name) / "tmp.exe"
    shutil.copy(args.input, b_temp)
    b_dst = Path(td.name) / "tmp-2.exe"

    pe_info_d = add_section(Path(args.build_dir) / "addscn.exe", b_temp, b_dst)
    p_text_2_addr = pe_info_d.split(":")[2]

    prun(
        cmd_gamemd_patch()
        + [
            "-s",
            pe_info_d,
            "-d",
            p_text_2_addr,
            "--raw",
            args.raw,
            "--input",
            b_dst,
            "-o",
            args.output,
        ]
    )


def main():
    lg.basicConfig(level=lg.INFO)
    a = parse_args()

    if a.auto_patch:
        return auto_patch(a)

    H = get_handle(a)
    if H.args.dump_patches:
        return write_out(
            bytes(
                "\n".join(p.to_patch_string() for p in H.patches),
                encoding="utf8",
            ),
            a.output,
        )
    binary_patched = do_patching(H)
    return write_out(binary_patched, a.output)


if __name__ == "__main__":
    main()
