# Spyro 1 Co-Op Mod

Split-screen co-op for **Spyro the Dragon** (PlayStation, NTSC).

This mod features drop-in drop-out split-screen co-op that is fairly
functional, with only a few minor bugs. This includes a Multiplayer settings
page to customize your experience. Currently it is for only 2 players, but
**the goal is up to four players in split-screen, and to release this on
OpenPete.**

Built by studying and porting the architecture of
[Spyromain's incredible Spyro2x2 mod](https://github.com/Spyromain/Spyro2x2),
an equivalent mod for *Ripto's Rage*. MIT licensed, and that attribution is
preserved.

---

## What works today

- Two players, independent movement, cameras, and Sparx
- Horizontal or vertical split, three view-fit modes, and optional 16:9
  widescreen
- Drop-in / drop-out Co-Op: switch between one and two players from the pause
  menu
- Enemies, gems, and fodder react to the nearest player
- Shared lives, with individual death and respawn
- Dragon rescues, the balloonist, portals, and level transitions all handled

[BUGS.md](BUGS.md) lists what is still open, including two parked issues: a
camera glitch on some hits, and pitch control in the flight levels.

## Playing it

The mod is distributed as a **patch** you apply to your own disc image. No
game data is included in this repository.

You will need:

- Your own copy of *Spyro the Dragon (USA)* as a `.bin`/`.cue` disc image.
  Verified against SHA-1 `cf3ce6bedeb89dfbc40990336180f3b9b0f40d9f`.
- An xdelta patcher, such as
  [Delta Patcher](https://github.com/marco-calautti/DeltaPatcher).
- An emulator or real hardware. **DuckStation** is what this is tested on.
- Two controllers. A CPU overclock of 300% is recommended to keep the
  framerate high. It's best to not go above it or it could result in crashes.

Apply the `.xdelta` to your `.bin`, keep the supplied `.cue` beside the
result, and open the `.cue` in your emulator.

## How it works

The retail executable is reassembled byte-for-byte, with **24 individual
instructions** replaced by jumps into new code. That code lives in PlayStation
BIOS scratch RAM below the game (about 11 KB across three regions), because
the game's own address space is full end to end. It gets there by enlarging
the executable's declared size so the BIOS loads an extra payload, which a
boot stub copies into place before the game starts.

Player 2 exists by running the game's own logic twice per frame with a second
set of state swapped in: Spyro's tick, the camera update, the moby pass, and
the scene build. The engine is never taught about a second character.

[CHANGES.md](CHANGES.md) documents every hook, the memory map, and the
per-player state.

## Layout

```
mod/           the mod: source, build scripts, and linker script
docs/          research notes, and history/ for superseded investigations
reference/     external projects we read but never build (untracked)
CHANGES.md     what the mod changes, and what the build verifies
BUGS.md        open work: active, parked, accepted
CLAUDE.md      working notes and the investigation log
```

`Roms/`, `reference/`, and `tools/` are local working directories and are
deliberately not tracked. `reference/` holds the projects this work draws on,
each of which has its own upstream repository.

## Building from source

Building is only needed if you want to modify the mod. Most players should use
the patch.

The mod exists in **two constructions**, and it matters which one you build.

**The one that ships** is compiled inside
[TheMobyCollective's decompilation](https://github.com/TheMobyCollective/spyro-1),
because that is the only construction that can rebuild the game's level
overlays — which several fixes, including the flight-level HUD, depend on.

```sh
./mod/scripts/build_release.sh v0.2
```

That script documents its own prerequisites at the top: Docker with the
decomp's build image, the PSYQ headers, the `maspsx` submodule, your own
retail disc, and `mkpsxiso` and `xdelta3` on your PATH. It builds the disc,
produces the `.xdelta`, and then verifies the patch by applying it to a fresh
copy of the source disc and checking the result matches the build
byte-for-byte. A patch that does not round-trip is worse than none, because it
fails on someone else's machine after they have already downloaded it.

**The standalone one** patches 24 individual instructions into the retail
executable and needs no decompilation, no Docker, and no Python beyond the
build scripts. It is how the mod was originally written and it still builds,
but it cannot touch the overlays:

```sh
cd mod/projects/ntsc
make setup     # extracts the game's files from your disc image (once)
make           # builds the executable and runs the verification gate
make disc      # repacks the playable disc
```

`make` alone does **not** produce a playable disc — `make disc` does. This path
needs a `mipsel-none-elf` GCC cross-compiler (the PlayStation runs a MIPS
processor, so the code has to be built by a compiler that targets it rather
than your own machine), Python 3, and
[mkpsxiso](https://github.com/Lameguy64/mkpsxiso) 2.30 or newer. `CLAUDE.md`
records how that toolchain was installed on macOS, including two patches that
were needed to get it to build at all.

Either way the build verifies itself: that only the intended instructions
changed, that each hook reaches the function it names, that no memory
allocations overlap, and that every hook is documented. See section 6 of
[CHANGES.md](CHANGES.md).

## Contributing

I am a novice coder working on this in my own time, and I would genuinely
welcome help. If any of this interests you, please reach out, open a pull
request, or create an issue. Bug reports and play-testing are just as useful
as code, and [BUGS.md](BUGS.md) is an honest list of what is still wrong.

## Credits

- **Spyromain** — [Spyro2x2](https://github.com/Spyromain/Spyro2x2), the
  reference implementation this is ported from, and generous advice besides.
- **TheMobyCollective** — [spyro-1](https://github.com/TheMobyCollective/spyro-1)
  decompilation, the source of nearly every struct and function name here.
- The **Mod the Dragon** community, for pointers that saved real work.

## Licence

MIT. See [LICENSE](LICENSE).
