# Building the mod inside the decompilation

Findings from 2026-08-31. `reference/spyro-1` is untracked and has its own
upstream, so anything we do there must be kept here as a patch or it is lost.

## The decomp builds, and it matches

All 38 targets: `PSX.EXE` plus every one of the 37 level overlays. And
`build/PSX.EXE` has SHA-1 `84e3728ab94720d0873e2514adf4aade4935e0c5` — the
same as our `SCUS_942.28`. It reproduces our game exactly.

Setup, of which only step 4 is in their README:

1. **PSYQ headers.** `psyq/` ships empty. Fetch
   `http://psx.arthus.net/sdk/Psy-Q/psyq-4.7-converted-full.7z` and copy its
   `include/` and `lib/` into `reference/spyro-1/psyq/`. Needs
   `brew install sevenzip`. These are compile-time SDK headers and have
   nothing to do with `Roms/BIOS/SCPH1001.BIN`, the console's firmware.
2. **`git submodule update --init`** — `tools/maspsx` was empty in our clone.
3. **`docker build --platform linux/amd64 -t s1_dev_env .`** — not the bare
   command in their `docker_env.sh`. The matching compiler
   `tools/gcc2.7.2/cc1` is a 32-bit x86 binary, which an arm64 image cannot
   run and an amd64 one can.
4. `docker run --rm --platform linux/amd64 -v "$(pwd)":/s1 s1_dev_env \
    bash -c 'make -j4 all'`

## THE BLOCKER — SOLVED AND USER-TESTED 2026-08-31

The pins below are now expressed relative to the overlay base, and a disc
built that way **played three levels with no crashes, glitches or visible
change**. The executable can grow.

`overlays.ld` pinned 44 undecompiled overlay functions to absolute retail
addresses. The game itself does not need that: `overlay_pointers.c` sets
`g_OverlaySpacePointer = &main_BSS_END`, a linker symbol, so overlays follow
when the executable grows. Only the pins did not.

Rewriting each as `main_BSS_END + <offset from retail's 0x8007AA38>`:

- **byte-neutral when nothing moves** — 38 of 38 still match;
- **tracks correctly when it does** — with code added, a pin that was
  `0x80082068` resolved to `0x800820A8`, exactly the `0x40` the base shifted.

**But rebuilt overlays must then be injected into `WAD.WAD`**, or the game
loads retail overlays built for the old base to the new address and each level
breaks as you enter it. `mod/scripts/overlay_map.py` records where each
overlay sits (run against a matching build, since a rebuilt one matches
nothing on the disc); `mod/scripts/inject_overlays.py` writes them back and
refuses any that outgrow their slot. Offsets are kept in
`docs/decomp/overlay_offsets.json`.

Original diagnosis follows.

## THE ORIGINAL BLOCKER: adding code to the executable moves the overlays

`add-our-code.patch` and `coop.c` are the experiment. They add one small
function, place its object last in `.text` so no game code moves, and call it
from `main()`. **It compiles and links cleanly, and it is broken.**

The executable's file size does not change — sector padding absorbs it — but
the addresses do:

| | retail | +64 bytes of ours |
| --- | --- | --- |
| `main_BSS_END` | `0x8007AA38` | `0x8007AA78` |

`.bss` is placed at `main_SDATA_END`, so growing `.text` pushes `.rdata`,
`.data` and `.sdata` along and `.bss` with them. `overlays.ld` then places
every overlay at `main_BSS_END` — so all 37 shift by the size of our code.
Confirmed: after the change, all 37 overlay checksums fail while the
executable stays 417,792 bytes.

That would be harmless if overlays were fully relocatable. They are not:
`overlays.ld` pins undecompiled overlay functions to **absolute retail
addresses** (`func_level_0_80082068 = 0x80082068;`, lowest `0x8007AC8C`).
Move the base and every call to a pinned function lands short. The link
succeeds and the game breaks at the first level load — a silent failure.

## The pins are small, and the flight levels have none

There are **44 pins across 8 overlays**, all of one shape
(`name = 0xADDRESS;`).

