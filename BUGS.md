# Spyro 1 Co-Op Mod — Open bug list

This is the **open-work list**: what is still wrong, what is parked, and what
has been consciously accepted. `CHANGES.md` records what the mod *does* (hooks,
memory map, per-player state); `CLAUDE.md` records the investigation history —
how each thing was found and what failed on the way.

**When an item is fixed and confirmed, delete it from this file** and record the
fix in `CHANGES.md`. Nothing here should stay after it is solved.

Last reviewed: 2026-08-30.

---

## 1. Active

Genuinely broken, nobody has decided to live with it. Worst first.

**A2 — The flight-level collectible HUD is mispositioned in split-screen.**

- Reported 2026-08-30: it does not appear at all in the **vertical** split,
  and is visible but **cut off** in the horizontal one.
- The shape of that is a HUD drawn at fixed coordinates for a full 512x240
  screen: in horizontal it overhangs the half-height viewport, and in
  vertical it sits outside a 256-wide one entirely. Exactly the fault the
  main HUD had before `Sp1x2HudShift`.
- **Wanted:** the same treatment as the gems / dragons / lives HUD, which is
  shifted per viewport and, for the side-by-side split, relaid out per moby.
- **WHAT IT IS (user, 2026-08-31): ten collectible slots that pop up as you
  collect during the flight and fade after a second or two.** NOT the results
  screen, which the user reports looks fine. An earlier note here claimed it
  was `Flight5`, the results screen — that was wrong, reached by eliminating
  alternatives rather than by looking, and the user corrected it.
- **Where it comes from.** `RegisterFlightMobyCollectibleType`
  (`moby_helpers.c:1818`) bumps `g_FlightObjectiveCounters` and records which
  types are currently showing in **`g_FlightObjectiveActiveSlots`**
  (`0x80078608`, four ints; counters at `0x80078630`). The only other code
  touching that array is `func_level_X_8007CFB4`, the flight levels' moby
  megafunction — **still assembly, ~5,800 lines**, which spawns character
  mobys and writes screen positions into them as immediates
  (`m_Position.x` at moby offset `0xC`; values seen include 30 and 400).
- **CONFIRMED FROM USER SCREENSHOTS + MEMORY, 2026-08-31.** The in-flight
  display has TWO parts, both in screen space for a 512-wide screen:
    1. a row of **8 collectible icons at the TOP-LEFT** (`x = 30`), filled
       according to how many of that type you have;
    2. the **countdown timer at the TOP-RIGHT** (`x = 400`) — the user also
       wants this handled.
  `g_FlightObjectiveCounters` at **`0x80078630`** is verified against the
  screen twice over: `[0, 2, 0, 0]` in flight with two arches lit, and
  `[0, 7, 2, 0]` on a results screen reading BARRELS 0/8, ARCHES 7/8, PLANES
  2/8, CHESTS 0/8. So the indices are barrels, arches, planes, chests.
  `g_FlightObjectiveActiveSlots` read all `-1` on both captures, so it is not
  what gates visibility — do not build on that assumption.
- **The results screen is fine** and needs no work. An earlier report of
  collectibles rendering as "1"/"0" while correctly coloured did not
  reproduce, and nothing looked wrong in memory; treat it as fixed by
  something since, and log it again only if it returns.
- So it is neither `hud.c` nor the decompiled part of the flight overlay,
  which is why two searches missed it.
- **COST, measured 2026-08-31.** The position writes reach the display mobys
  through a register chain (`$a1` from `$s3`, set far earlier) inside that
  5,800-line assembly function. Moving them means either tracing those
  registers to find where the display mobys are stored, or decompiling that
  part. Neither is a coordinate tweak.
- **AND IT CANNOT BE TESTED YET.** The decomp build has no split screen,
  because the mod is not ported to it, so there is nothing there for a fix to
  be right or wrong against. Doing this before the port means writing code
  that cannot be verified. **Port first.**
- **It needs a HUD PER VIEWPORT, like the main one.** Flight levels are fully
  two-player: both dragons fly and play. An older note in this project called
  them "single-player, player 2 frozen" — that has been out of date for some
  time and the user corrected it on 2026-08-31. The only outstanding flight
  problem is player 2's Y-axis steering, which is parked separately.
- Wanted layout, per the user: **horizontal split — across the top of each
  view; vertical split — top-left going down**, matching what
  `Sp1x2HudShift` does for gems, dragons and lives. Stock layout in
  one-player. The **timer belongs top-right** of each view and is drawn by
  the same code, so it comes along with the same change.
- **This may make it easier, not harder.** If those are mobys in the HUD
  list, our own render pass could reposition them the way `Sp1x2HudShift`
  already repositions the main HUD — no overlay edit at all. Confirm what
  they are before designing anything.
- **THIS IS WHY IT CANNOT BE FIXED TODAY, and space is not the reason.**
  Our architecture patches the executable and *does not patch overlays* at
  all. There is no hook to place. Even with bytes to spare, the code that
  needs changing is not code we can reach.
- **With the decompilation it becomes ordinary work**, because overlays are
  built from source there — `make` produces all 37 of them. This is the
  clearest single argument for that pivot: it turns an impossible item into
  a normal one.
