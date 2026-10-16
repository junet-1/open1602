# SPDX-License-Identifier: GPL-3.0-or-later
# Ghidra script: export user-defined function names and comments to re/symbols/functions.csv.
#
# Works as PyGhidra (Python 3) and Jython script. Run from the Script Manager or headless:
#   analyzeHeadless <projdir> Anno1602 -process 1602.exe -noanalysis \
#       -scriptPath re/scripts/ghidra -postScript export_symbols.py re/symbols/functions.csv
#
# Only names that differ from Ghidra's defaults (FUN_xxxxxxxx) are exported, so the CSV stays
# reviewable. Existing columns 'confidence' and 'evidence' are preserved for known addresses.
#@category Open1602

import csv
import os

from ghidra.program.model.symbol import SourceType

args = getScriptArgs()
out_path = args[0] if args else askFile("Export to", "Save").getAbsolutePath()

existing = {}
if os.path.exists(out_path):
    with open(out_path) as f:
        for row in csv.DictReader(f):
            existing[row["address"].lower()] = row

rows = []
fm = currentProgram.getFunctionManager()
for func in fm.getFunctions(True):
    if func.getSymbol().getSource() == SourceType.DEFAULT:
        continue
    addr = "0x%08x" % func.getEntryPoint().getOffset()
    old = existing.get(addr, {})
    rows.append({
        "address": addr,
        "name": func.getName(True),
        "module": old.get("module", ""),
        "confidence": old.get("confidence", "medium"),
        "evidence": old.get("evidence", func.getComment() or ""),
    })

with open(out_path, "w") as f:
    writer = csv.DictWriter(f, fieldnames=["address", "name", "module", "confidence", "evidence"],
                            lineterminator="\n")
    writer.writeheader()
    for row in sorted(rows, key=lambda r: r["address"]):
        writer.writerow(row)

print("exported %d functions to %s" % (len(rows), out_path))