**THESE ARE NOT A DECOMP BUG, and we should not offer a "fix" upstream.**
They are ordinary scaffolding. `src/overlay_pointers.c` in the executable
holds pointers into each level's overlay
(`g_Buffers.m_CopyBuf = func_level_0_80082068;`), and where that overlay
function is not yet decompiled its retail address is hardcoded so the rest can
link. In a matching build nothing moves, so a fixed address is correct and
costs nothing — it works exactly as intended for the project's own goal. It
constrains only someone who wants to GROW the binary, which is our aim and not
theirs, and it dissolves by itself as overlays get decompiled.

If we ever need them relocatable, that is a change we make in our own branch
for our own reasons.

More immediately useful: the pinned overlays are levels 0, 1, 2, 3, 4, 7, 9
and 99. **The five flight levels (5, 11, 17, 23, 29) carry no pins at all.**
So their overlays can be edited and rebuilt freely, provided the overlay BASE
does not move — which it does not, as long as our own code stays out of
`.text`. The main executable reaches overlay code through pointers
(`g_UpdateMoby` is a function pointer, and `overlay_pointers.o` holds the
entry table), so nothing calls into them at a fixed address.

**That makes the flight-HUD bug fixable without solving the pin problem.**

## What this means

The decompilation gives us **symbols, direct calls, and the ability to edit
overlays** — the last of which our current architecture cannot do at all, and
which is the only route to the flight-HUD bug. It does **not** hand us free
space.

Options, none yet tried:

- **Keep our code in BIOS scratch**, as the mod does today, but built from
  source with a linker region. `main_BSS_END` never moves, so overlays are
  safe. Lowest risk, and it keeps every benefit except space.
- **Shrink the game to pay for our code**, keeping `main_SDATA_END` exactly
  where retail has it by padding. The modern compiler frees ~22 KB
  (`MODERN_COMPILER=1 NEW_PSYQ=1 make non_matching` → 395,264 bytes vs
  417,792). Untested, and it needs PSYQ 4.7's libraries linked in, which are
  not the libraries the game shipped with.
- **Make the pins relocatable in our own branch** — define them relative to
  the overlay base rather than absolutely. Mechanical, 44 lines. Ours to do
  for our own reasons; see above for why it is not an upstream matter.
- **Wait**, since the pins disappear as overlays get decompiled.

## Getting a modified overlay onto the disc — SOLVED, and easier than feared

Our disc pipeline copies `WAD.WAD` as one 110 MB blob and knows nothing about
its contents, so changing an overlay looked like it needed the WAD format
reverse-engineered. It does not.

**Every overlay appears verbatim, exactly once, at a sector-aligned offset.**
Measured against a matching build:

| overlay | size | offset in WAD.WAD |
| --- | --- | --- |
| `level_0_artisans_home_code.ovl` | 57,344 | `0x7F2800` |
| `level_5_artisans_sunny_flight_code.ovl` | 43,008 | `0x16A8000` |
| `level_11_peace_keepers_night_flight_code.ovl` | 38,912 | `0x258F800` |

So the procedure is: build matching to get the retail bytes, search `WAD.WAD`
for them to find the offset, then write the modified overlay there. No format
knowledge required, and the search doubles as a check that we are patching the
right thing.

**Budget for growing a flight overlay in place** — trailing padding, which is
what an edit can expand into before it would disturb anything after it:

| flight overlay | size | free |
| --- | --- | --- |
| night flight | 38,912 | 1,604 |
| crystal flight | 40,960 | 1,700 |
| sunny flight | 43,008 | 868 |
| wild flight | 40,960 | 496 |
| icy flight | 40,960 | 256 |

Icy flight is the tight one at 256 bytes. A per-viewport HUD reposition should
fit, but it is worth checking the worst case first rather than last.

## Booting a decomp-built disc — the procedure

Our own pipeline packs the disc; the decomp only produces `PSX.EXE` and the
`.ovl` files. To make a disc from a decomp build:

