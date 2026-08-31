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
- **BLOCKED ON TWO THINGS, in order:**
  1. *Nobody has found what draws it.* It is **not** `hud.c`, which only
     handles gems, dragons and lives. The draw path never tests the flight
     flag, and the flight levels' overlay touches the HUD only to update the
     gem count. Retail also skips `EnqueueLoadingScreenSprites` in flight
     levels, so the usual composer is not involved. Finding the drawer is
     step one and nothing can be estimated before it.
  2. *There is no space.* LOADER 8 free, BIOS2 16, BIOS2B 4 — 28 bytes in
     total, against roughly 150 for a relayout. The RAM survey concluded
     BIOS scratch is fully mapped, so this needs a structural change, and
     it is a large part of the argument for building on the decomp.
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

*Nothing outstanding.*

---
