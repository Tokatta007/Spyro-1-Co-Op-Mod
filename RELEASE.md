# Release notes

The text for the current GitHub Release. Version-specific only — what is in
this build, its hashes, and what someone downloading it needs to know before
they play. Everything else about the project lives in [README.md](README.md),
and this file should not restate it.

---

# v0.3 — per-player colour

Give each dragon his own colour, from a page in the game's own pause menu.

## What is new since v0.2

- **A COLOR page**, at Pause → Options → SQUARE → COLOR. Eight dials: red,
  green, blue and tint strength, for each dragon.
- **A live swatch** above the box for each player, framed in the game's own
  shimmering gold. It shows the blended result rather than the raw value, so
  turning the tint down visibly walks back toward Spyro's purple.
- **The d-pad steps by one; L2 and R2 by sixteen.** SQUARE restores the column
  your cursor is in.
- Colours **hold through portals, level entrances, dialogue and exits** — the
  dragons stay themselves through a whole session.
- Defaults are **Spyro exactly as he ships**, so a fresh start looks like
  retail and any colour is one you chose.

This tints the dragon rather than repainting his textures — the same thing
*Spyro 2*'s colour cheat codes did, and with the same character: a yellow
Spyro is a yellow-tinted Spyro, not a repainted one.

## Files

Download **`spyro1-coop-v0.3.zip`** and unzip it. Inside:

| file | |
| --- | --- |
| `spyro1-coop-v0.3.xdelta` | the patch — apply to your own disc image |
| `spyro1-coop-v0.3.cue` | keep beside the patched `.bin`, and open this |
| `README.txt` | the same instructions, offline |

- Apply to a *Spyro the Dragon (USA)* `.bin` with SHA-1
  `cf3ce6bedeb89dfbc40990336180f3b9b0f40d9f`
- Patch it with [Delta Patcher](https://github.com/marco-calautti/DeltaPatcher),
  or online at [romhacking.net](https://www.romhacking.net/patch/) — nothing to
  install

**Check both hashes.** Almost every "it crashed" report comes down to one of
them being wrong: a different disc dump going in, or a patch that did not apply
cleanly coming out.

No game data is included — the patch is only the difference between your disc
and the modded one, which is why it can be shared when the game cannot.

Tested on **DuckStation**. A 300% CPU overclock is recommended for the
framerate in busy scenes; above that can cause crashes.

## Known issues

- **Camera glitch on some hits.** Being hit — a charging ram especially — can
  send a camera briefly skyward before it springs back. Visual only.
- **Flight-level pitch.** Vertical steering responds poorly when the scene is
  busy. Measured to be framerate-related rather than a state bug.
- **Player 2 talking to the balloonist** shows player 1's colour for the
  duration of the conversation. Cosmetic, and the fix does not fit in the
  space left.
- **Four-player split is not achievable on PS1 hardware.** It remains the
  goal, but the scene is drawn once per viewport and four measured at half the
  framerate of two. That is an OpenPete target, not a PS1 one.

[BUGS.md](BUGS.md) is the honest list, including cosmetic items.

## Notes on this build

Compiled inside
[TheMobyCollective's Spyro 1 decompilation](https://github.com/TheMobyCollective/spyro-1)
rather than patched into the retail executable.

Built with `mod/scripts/build_release.sh`, which verifies the patch by
applying it back and comparing against the build.