1. Stage the executable: copy `reference/spyro-1/build/PSX.EXE` over
   `mod/projects/ntsc/build/rom/SCUS_942.28`.
2. **Regenerate the disc XML** — do not reuse our mod's. `spyro1-coop.xml`
   carries sector padding computed for OUR executable, which is 5 sectors
   larger than retail. `create_mkpsxiso_xml.py` compares `rom/<file>` against
   `build/rom/<file>` and adjusts the `<dummy sectors>` count so every file
   after it keeps its LBA. With a retail-sized executable it restores the
   retail figure of 5921. Getting this wrong shifts every file after the
   executable and the game reads the wrong sectors.
3. `mkpsxiso -y -lba build/decomp.lba -c ... -o ... build/decomp.xml`
4. Restore our own `SCUS_942.28` afterwards.

**TRAP, cost half an hour: mkpsxiso does NOT zero dummy sectors, so writing
over an existing image leaves stale bytes from the previous layout.** A first
attempt with the wrong padding left ~1,035 bytes of debris in the gap before
`PETEXA0.STR` that survived the corrected rebuild. It looked like a layout
error and was not. **Delete the .bin before repacking**, and confirm with the
`.lba` file that the LBAs are what you expect.

## Verified end to end, 2026-08-31

Changed one constant (`initialization.c`, starting lives 4 -> 9) and rebuilt.

- The executable differs from the matching build by **exactly one byte**
- All **37 overlays stay byte-identical** to retail
- The disc differs from retail in **two clusters only**: 1,185 bytes of ISO
  metadata, which is *exactly* what our known-working v0.1 disc also differs
  by, and 37 bytes at the executable, being our one byte plus its sector's
  error-correction data

So the chain source -> executable -> disc is sound, and our v0.1 disc is the
control that proves the metadata difference is harmless.

## The port — complete, 2026-08-31

All 3,630 lines of the mod build inside the decompilation, all 24 hooks are
wired, and the disc packs with retail's LBAs intact (`WAD.WAD` 37,
`SCUS_942.28` 53875, `PETEXA0.STR` 60000). The executable is 430,080 bytes,
six sectors larger than retail, absorbed by the dummy padding.

### What the hooks became

Most are now ordinary calls in decompiled C: the render entry in `main.c`,
four inside `GamestateUpdate`, the portal draw, the pause menu's six rows and
its two box tests, the sound distance, the pad-callback install. Three stay in
assembly because their callers are — the two death sites in `pete`, and the
sparkle ageing in `r_particles`. The retail Baruti crash is fixed where it
lives, in `math.s`.

Two techniques retire completely: the **boot stub and payload arithmetic**, and
the **entry patches** on the collision guards, which were the most delicate
thing in the mod.

### THE TRAP THAT WOULD HAVE BROKEN IT SILENTLY

The mod carried its per-player swap tables as **16-bit offsets from
0x80075000**, packed that way to save bytes, plus about 35 other raw
addresses. Adding our code lengthens the executable, so the game's `.data` and
`.bss` no longer sit where they did — **and they do not shift by the same
amount as each other**, because our text, rodata and data each push the
sections after them. There is no uniform correction. An offset table would
have swapped the wrong memory, symmetrically, with nothing crashing: exactly
the shape of the `g_PadBackup` bug that survived months.

Every address is now a real symbol and the tables hold pointers. Two of the
conversions confirmed old findings independently: `0x80075760` resolves to
**`g_UnprocessedFrames`**, not the frame counter open-spyro named it, and the
`g_PadBackup` entry corrected in August lands exactly on `g_PadBackup`.

### Build-system fixes made along the way

- **`pipefail`**. Every compile is a pipeline and make only sees the last
  command, so a compiler error was silently swallowed: `as` succeeded on empty
  input and produced ~800-byte objects. Our files "compiled" while producing
  nothing.
- **`src/coop/` uses the modern compiler at `-G0`**, as the standalone mod
  does. The matching compiler is GCC 2.7.2, which is C89 and rejects
  declarations after statements.
