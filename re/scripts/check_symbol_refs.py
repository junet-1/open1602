#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Lists addresses of 1602.exe that the code cites but neither the symbol tables nor docs/re name.

Usage: python3 re/scripts/check_symbol_refs.py [repo root]
An address counts as known when it is a row of re/symbols/functions.csv or globals.csv, or appears
anywhere in docs/re. Both live in the private repository open1602-re, so run this locally with them
linked in (CONTRIBUTING.md). Exit code 1 lists the unknown addresses with the places citing them.
Also fails when a row of the symbol tables does not have the five columns of the header.
"""
import csv
import pathlib
import re
import sys

ADDRESS = re.compile(r"\b0x(00[4-6][0-9a-fA-F]{5})\b")
CODE_DIRS = ("engine", "tools", "tests")
CODE_SUFFIXES = {".cpp", ".hpp", ".h", ".py", ".cmake"}


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    known = set()
    problems = []
    for name in ("functions.csv", "globals.csv"):
        path = root / "re" / "symbols" / name
        with path.open(encoding="utf-8", newline="") as f:
            rows = list(csv.reader(f))
        for number, row in enumerate(rows[1:], 2):
            if len(row) != len(rows[0]):
                problems.append(f"re/symbols/{name}:{number}: {len(row)} columns instead of {len(rows[0])}")
            if row:
                known.add(row[0].lower())
    for path in sorted((root / "docs" / "re").rglob("*.md")):
        known.update("0x" + a.lower() for a in ADDRESS.findall(path.read_text(encoding="utf-8")))

    cited = {}
    for directory in CODE_DIRS:
        for path in sorted((root / directory).rglob("*")):
            if path.suffix not in CODE_SUFFIXES or not path.is_file():
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                for a in ADDRESS.findall(line):
                    cited.setdefault("0x" + a.lower(), []).append(f"{path.relative_to(root)}:{number}")
    unknown = sorted(a for a in cited if a not in known)
    if problems:
        print("malformed rows (quote an evidence that holds a comma):")
        print("\n".join(problems))
    if unknown:
        print("addresses cited in the code but not in re/symbols or docs/re:")
        for a in unknown:
            print(f"{a}  {', '.join(cited[a][:3])}{' ...' if len(cited[a]) > 3 else ''}")
    if problems or unknown:
        return 1
    print(f"{len(cited)} cited addresses, {len(known)} known - ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
