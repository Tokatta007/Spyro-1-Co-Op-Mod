#!/usr/bin/env python3
"""Verify every name the port binds resolves to the address the mod expects.

The mod was written against a binary and knows each game symbol by ADDRESS
(mod/projects/ntsc/symbols.ld). The port binds those names to the
decompilation's names (coop_names.h). If a binding points at the wrong symbol,
nothing fails to compile - the mod simply reads and writes the wrong memory,
usually symmetrically, usually without crashing. That is the exact shape of the
g_PadBackup bug that hid for months, and of the vsync-counter bug that put two
stores per frame onto the portal-wait flag.

    ./check_names.py <symbols.ld> <coop_names.h> <psx.map from a MATCHING build>

The map must come from a matching build: only there do the decomp's symbols sit
at the addresses the mod's own table records.
"""
import pathlib
import re
import sys


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    ld, names_h, mapf = (pathlib.Path(a) for a in sys.argv[1:])

    want = {m.group(1): int(m.group(2), 16) for m in
            re.finditer(r"^(\w+)\s*=\s*(0x[0-9a-fA-F]+);", ld.read_text(), re.M)}

    addr_of = {}
    for m in re.finditer(r"^\s+(0x[0-9a-f]{8})\s+(\w+)\s*$", mapf.read_text(), re.M):
        addr_of.setdefault(m.group(2), int(m.group(1), 16))

    checked = bad = skipped = 0
    for m in re.finditer(r"^#define\s+(\w+)\s+(\w+)\s*(?:/\*|$)",
                         names_h.read_text(), re.M):
        ours, theirs = m.group(1), m.group(2)
        if ours not in want:
            continue
        if theirs not in addr_of:
            skipped += 1        # accessor macros and casts land here
            continue
        checked += 1
        if addr_of[theirs] != want[ours]:
            bad += 1
            print(f"  MISMATCH {ours}: expected 0x{want[ours]:08X}, "
                  f"but {theirs} is at 0x{addr_of[theirs]:08X}")

    print(f"checked {checked} bindings, {skipped} not plain renames")
    if bad:
        sys.exit(f"FAIL - {bad} binding(s) point at the wrong symbol")
    print("PASS - every plain rename resolves to the address the mod expects")


if __name__ == "__main__":
    main()
