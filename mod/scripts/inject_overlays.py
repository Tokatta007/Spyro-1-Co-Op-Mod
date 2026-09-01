#!/usr/bin/env python3
"""Write rebuilt level overlays into a copy of WAD.WAD.

Needed whenever the executable changes size. The game loads overlays to
`&main_BSS_END` (overlay_pointers.c), so growing the executable moves where
they land - and an overlay built for the old base, loaded at a new one, is
broken in ways that only appear when you enter that level.

Offsets come from overlay_map.py, run against a matching build. They cannot be
found by content here, because a rebuilt overlay matches nothing on the disc.

    ./inject_overlays.py <in-WAD.WAD> <new-ovl-dir> <map.json> <out-WAD.WAD>
"""
import json
import pathlib
import sys


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    wad_in, ovl_dir, map_path, wad_out = (pathlib.Path(a) for a in sys.argv[1:])

    entries = json.loads(map_path.read_text())
    wad = bytearray(wad_in.read_bytes())
    written = unchanged = 0

    for name, info in sorted(entries.items()):
        src = ovl_dir / name
        if not src.exists():
            sys.exit(f"{name}: missing from {ovl_dir}")
        data = src.read_bytes()
        off, slot = info["offset"], info["size"]

        # The slot is what retail occupied. Growing past it would run into
        # whatever follows, so refuse rather than corrupt the neighbour.
        if len(data) > slot:
            sys.exit(f"{name}: {len(data):,} bytes will not fit its "
                     f"{slot:,}-byte slot ({len(data) - slot:,} over)")

        if wad[off:off + len(data)] == data:
            unchanged += 1
            continue
        wad[off:off + len(data)] = data
        # Anything between a shorter overlay and the end of its slot is left
        # exactly as it was, so the disc keeps its original padding.
        written += 1

    wad_out.parent.mkdir(parents=True, exist_ok=True)
    wad_out.write_bytes(bytes(wad))
    print(f"{written} overlay(s) written, {unchanged} already current "
          f"-> {wad_out}")


if __name__ == "__main__":
    main()
