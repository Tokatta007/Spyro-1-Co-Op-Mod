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

    bad += check_no_raw_addresses(names_h.parent.parent / "src" / "coop")
    if bad:
        sys.exit("FAIL")


def blank_comments(src):
    """Replace comments and string bodies with spaces, keeping line numbers.

    Worth doing properly: a line-by-line approximation of this missed six
    live writes during the port, each aimed at a retail address that the
    ported build no longer uses - including one that zeroed the pad's button
    state somewhere else entirely.
    """
    out, i, n = [], 0, len(src)
    pad = lambda s: "".join(c if c == "\n" else " " for c in s)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(pad(src[i:j])); i = j
        elif c == "/" and i + 1 < n and src[i + 1] == "/":
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(pad(src[i:j])); i = j
        elif c in "\"'":
            q = c; j = i + 1
            while j < n and src[j] != q:
                j += 2 if src[j] == "\\" else 1
            out.append(src[i:j + 1]); i = j + 1
        else:
            out.append(c); i += 1
    return "".join(out)


def check_no_raw_addresses(coop_dir):
    """No raw GAME address may survive in the ported code.

    Our code lengthens the executable, so the game's .data and .bss no longer
    sit at their retail addresses - and they do not shift by the same amount
    as each other, so no uniform correction exists. A leftover literal reads
    and writes the wrong memory silently. BIOS scratch (below 0x80010000) is
    exempt: that region is ours and does not move.
    """
    found = 0
    for src in sorted(pathlib.Path(coop_dir).glob("*.c")):
        code = blank_comments(src.read_text())
        lines = src.read_text().splitlines()
        for m in re.finditer(r"0x800[0-7][0-9A-Fa-f]{4}", code):
            if int(m.group(0), 16) < 0x80010000:
                continue
            ln = code[:m.start()].count("\n")
            print(f"  RAW ADDRESS {src.name}:{ln + 1}  {m.group(0)}  "
                  f"{lines[ln].strip()[:60]}")
            found += 1
    if found:
        print(f"FAIL - {found} raw game address(es) left in ported code")
    else:
        print("PASS - no raw game addresses in ported code")
    return found


if __name__ == "__main__":
    main()
