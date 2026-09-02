#!/bin/bash
#
# Build the co-op mod's release disc and patch, from the decompilation.
#
# The mod has two constructions. The STANDALONE one patches 24 instructions
# into the retail executable and is what `make` in mod/projects/ntsc builds.
# This script builds the other: the mod compiled inside TheMobyCollective's
# decompilation, which is the one that ships, because it is the only one that
# can rebuild the level overlays.
#
# The steps do not fit a Makefile cleanly - the decomp builds in Docker, its
# overlays have to be spliced into WAD.WAD, and the disc XML must be
# regenerated for the executable's size - so they live here, written down,
# rather than in anyone's memory.
#
# PREREQUISITES, none of which this script can install for you:
#   - Docker running, with the s1_dev_env image built:
#       cd reference/spyro-1 && docker build --platform linux/amd64 -t s1_dev_env .
#   - PSYQ headers in reference/spyro-1/psyq/     (see docs/decomp/README.md)
#   - the maspsx submodule checked out:  git submodule update --init
#   - your own retail disc at mod/projects/ntsc/disc/spyro1.bin
#   - mkpsxiso and xdelta3 on PATH
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DECOMP="$ROOT/reference/spyro-1"
NTSC="$ROOT/mod/projects/ntsc"
NAME="spyro1-coop"

say() { printf '\n=== %s ===\n' "$1"; }

[ -d "$DECOMP" ]              || { echo "missing $DECOMP"; exit 1; }
[ -f "$NTSC/disc/spyro1.bin" ] || { echo "missing your retail disc at $NTSC/disc/spyro1.bin"; exit 1; }
[ -d "$DECOMP/psyq/include" ] || { echo "missing PSYQ headers - see docs/decomp/README.md"; exit 1; }

say "building the mod inside the decompilation"
# MODERN_COMPILER shrinks the game's own code, freeing room below retail's
# boundary. The overlays stay on the matching compiler - see the Makefile.
docker run --rm --platform linux/amd64 -v "$DECOMP":/s1 s1_dev_env \
    bash -c 'MODERN_COMPILER=1 make -j4 non_matching' >/dev/null
echo "  PSX.EXE: $(wc -c < "$DECOMP/build/PSX.EXE" | tr -d ' ') bytes"

say "checking the port's bindings"
if [ -f /tmp/psx_matching.map ]; then
    python3 "$ROOT/mod/scripts/check_names.py" \
        "$NTSC/symbols.ld" "$DECOMP/include/coop_names.h" /tmp/psx_matching.map
else
    echo "  SKIPPED - no matching-build map at /tmp/psx_matching.map."
    echo "  Produce one with:  git checkout main && make all  in the decomp,"
    echo "  then copy build/psx.map there. The check verifies every ported"
    echo "  name still resolves to the address the mod expects - a wrong"
    echo "  binding does not fail to compile, it reads the wrong memory."
fi

say "splicing the rebuilt overlays into WAD.WAD"
python3 "$ROOT/mod/scripts/inject_overlays.py" \
    "$NTSC/build/rom/WAD.WAD" "$DECOMP/build/wad" \
    "$ROOT/docs/decomp/overlay_offsets.json" /tmp/WAD_release.WAD

say "packing the disc"
cd "$NTSC"
cp build/rom/SCUS_942.28 /tmp/SCUS_standalone.bak
cp "$DECOMP/build/PSX.EXE" build/rom/SCUS_942.28
cp /tmp/WAD_release.WAD build/rom/WAD.WAD
# The XML's sector padding is computed from the executable's size, so it MUST
# be regenerated: reusing one built for a different executable shifts every
# file after it and the game reads the wrong sectors.
python3 ../../scripts/create_mkpsxiso_xml.py spyro1.xml build/release.xml >/dev/null
# mkpsxiso does not zero dummy sectors, so an existing image leaves debris.
rm -f "build/disc/$NAME.bin" "build/disc/$NAME.cue"
mkpsxiso -y -lba build/release.lba -c "build/disc/$NAME.cue" \
         -o "build/disc/$NAME.bin" build/release.xml | tail -1
cp /tmp/SCUS_standalone.bak build/rom/SCUS_942.28
cp rom/WAD.WAD build/rom/WAD.WAD

say "building the patch, and verifying it round-trips"
mkdir -p build/release
xdelta3 -e -9 -f -s disc/spyro1.bin "build/disc/$NAME.bin" "build/release/$NAME.xdelta"
xdelta3 -d -f -s disc/spyro1.bin "build/release/$NAME.xdelta" build/release/roundtrip.bin
built=$(shasum "build/disc/$NAME.bin" | cut -d' ' -f1)
back=$(shasum build/release/roundtrip.bin | cut -d' ' -f1)
rm -f build/release/roundtrip.bin
if [ "$built" != "$back" ]; then
    echo "  FAIL - the patch does not reproduce the build"
    echo "         built:   $built"
    echo "         patched: $back"
    exit 1
fi
cp "build/disc/$NAME.cue" "build/release/$NAME.cue"

say "done"
echo "  disc SHA-1 : $built"
echo "  source disc: $(shasum disc/spyro1.bin | cut -d' ' -f1)"
echo "  patch      : build/release/$NAME.xdelta ($(wc -c < "build/release/$NAME.xdelta" | tr -d ' ') bytes)"
echo "  cue        : build/release/$NAME.cue"
