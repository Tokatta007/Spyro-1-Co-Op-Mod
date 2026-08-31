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

**A1 — Enemies near player 2 behave oddly: wandering aimlessly, running in place.**

- *Re-test this first.* The `g_PadBackup` fix (2026-08-28) may have cured it
  outright, and it has never been re-tested against this symptom.
- **Cause, candidate (a):** the moby-ownership split — a moby that hits a player
  it does not own runs its multi-frame hit reaction in the wrong player's pass.
  The user's cave test (P2 parked far away) made this and the camera spasm stop
  together.
- **Cause, candidate (b):** the `g_PadBackup` swap-offset typo, which zeroed 42
  model pointers (`g_Models[216..257]`) during every one of P2's passes.
- **Tried:** owner hysteresis (25%, kept). An ownership freeze / hit latch was
  built, did not fix the camera spasm, and was removed in the 2026-08-27
  baseline retreat.
- **Next step:** re-test after the `g_PadBackup` fix. If it persists, instrument
  *which* moby runs a hit reaction in the wrong pass before touching camera or
  ownership code again.

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

**C0 — Respawn drops the dragon out of the air.** *Untested.*

- Reported 2026-08-30: respawning inside a level put Spyro in the air and he
  fell to the ground, where the level's own entrance has him land properly.
- Fixed by asking the game where the floor is — its own `func_8004D5EC`,
  which retail uses to place every moby in a level — and standing him on it.
- **Watch for:** the respawn point being right, not just grounded. If a
  respawn ever puts him *through* the floor or on a ledge he should not be
  on, the probe found the wrong surface and the accept-window in
  `Sp1x2Ground.c` is the knob. Nothing outside the respawn is affected.
- Applies to individual respawns only. Solo play and game over still take
  the stock path, which reloads the level as always.

---

**C1 — Interrupt-deadlock fix (deferred pad poll).** *Untested — test this first.*

- Spyro's tick and the camera update no longer run with interrupts disabled.
  The pad callback is held off by a flag and its poll deferred, instead.
- **Watch for:** input problems for either player — sluggish, dropped or
  crossed controls. That is what this change touches.
- **And:** whether the hard freeze (old A2) returns. If it does, the CP0 dump
  says which kind: Cause 8 with the music stopped means this did not take;
  a bad address with music playing means the older memory-fault family.

---

**C2 — `g_PadBackup` swap offset corrected** (`0x16D8` -> `0x26D8`,
2026-08-28). The wrong address sat inside `g_Models`, so 42 enemy model
pointers were being zeroed during every one of player 2's passes. May well
have cured A1; nobody has re-tested.

---
