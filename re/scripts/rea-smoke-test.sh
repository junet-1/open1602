#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Feasibility test: can rea (https://github.com/morluto/rea) analyse 1602.exe (PE32, x86, MSVC 6)
# through its Ghidra backend?
#
#   ANNO1602_DIR="$HOME/Games/Heroic/Anno 1602" re/scripts/rea-smoke-test.sh
#
# The rea documentation lists Linux x64 as supported host and Ghidra 12.1.x as backend,
# but does not explicitly mention 32-bit PE targets. This script checks exactly that and
# compares the result with addresses already verified via objdump (see re/symbols/functions.csv).
#
# Output is written to re/notes/rea-smoke-test-<date>.log. The log contains decompiled code of the
# original and is therefore git-ignored; record the conclusions in re/notes/rea-smoke-test-result.md.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EXE="${1:-${ANNO1602_DIR:-}/1602.exe}"
LOG="$ROOT/re/notes/rea-smoke-test-$(date +%Y%m%d-%H%M%S).log"
REA="${REA:-rea}"

mkdir -p "$ROOT/re/notes"
exec > >(tee "$LOG") 2>&1

step() { printf '\n==== %s\n' "$*"; }
run()  { printf '$ %s\n' "$*"; "$@"; local rc=$?; printf '[exit %d]\n' "$rc"; return 0; }

step "Environment"
run uname -a
run node --version
run java -version
echo "GHIDRA_INSTALL_DIR=${GHIDRA_INSTALL_DIR:-<unset>}"
if [[ ! -f "$EXE" ]]; then
    echo "1602.exe not found: '$EXE' - set ANNO1602_DIR or pass the path as first argument." >&2
    exit 1
fi
run sha256sum "$EXE"

if ! command -v "$REA" >/dev/null 2>&1; then
    echo "rea not found on PATH, using npx (rea-agents)"
    REA_CMD=(npx -y rea-agents@latest)
else
    REA_CMD=("$REA")
fi

step "rea version / help"
run "${REA_CMD[@]}" --version
run "${REA_CMD[@]}" --help

# Known facts (verified with objdump, see docs/re/1602exe-overview.md):
#   0x004081D0  WinMain
#   0x00494870  game initialisation (loads stadtfld.col, haeuser.cod, figuren.cod)
#   0x0048F8D0  Text.cod loader, contains the byte negation loop at 0x0048F94D
#   0x0049C4A4  string "haeuser.cod"
P=(--provider ghidra --format json)

step "0) Generic analysis (as suggested by 'rea setup')"
run "${REA_CMD[@]}" analyze "$EXE"

step "1) Function lookup: WinMain @ 0x4081d0"
run "${REA_CMD[@]}" function "$EXE" --procedure=0x4081d0 "${P[@]}"

step "2) Decompile Text.cod loader @ 0x48f8d0 (expect a loop negating every byte)"
run "${REA_CMD[@]}" decompile "$EXE" --procedure=0x48f8d0 "${P[@]}"

step "3) Cross references to string 'haeuser.cod' @ 0x49c4a4 (expect a call site in 0x494870)"
run "${REA_CMD[@]}" xrefs "$EXE" --address=0x49c4a4 "${P[@]}"

step "4) Pattern search 'stadtfld'"
run "${REA_CMD[@]}" search "$EXE" --pattern=stadtfld "${P[@]}"

step "5) Instruction at 0x48f950 (expect: NEG CL)"
run "${REA_CMD[@]}" inspect-native-instruction "$EXE" 0x48f950 --provider ghidra

step "Done"
echo "Log written to $LOG"
echo "If steps 1-5 fail with an 'unsupported format/architecture' error, rea cannot handle PE32 yet;"
echo "fallback: Ghidra headless with re/scripts/ghidra/*.py (see docs/re/README.md)."
