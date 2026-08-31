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

## THE BLOCKER: adding code to the executable moves the overlays

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
