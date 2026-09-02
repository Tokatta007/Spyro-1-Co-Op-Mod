# Spyro 1 Co-Op Mod — v0.2

Split-screen co-op for **Spyro the Dragon** (PlayStation, NTSC).

Two dragons, two controllers, two cameras, and a Multiplayer settings page
inside the game's own pause menu. Drop-in and drop-out at any time.

## Installing

You supply your own copy of the game; the patch contains none of it.

1. Get *Spyro the Dragon (USA)* as a `.bin`/`.cue` disc image. The patch
   expects one whose `.bin` has SHA-1 `cf3ce6bedeb89dfbc40990336180f3b9b0f40d9f`.
2. Apply `spyro1-coop.xdelta` to the `.bin` with an xdelta patcher, such as
   [Delta Patcher](https://github.com/marco-calautti/DeltaPatcher).
3. Keep the supplied `spyro1-coop.cue` beside the result, and open the `.cue`
   in your emulator.

**DuckStation** is what this is tested on. A CPU overclock of 300% is
recommended to keep the framerate up in busy scenes; going above it can cause
crashes.

## What works

- Two players with independent movement, cameras, and Sparx
- Horizontal or vertical split, three view-fit modes, optional 16:9 widescreen
- Switch between one and two players at any time from the pause menu
- Enemies, gems, and fodder react to whichever player is nearest
- Shared lives, with individual death and respawn
- Dragon rescues, the balloonist, portals, and level transitions
- Flight levels, including their own HUD in both split modes

## Known issues

Stated plainly, because you will meet them:

- **Camera glitch on some hits.** Being hit — a charging ram especially — can
  send a camera briefly skyward before it springs back. Visual only; it does
  not affect control or collision. Roughly a dozen attempts have failed to fix
  it and it is parked rather than forgotten.
- **Flight-level pitch.** In flight levels the vertical steering responds
  poorly when the scene is busy. Measured to be framerate-related rather than a
  state bug. Spyromain's *Spyro2x2* did not solve this either.
- Some cosmetic items are listed in [BUGS.md](BUGS.md), which is kept honest.

**Four-player split is the goal but is not achievable on PS1 hardware** — the
scene is drawn once per viewport, and four viewports measured at half the
framerate of two. That is an [OpenPete](https://github.com/) target.

## What this is built from

The mod is compiled inside
[TheMobyCollective's Spyro 1 decompilation](https://github.com/TheMobyCollective/spyro-1),
which is what allows the level overlays to be rebuilt — the only route to
several fixes, the flight HUD among them.

The patch is around 500 KB. Nearly all of that is the game's own code, which
this build recompiles; the mod itself is about 11 KB. It contains no game data,
which is why it can be shared when the game cannot.

Player 2 exists by running the game's own logic twice per frame with a second
set of state swapped in: Spyro's tick, the camera update, the moby pass, and
the scene build. The engine is never taught about a second character.

## Thanks

- **Spyromain**, for [Spyro2x2](https://github.com/Spyromain/Spyro2x2) — the
  reference implementation this is ported from — and for generous advice.
- **TheMobyCollective**, for the decompilation.
- The **Mod the Dragon** community.

MIT licensed. Contributions and bug reports are very welcome — see the
Contributing section of [README.md](README.md).
