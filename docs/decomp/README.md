# Building the mod inside the decompilation

Findings from 2026-08-31. `reference/spyro-1` is untracked and has its own
upstream, so the mod's work there lives on a FORK, not in this repository and
not as patches: **https://github.com/Tokatta007/spyro-1, branch
`port-shrink`** (remote `fork`). Push to it after any change there. The local
clone is not a backup — it is gitignored, and until 2026-09-02 the entire port
existed nowhere else.

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

## THE REAL CEILING: THE EXECUTABLE CANNOT GROW (2026-08-31)

**Proven by experiment, and it is a harder limit than the overlay pins.**

A build of VANILLA Spyro - no mod code at all, just `. = . + 0x3000;` in
`.text` so `main_BSS_END` moves by the 12 KB our code moves it - **breaks the
intro cutscene in exactly the way the ported mod does**: the world renders,
the actors never appear. User-confirmed.

So the mod's hooks are innocent. What breaks is growing the executable.

**Why.** The game fills RAM from both ends. The executable and its `.bss` grow
UP, then the level overlay and level data; from `0x80200000` the stack, shared
animations, particles, ordering tables and poly buffers grow DOWN
(`4BEF8.c:53`, `initialization.c:303`). Nothing in the game hardcodes a low
address - the top-down side is all computed from `0x80200000` - so this is not
a stale pointer. It is that the GAP between the two shrinks by exactly as much
as the executable grows, and the intro cutscene needs more of it than is then
left. Its actors end up positioned and queued for drawing (measured: 16
actors, 8 alive, 5 queued) with their geometry corrupt.

**What this means for the port.** Fixing the overlay pins was necessary but
NOT sufficient. There are two independent constraints on growing the
executable, and only one is solved:

  1. overlays.ld's absolute pins - fixed, see below;
  2. the bottom-up/top-down gap - NOT fixed, and not fixable by relocation.

**So our code cannot live in the executable.** It goes where the standalone
mod puts it: BIOS scratch RAM, below the game and outside both growth
directions. **DONE AND USER-CONFIRMED 2026-09-01** - the intro plays and the
mod works. The port keeps everything else it gained (real symbols, direct
calls instead of 24 patched instructions, the ability to rebuild overlays) but
not free space.

### How the mod is delivered now

Three chunks are appended to the executable. The BIOS loads them to
`0x80075800`, which is inside `.bss` - safe because the header declares
**`b_size = 0`**, so the BIOS zeroes nothing and the game clears `.bss` itself
in its own startup, which runs after us. `Sp1x2Loader` is the executable's
entry point (`pc0`), lives in the sector padding above `main_SDATA_END`,
clears the mod's scratch, copies the three chunks out, flushes the I-cache and
jumps to the game's real entry. It is then overwritten by `.bss`, which is
fine - it runs once.

**`.text` is padded back to retail's length** so every section after it keeps
its address. All three boundaries match retail exactly: `main_TEXT_END`
`0x8006BBE0`, `main_SDATA_END` `0x80075640`, `main_BSS_END` `0x8007AA38`.

### FOUR TRAPS, all of which cost a build

1. **Pad in `.text`, never after `.sdata`.** Padding between `.sdata` and
   `.sbss` pushed the game's `$gp`-relative globals out of the +/-32KB window
   and the link died with dozens of `relocation truncated to fit
   R_MIPS_GPREL16`.
2. **Inside a section description GNU ld treats `.` as an offset from the
   section start, not an absolute address.** `. = 0x8006bbe0` silently
   produced addresses 0x7FFF0000 low, and the overlays then failed to link
   with `R_MIPS_26` overflows. Write `. = 0x8006bbe0 - 0x80010000`.
3. **`t_size` must be a whole number of 2048-byte sectors** or the BIOS hangs
   at the boot logo with no error - the same trap that cost a boot failure in
   August. The payload is padded to `0x3000` so `t_size` is `0x68800` = 209.00.
4. **Anything that only runs at boot belongs in the loader, not in scratch.**
   `Sp1x2Init` moved there and kept 36 bytes out of a region with a dozen to
   spare.

### Space, as it now stands

The mod is 11,108 bytes against 11,136 of usable scratch, so the packing is at
function granularity and the margin is: **LOADER ~28 bytes, BIOS2 ~8, BIOS2B
~16.** Anything new needs space found first. The way to get it is to make the
executable SMALLER and spend the slack while padding `.text` back to retail's
length - which is what the modern compiler's ~22 KB would actually buy.

The way to get space back later is to make the executable SMALLER and spend
the slack while padding `main_SDATA_END` to its retail value, so the layout
never moves. That is what the modern compiler's ~22 KB would actually buy, and
it is a different argument from the one made for it earlier.

## THE OVERLAY-PIN BLOCKER — SOLVED AND USER-TESTED 2026-08-31

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

## How to make the executable smaller — MEASURED 2026-09-01

Space now comes from making the game's own code shorter and spending the slack
below retail's `.text` boundary, so the layout never moves. Measured options:

| lever | frees | state |
| --- | --- | --- |
| **Modern-compile the executable only** | **17,432 bytes** | links; never booted |
| Remove the crash demo | ~176 bytes | needs a `jr ra` stub per entry |
| Trimming our own code | tens of bytes | last resort, and the trap below |

**The big lever is the modern compiler, applied to the EXECUTABLE ONLY.** The
overlays must stay on the matching compiler: `overlays.ld` pins undecompiled
functions at offsets INSIDE each overlay, so recompiling one moves its own code
out from under its own pins. Nothing calling into the executable has that
problem - those references are all by symbol. The Makefile already supports
per-directory flags, and `shrink-test` in the reference clone does exactly
this.

17,432 bytes is more than the mod's 11,108, so the mod could live inside the
executable's slack and leave all 11,136 bytes of BIOS scratch free - about
2.5x the room we have today.

**What it costs:** the executable stops matching, so its half of the 38-file
verification goes (the 37 overlays still match, which is worth keeping). And
it is untested - it links, nobody has booted it.

**A related find:** the modern compiler rejected three of our functions that
had no prototype at all. In the standalone mod each is reached by a patched
instruction, so nothing ever called them from C, and GCC 2.7.2 waves implicit
declarations through. Calling a function with no prototype is only safe by
luck about argument passing. Fixed on `port`.

**And the trap worth stating plainly:** do NOT trim our own code for bytes.
This project has a scar from deleting a null check to save four bytes on a
"provably set" argument where the proof was wrong. Space should come
structurally - from the executable, not from our safety checks.
