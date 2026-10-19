#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Starts the game with an unknown option: it must load (SDL3 found, on Windows SDL3.dll next to
# the program) and answer with its usage text and exit code 2. Original data is not needed.
set -u
game="$1"
if [[ "${RUNNER_OS:-}" == "Windows" && ! -f "$(dirname "$game")/SDL3.dll" ]]; then
    echo "SDL3.dll is missing next to $game" >&2
    exit 1
fi
"$game" --no-such-option 2> usage.txt
code=$?
head -n 1 usage.txt
if [[ $code -ne 2 ]] || ! grep -q '^usage: open1602' usage.txt; then
    echo "the game did not start as expected (exit code $code)" >&2
    exit 1
fi
