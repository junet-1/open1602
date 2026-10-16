#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Find code that references strings in a PE32 executable - lightweight helper until Ghidra/rea
is set up. Requires GNU objdump (binutils).

  re/scripts/string_xrefs.py 1602.exe haeuser.cod stadtfld

For every string containing one of the patterns, prints its virtual address and each
instruction that uses the address as an immediate, plus the containing function
(estimated as the nearest preceding direct call target).
"""
import bisect
import re
import struct
import subprocess
import sys


def sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt = pe + 24
    base = struct.unpack_from("<I", data, opt + 28)[0]
    size_opt = struct.unpack_from("<H", data, pe + 20)[0]
    out = []
    for i in range(nsec):
        name, vsize, va, rsize, roff = struct.unpack_from("<8sIIII", data, opt + size_opt + i * 40)
        out.append((name.rstrip(b"\0").decode(), base + va, vsize, roff, rsize))
    return out


def find_strings(data, secs, patterns):
    for m in re.finditer(rb"[\x20-\x7e]{4,}", data):
        text = m.group().decode()
        if not any(p.lower() in text.lower() for p in patterns):
            continue
        for name, va, vsize, roff, rsize in secs:
            if roff <= m.start() < roff + rsize:
                yield va + m.start() - roff, text


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    exe, patterns = sys.argv[1], sys.argv[2:]
    data = open(exe, "rb").read()
    secs = sections(data)
    asm = subprocess.run(["objdump", "-d", "-M", "intel", "--no-show-raw-insn", exe],
                         capture_output=True, text=True, check=True).stdout.splitlines()
    targets = sorted({int(m.group(1), 16) for line in asm
                      if (m := re.search(r"call\s+0x([0-9a-f]+)$", line.strip()))})
    for va, text in find_strings(data, secs, patterns):
        print(f"0x{va:08x}  {text!r}")
        needle = f"0x{va:x}"
        for line in asm:
            if re.search(rf"{needle}\b", line):
                site = int(line.split(":")[0], 16)
                k = bisect.bisect_right(targets, site) - 1
                func = f"0x{targets[k]:08x}" if k >= 0 else "?"
                print(f"    {line.strip():<60}  (in function ~{func})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