- **And it is fixable there sooner than expected (2026-08-31).** The
  decomp pins some undecompiled overlay functions to absolute addresses,
  which would make editing an overlay risky — but the five flight levels
  carry **none** of those pins. Their overlays can be rebuilt freely. See
  `docs/decomp/README.md`.
- Cosmetic, and confined to the four flight levels.

---


## 2. Parked

Real bugs where investigation is deliberately suspended.

**P1 — On some hits (the charging ram especially) a camera flies stratosphere-high and springs back.** Also shows as a brief zoom pulse.

- **Cause:** the camera position is *derived* — `focus + radius x direction`. A
  measured spasm had `m_Focus = NULL` with camera Z 54,150,062 against a live
  Spyro Z of 21,058: the camera was faithfully following address zero. The null
  comes from `func_8003FE40` inside Spyro's tick copying the pointer at
  `g_Spyro+0x21C`, which is null for player 2.
- **Tried:** roughly ten fixes, all missed — radius and elevation clamps (the
  elevation clamp was *generating* spasms; never clamp a wrapping angle),
  shake-trio zeroing (the trio is dead code), the moby hit latch, a swap-guard
  fix (real, kept, not this bug), the four unswapped camera globals (real, kept,
  gave P2 the vanilla zoom-back). `Sp1x2FixFocus` (2026-08-28) repairs the null
  either side of P2's camera update — **the spasm persists**.
- **Next step:** read the repair counters at `0x8000ED74` / `0x8000ED78` after a
  session before theorising. If only `ED78` climbs, the null is written *inside*
  the camera update.
- **Why parked:** ~10 reasoned fixes have missed, and the crashes it caused are
  now contained — the two collision entry gates refuse impossible coordinates
  (counters `0x8000ED60` / `0x8000ED64`) and the Baruti `addu` patches fix the
  retail overflow. Only the *visual* spasm remains. Restart on a measurement,
  not a theory.

---

**P2 — Flight/speedway levels: the Y axis (pitch) barely responds when the scene is busy.** Both players are present and playable; only vertical steering is wrong.

- **Cause: framerate-sensitive, not shared state.** Measured live: heavy scene
  -> `g_DeltaTime` 6-7 -> steering dead; turn away -> dt 3-4 -> steering normal,
  instantly. Pitch and yaw are integrated per *substep*, steering input applied
  per *frame*.
- **Tried:** the flight attitude controller, the pitch accumulator, the substep
  counter, the collision scalars (added to the swap set — kept), disabling the
  player push in flight (kept), raising the substep clamp 4->8, capping
  `RotateSpyroToNeutral`. Three leak sweeps found no unrestored state.
- **Next step:** nothing until `func_80047B60` (`DispatchSpyroPhysicsByState`)
  is decompiled — that is where steering input becomes pitch.
- **Why parked:** 2026-08-20 — **Spyro2x2 did not solve this either**
  (`Sp2x2TeleportSpyro.c`: "TODO for icy speedway minigame"), so there is no
  missing port to find. Restart when `func_80047B60`, `func_80043FE4` or
  `func_80041670` land in the spyro-1 decomp.

---

## 3. Accepted

Known limitations we have consciously decided not to fix. These belong in the
release readme, not in a bug tracker.

**X1 — Widescreen: distant terrain and portal doors blink in and out at the left/right edges at the widest fit.**

- We draw a wider view than the game's visibility logic expects.
- Scaling the pure camera matrix was tried twice and reverted — the renderer
  *draws* with that matrix, giving blocky green distant geometry.
- A real fix widens `r_environment`'s per-chunk visibility test. Widescreen is
  optional and the artefact only shows at the widest settings. **Worth
  documenting in the readme so players can choose.**

---

**X2 — The "+3" pickup text appears in both viewports.**

- Enqueued once, drawn by both passes. Cosmetic; same family as the region
  table and the flame chains, both of which were fixed by giving each viewport
  its own copy.
- **Would like fixed eventually** — it should only show on the screen of the
  player who collected it. Low priority; it does not look bad.

---

**X3 — Speedway pickup counters lay out horizontally even in side-by-side split.**

- As you collect the ten of each item on a speedway, the counter row is
  always horizontal, which reads oddly when the screens are side by side.
- Probably the same shape as the HUD relayout already solved for the main HUD
  (per-moby offsets rather than moving a whole group). Cosmetic, unexplored.

---

## 4. Recently fixed — needs confirmation

**C3 — The decomp port.** *User-confirmed booting and playing 2026-09-01;
everything beyond the intro is still lightly tested.*

- The mod now builds inside the decompilation. Same behaviour, different
  construction: direct calls instead of 24 patched instructions, real symbols
  instead of raw addresses, and the collision guards are ordinary wrappers
  rather than entry patches.
- **Watch for:** anything that differs from v0.1. Both discs are kept side by
  side (`spyro1-coop.cue` is v0.1, `spyro1-port.cue` is the port), so any
  difference can be compared directly rather than guessed at. The logic was
  translated unchanged precisely so that a bug has a v0.1 counterpart.
- Three bugs found and fixed during the port, all worth knowing:
  the vsync counters were bound 0x40 low and overwrote the portal-wait flag;
  six raw addresses survived the first conversion pass; and the executable
  cannot grow at all (see `docs/decomp/README.md`).

---
