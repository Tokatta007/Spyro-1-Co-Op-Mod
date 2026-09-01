#!/usr/bin/env python3
"""Locate each level overlay inside WAD.WAD and record where it lives.

The decompilation builds the 37 level overlays as separate .ovl files, but the
disc carries them inside WAD.WAD, which our pipeline otherwise copies whole.
Every overlay turns out to sit in there VERBATIM, exactly once, at a
sector-aligned offset - so patching one is a plain overwrite once you know the
offset, and no WAD format knowledge is needed.

Run this against a MATCHING build, whose overlays are byte-identical to
retail, and it writes the offsets to a JSON map. inject_overlays.py then uses
that map to place modified overlays, which cannot be located by content
because they no longer match anything on the disc.

    ./overlay_map.py <WAD.WAD> <matching-ovl-dir> <out.json>
"""
import json
import pathlib
import sys


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    wad_path, ovl_dir, out_path = (pathlib.Path(a) for a in sys.argv[1:])

    wad = wad_path.read_bytes()
    entries, missing = {}, []

    for ovl in sorted(ovl_dir.glob("*.ovl")):
        data = ovl.read_bytes()
        off = wad.find(data)
        if off < 0:
            missing.append(ovl.name)
            continue
        # A second occurrence would make the offset ambiguous, and silently
        # patching the wrong copy is exactly the kind of bug that only shows
        # up in one level, hours in.
        if wad.find(data, off + 1) >= 0:
            sys.exit(f"{ovl.name}: appears more than once in the WAD")
        if off % 2048:
            sys.exit(f"{ovl.name}: offset 0x{off:X} is not sector-aligned")
        entries[ovl.name] = {"offset": off, "size": len(data)}

    if missing:
        sys.exit("not found verbatim in the WAD (is this a matching build?):\n  "
                 + "\n  ".join(missing))

    out_path.write_text(json.dumps(entries, indent=2, sort_keys=True) + "\n")
    print(f"mapped {len(entries)} overlays -> {out_path}")


if __name__ == "__main__":
    main()
