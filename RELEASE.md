# Release notes

The text for the current GitHub Release. Version-specific only — what is in
this build, its hashes, and what someone downloading it needs to know before
they play. Everything else about the project lives in [README.md](README.md),
and this file should not restate it.

---

# v0.2 — first public release

Split-screen co-op for **Spyro the Dragon** (PlayStation, NTSC). Two dragons,
two controllers, two cameras, and a Multiplayer page in the game's own pause
menu. Drop in and out at any time.

## Files

| file | |
| --- | --- |
| `spyro1-coop-v0.2.xdelta` | the patch — apply to your own disc image |
| `spyro1-coop-v0.2.cue` | keep beside the patched `.bin`, and open this |

- Apply to a *Spyro the Dragon (USA)* `.bin` with SHA-1
  `cf3ce6bedeb89dfbc40990336180f3b9b0f40d9f`
- The patched `.bin` should come out as SHA-1
  `8d6ac27e58b0ff652950cf3d3363e35f6dd004ee`
- Patch it with [Delta Patcher](https://github.com/marco-calautti/DeltaPatcher),
  or online at [romhacking.net](https://www.romhacking.net/patch/) — nothing to
  install

**Check both hashes.** Almost every "it crashed" report comes down to one of
them being wrong: a different disc dump going in, or a patch that did not apply
cleanly coming out. If your patcher reports *"not implemented: secondary
decompressor"*, you have the original v0.2 patch — download it again.

No game data is included — the patch is only the difference between your disc
and the modded one, which is why it can be shared when the game cannot.

Tested on **DuckStation**. A 300% CPU overclock is recommended for the
framerate in busy scenes; above that can cause crashes.

## Known issues

Stated plainly, because you will meet them:

- **Camera glitch on some hits.** Being hit — a charging ram especially — can
  send a camera briefly skyward before it springs back. Visual only; control
  and collision are unaffected. Roughly a dozen attempts have failed to fix it.
- **Flight-level pitch.** Vertical steering responds poorly when the scene is
  busy. Measured to be framerate-related rather than a state bug; Spyromain's
  *Spyro2x2* did not solve this either.
- **Four-player split is not achievable on PS1 hardware.** It remains the
  goal, but the scene is drawn once per viewport and four measured at half the
  framerate of two. That is an OpenPete target, not a PS1 one.

[BUGS.md](BUGS.md) is the honest list, including cosmetic items.

## Notes on this build

Compiled inside
[TheMobyCollective's Spyro 1 decompilation](https://github.com/TheMobyCollective/spyro-1)
rather than patched into the retail executable. That is what allows the level
overlays to be rebuilt, which is the only route to several fixes — the
flight-level HUD among them.

The patch is around 600 KB. Almost all of that is the game's own code, which
this build recompiles; the mod itself is about 11 KB.

Built with `mod/scripts/build_release.sh`, which verifies the patch by
applying it back and comparing against the build.
