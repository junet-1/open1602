# SPDX-License-Identifier: GPL-3.0-or-later
# Ghidra script: apply re/symbols/functions.csv and globals.csv to the current program.
#
# The CSV files in the repository are the source of truth; a Ghidra database can always be
# recreated from 1602.exe + these files. Run after auto-analysis:
#   analyzeHeadless <projdir> Anno1602 -import 1602.exe \
#       -scriptPath re/scripts/ghidra -postScript import_symbols.py re/symbols
#@category Open1602

import csv
import os

from ghidra.program.model.symbol import SourceType

args = getScriptArgs()
sym_dir = args[0] if args else askDirectory("Symbol directory", "Select").getAbsolutePath()


def addr(text):
    return toAddr(int(text, 16))


count = 0
path = os.path.join(sym_dir, "functions.csv")
with open(path) as f:
    for row in csv.DictReader(f):
        a = addr(row["address"])
        func = getFunctionAt(a) or createFunction(a, None)
        if func is None:
            print("WARN: could not create function at %s" % row["address"])
            continue
        func.setName(row["name"].split("::")[-1], SourceType.USER_DEFINED)
        note = "[%s] %s" % (row.get("confidence", ""), row.get("evidence", ""))
        func.setComment(note)
        count += 1

path = os.path.join(sym_dir, "globals.csv")
if os.path.exists(path):
    with open(path) as f:
        for row in csv.DictReader(f):
            createLabel(addr(row["address"]), row["name"], True, SourceType.USER_DEFINED)
            setEOLComment(addr(row["address"]), "[%s] %s" % (row.get("confidence", ""), row.get("evidence", "")))
            count += 1

print("applied %d symbols from %s" % (count, sym_dir))
