# Spyro 1 Co-Op Mod

Split-screen co-op for **Spyro the Dragon** (PlayStation, NTSC). Two dragons,
two controllers, two cameras, per-player HUD, enemies and pickups assigned to
whichever player is nearest, and a Multiplayer settings page inside the game's
own pause menu.

Built by studying and porting the architecture of
[Spyromain's Spyro2x2](https://github.com/Spyromain/Spyro2x2), an equivalent
mod for *Ripto's Rage*. This project is MIT licensed and preserves that
attribution.

---

## What works

- Two players, independent movement, cameras and Sparx
- Horizontal or vertical split, three view-fit modes, optional 16:9 widescreen
- Drop-in / drop-out: switch between one and two players from the pause menu
- Enemies, gems and fodder react to the nearest player
- Shared lives; individual death and respawn
- Dragon rescues, the balloonist, portals and level transitions all handled

See [CHANGES.md](CHANGES.md) for exactly what the mod changes, and
[BUGS.md](BUGS.md) for what is still open — including two parked issues
(a camera glitch on some hits, and pitch control in the flight levels).

## What you need

- **Your own copy of the game.** No game data is included in this repository
  and none will be. You must supply `Spyro the Dragon (USA)` yourself.
  Verified against SHA-1 `cf3ce6bedeb89dfbc40990336180f3b9b0f40d9f`
  (`SCUS_942.28`: `84e3728ab94720d0873e2514adf4aade4935e0c5`).
- A `mipsel-none-elf` GCC toolchain, Python 3, and
  [mkpsxiso](https://github.com/Lameguy64/mkpsxiso) 2.30+.
- An emulator. **DuckStation** for playing; **PCSX-Redux** if you want the
  memory viewer and debugger.

## Building

Put a copy of your disc image in `mod/projects/ntsc/disc/`, set the toolchain
paths in `mod/env.mk`, then:

```sh
cd mod/projects/ntsc
make setup     # extracts the game's files from your disc image (once)
make           # builds the executable and runs the verification gate
make disc      # repacks the playable disc
```

`make` on its own does **not** produce a playable disc — `make disc` does.

Every build is verified automatically: that only the intended instructions
changed, that each hook reaches the function it names, that no memory
allocations overlap, and that every hook is documented. See section 6 of
[CHANGES.md](CHANGES.md).

## How it works

The retail executable is reassembled byte-for-byte, with **24 individual
instructions** replaced by jumps into new code. That code lives in
PlayStation BIOS scratch RAM below the game (about 11 KB across three
regions), because the game's own address space is full end to end. It is
delivered by enlarging the executable's declared size so the BIOS loads an
extra payload, which a boot stub then copies into place.

Player 2 exists by running the game's own logic twice per frame with a second
set of state swapped in — Spyro's tick, the camera update, the moby pass and
the scene build — rather than by teaching the engine about a second character.

## Layout

```
mod/           the mod: source, build scripts, linker script
docs/          research notes, and history/ for superseded investigations
CHANGES.md     what the mod changes, and what the build verifies
BUGS.md        open work: active, parked, accepted
CLAUDE.md      working notes and the investigation log
```

`Roms/`, `decomps/`, `Spyro2x2/` and `tools/` are local working directories
and are deliberately not tracked.

## Credits

- **Spyromain** — [Spyro2x2](https://github.com/Spyromain/Spyro2x2), the
  reference implementation this is ported from (MIT).
- **TheMobyCollective** — [spyro-1](https://github.com/TheMobyCollective/spyro-1)
  decompilation, the source of nearly every struct and function name here.
- **theMagicalKarp** — [open-spyro](https://github.com/theMagicalKarp/open-spyro),
  used as an address index.
- The **Mod the Dragon** community, for pointers that saved real work.

## Licence

MIT. See [LICENSE](LICENSE).
