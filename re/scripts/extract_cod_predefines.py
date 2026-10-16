#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract the symbols that 1602.exe predefines before parsing a .cod file.

The .cod loaders (Cod_LoadHaeuser @0x460750, Cod_LoadFiguren @0x441210) register constants such as
BODEN=11, MAXHAUS=500 or WAV_* sample ids by calling Cod_DefineSymbol(name, value, 0) @0x4020d0 -
partly as straight-line calls, partly in loops over string pointer tables (value = table index).
These names are not defined in the .cod files themselves, so a reimplementation needs them.

This script interprets the loader prologue (everything before the first call to Cod_ParseLine
@0x402260) with a tiny x86 subset interpreter over GNU objdump output and records every
Cod_DefineSymbol call.

  re/scripts/extract_cod_predefines.py 1602.exe --json            # inspect
  re/scripts/extract_cod_predefines.py 1602.exe --cpp > engine/assets/src/cod_predefined.cpp
"""
import argparse
import json
import re
import struct
import subprocess
import sys

DEFINE_SYMBOL = 0x4020D0
PARSE_LINE = 0x402260
LOADERS = {"haeuser": 0x460750, "figuren": 0x441210}


class Image:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.data, pe + 6)[0]
        opt = pe + 24
        self.base = struct.unpack_from("<I", self.data, opt + 28)[0]
        size_opt = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.sections = []
        for i in range(nsec):
            _, vsize, va, rsize, roff = struct.unpack_from("<8sIIII", self.data, opt + size_opt + i * 40)
            self.sections.append((self.base + va, max(vsize, rsize), roff, rsize))

    def offset(self, va):
        for start, size, roff, rsize in self.sections:
            if start <= va < start + size and va - start < rsize:
                return roff + va - start
        raise ValueError(f"address 0x{va:x} not backed by file data")

    def u32(self, va):
        return struct.unpack_from("<I", self.data, self.offset(va))[0]

    def cstr(self, va):
        o = self.offset(va)
        return self.data[o:self.data.index(b"\0", o)].decode("cp1252")


INSN = re.compile(r"^\s*([0-9a-f]+):\s+(\S+)\s*(.*)$")


def disassemble(path, start, length=0x3000):
    out = subprocess.run(["objdump", "-d", "-M", "intel", "--no-show-raw-insn",
                          f"--start-address=0x{start:x}", f"--stop-address=0x{start + length:x}", path],
                         capture_output=True, text=True, check=True).stdout
    insns = []
    for line in out.splitlines():
        m = INSN.match(line)
        if m:
            insns.append((int(m.group(1), 16), m.group(2), m.group(3).strip()))
    return insns


def imm(text):
    text = text.strip()
    return int(text, 16) if text.startswith("0x") else int(text)


def run_prologue(image, insns):
    """Interprets until the first call to Cod_ParseLine; returns [(name, value)] in call order."""
    index = {addr: i for i, (addr, _, _) in enumerate(insns)}
    regs = {}
    stack = []
    flags = None
    defines = []
    i = 0
    steps = 0
    while i < len(insns):
        steps += 1
        if steps > 200000:
            raise RuntimeError("runaway interpretation")
        addr, op, args = insns[i]
        i += 1
        a = [x.strip() for x in args.split(",")] if args else []
        if op == "push":
            if a[0] in regs:
                stack.append(regs[a[0]])
            else:
                try:
                    stack.append(imm(a[0]))
                except ValueError:
                    stack.append(None)
        elif op == "mov" and len(a) == 2 and re.fullmatch(r"e[a-d]x|e[sd]i|ebp", a[0]):
            m = re.fullmatch(r"DWORD PTR \[(e\w\w)\]", a[1])
            if m and regs.get(m.group(1)) is not None:
                regs[a[0]] = image.u32(regs[m.group(1)])
            elif a[1] in regs:
                regs[a[0]] = regs[a[1]]
            else:
                try:
                    regs[a[0]] = imm(a[1])
                except ValueError:
                    regs[a[0]] = None
        elif op == "xor" and len(a) == 2 and a[0] == a[1]:
            regs[a[0]] = 0
        elif op == "inc" and a[0] in regs and regs[a[0]] is not None:
            regs[a[0]] += 1
        elif op == "add" and a[0] == "esp":
            for _ in range(imm(a[1]) // 4):
                if stack:
                    stack.pop()
        elif op == "add" and a[0] in regs and regs[a[0]] is not None:
            regs[a[0]] += imm(a[1])
        elif op == "cmp" and a[0] in regs:
            rhs = regs.get(a[1]) if a[1] in regs else (imm(a[1]) if re.fullmatch(r"0x[0-9a-f]+|\d+", a[1]) else None)
            flags = (regs[a[0]], rhs) if rhs is not None else None
        elif op == "jl" and flags is not None:
            target = int(a[0].split()[0], 16)
            if target < addr and flags[0] is not None and flags[0] < flags[1]:
                i = index[target]
            flags = None
        elif op == "call":
            target = a[0].split()[0]
            if target.startswith("0x"):
                t = int(target, 16)
                if t == PARSE_LINE:
                    return defines
                if t == DEFINE_SYMBOL:
                    name_ptr, value = stack[-1], stack[-2]
                    defines.append((image.cstr(name_ptr), value if value < 0x80000000 else value - (1 << 32)))
            regs["eax"] = None  # cdecl: caller cleans the stack, eax clobbered
        elif op in ("ret",):
            break
    raise RuntimeError("Cod_ParseLine call not reached")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("exe")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--json", action="store_true")
    g.add_argument("--cpp", action="store_true")
    args = p.parse_args()

    image = Image(args.exe)
    result = {}
    for name, addr in LOADERS.items():
        result[name] = run_prologue(image, disassemble(args.exe, addr))

    if args.json:
        json.dump({k: [{"name": n, "value": v} for n, v in d] for k, d in result.items()}, sys.stdout, indent=1)
        print()
        return 0

    print("// SPDX-License-Identifier: GPL-3.0-or-later")
    print("// GENERATED by re/scripts/extract_cod_predefines.py from 1602.exe (GOG Koenigsedition,")
    print("// sha256 33c428d7...726e). Do not edit by hand.")
    print("//")
    print("// Symbols the original game defines before parsing haeuser.cod / figuren.cod (in call order;")
    print("// later definitions of the same name overwrite earlier ones, as in the original).")
    print('#include "o1602/assets/cod_predefined.hpp"')
    print()
    print("namespace o1602::assets {")
    print()
    print("namespace {")
    for name, defines in result.items():
        print(f"constexpr CodPredefinedSymbol k{name.capitalize()}[] = {{")
        for sym, value in defines:
            print(f'    {{"{sym}", {value}}},')
        print("};")
    print("}  // namespace")
    print()
    for name in result:
        print(f"std::span<const CodPredefinedSymbol> cod_predefined_{name}() noexcept {{ return k{name.capitalize()}; }}")
    print()
    print("}  // namespace o1602::assets")
    return 0


if __name__ == "__main__":
    sys.exit(main())
