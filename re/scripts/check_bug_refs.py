#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that every bug id referenced in the code (e.g. "Fix S-002") exists in docs/re/bugs.md.

Usage: python3 re/scripts/check_bug_refs.py [repo root]
Exit code 1 lists unknown ids (ADR 0006). The catalog lives in the private repository open1602-re,
so run this locally with docs/re linked in (CONTRIBUTING.md); public CI cannot see it.
"""
import pathlib
import re
import sys

ID = re.compile(r"\b([TSLMUD]-\d{3})\b")
CODE_DIRS = ("engine", "tools", "tests")
CODE_SUFFIXES = {".cpp", ".hpp", ".h", ".py", ".cmake"}


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    catalog = (root / "docs" / "re" / "bugs.md").read_text(encoding="utf-8")
    # Defined ids are those that start a table row: "| S-002 |"
    known = set(re.findall(r"^\|\s*([TSLMUD]-\d{3})\s*\|", catalog, flags=re.MULTILINE))
    unknown = []
    references = 0
    for directory in CODE_DIRS:
        for path in sorted((root / directory).rglob("*")):
            if path.suffix not in CODE_SUFFIXES or not path.is_file():
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                for bug_id in ID.findall(line):
                    references += 1
                    if bug_id not in known:
                        unknown.append(f"{path.relative_to(root)}:{number}: {bug_id}")
    if unknown:
        print("unknown bug ids (add them to docs/re/bugs.md):")
        print("\n".join(unknown))
        return 1
    print(f"{references} references, {len(known)} catalogued ids - ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
