# Spyro 1 Co-Op Mod

Split-screen co-op for **Spyro the Dragon** (PlayStation, NTSC).

As it stands: two dragons, two controllers, two cameras, a per-player HUD,
enemies and pickups assigned to whichever player is nearest, and a Multiplayer
settings page inside the game's own pause menu. **The goal is up to four
players in split-screen.**

Built by studying and porting the architecture of
[Spyromain's Spyro2x2](https://github.com/Spyromain/Spyro2x2), an equivalent
mod for *Ripto's Rage*. MIT licensed, and that attribution is preserved.

---

## What works today

- Two players, independent movement, cameras and Sparx
- Horizontal or vertical split, three view-fit modes, optional 16:9 widescreen
- Drop-in / drop-out: switch between one and two players from the pause menu
- Enemies, gems and fodder react to the nearest player
- Shared lives, with individual death and respawn
- Dragon rescues, the balloonist, portals and level transitions all handled

[BUGS.md](BUGS.md) lists what is still open, including two parked issues: a
camera glitch on some hits, and pitch control in the flight levels.

## Playing it

You supply your own copy of the game. No game data is included in this
repository.

The mod is distributed as a **patch** you apply to your own disc image — you
will not need to build anything or touch a compiler.

> **Not released yet.** Patch releases will appear on the
> [Releases](../../releases) page once the remaining freeze is resolved. Until
> then this repository is source only.

When it is ready you will need:

- Your own copy of *Spyro the Dragon (USA)* as a `.bin`/`.cue` disc image.
  Verified against SHA-1 `cf3ce6bedeb89dfbc40990336180f3b9b0f40d9f`.
- An xdelta patcher, such as
  [Delta Patcher](https://github.com/marco-calautti/DeltaPatcher).
- An emulator or real hardware. **DuckStation** is what this is tested on.
- Two controllers. A CPU overclock of around 300% helps the framerate in busy
  scenes; do not go above it.

## How it works

The retail executable is reassembled byte-for-byte, with **24 individual
instructions** replaced by jumps into new code. That code lives in PlayStation
BIOS scratch RAM below the game — about 11 KB across three regions — because
the game's own address space is full end to end. It gets there by enlarging
the executable's declared size so the BIOS loads an extra payload, which a
boot stub copies into place before the game starts.

Player 2 exists by running the game's own logic twice per frame with a second
set of state swapped in — Spyro's tick, the camera update, the moby pass and
the scene build — rather than by teaching the engine about a second character.

[CHANGES.md](CHANGES.md) documents every hook, the memory map, and the
per-player state.

## Layout

```
mod/           the mod: source, build scripts, linker script
docs/          research notes, and history/ for superseded investigations
reference/     external projects we read but never build (untracked)
CHANGES.md     what the mod changes, and what the build verifies
BUGS.md        open work: active, parked, accepted
CLAUDE.md      working notes and the investigation log
```

`Roms/`, `reference/` and `tools/` are local working directories and are
deliberately not tracked. `reference/` holds the projects this work draws on,
each of which has its own upstream repository.

## Building from source

Only needed if you want to modify the mod — players should use the patch.

Requires a `mipsel-none-elf` GCC cross-compiler (the PlayStation runs a MIPS
processor, so the code has to be built by a compiler that targets it rather
than your own machine), Python 3, and
[mkpsxiso](https://github.com/Lameguy64/mkpsxiso) 2.30 or newer for repacking
the disc. `CLAUDE.md` records how that toolchain was installed on macOS,
including two patches that were needed to get it to build at all.

```sh
cd mod/projects/ntsc
make setup     # extracts the game's files from your disc image (once)
make           # builds the executable and runs the verification gate
make disc      # repacks the playable disc
```

`make` alone does **not** produce a playable disc — `make disc` does.

Every build verifies itself: that only the intended instructions changed, that
each hook reaches the function it names, that no memory allocations overlap,
and that every hook is documented. See section 6 of [CHANGES.md](CHANGES.md).

## Credits

- **Spyromain** — [Spyro2x2](https://github.com/Spyromain/Spyro2x2), the
  reference implementation this is ported from, and generous advice besides.
- **TheMobyCollective** — [spyro-1](https://github.com/TheMobyCollective/spyro-1)
  decompilation, the source of nearly every struct and function name here.
- The **Mod the Dragon** community, for pointers that saved real work.

## Licence

MIT. See [LICENSE](LICENSE).
