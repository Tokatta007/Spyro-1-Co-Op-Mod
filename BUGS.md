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

*Nothing.* Cleared again on 2026-09-01 when the flight-level HUD was fixed.
Anything new goes here.

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

**C4 — Flight-level HUD in split-screen.** *User-confirmed 2026-09-01
("pretty much perfect").*

- The collectible icons and countdown timer were drawn for a full 512x240
  screen: cut off in a horizontal split, and the timer lost entirely in a
  vertical one.
- Fixed from our own render pass rather than in the overlays. See
  `Sp1x2Flight.c`; the reasoning is worth reading before touching it.
- **Watch for:** anything in the WORLD moving that should not. Elements are
  identified by having screen-space coordinates, which is a heuristic - a
  deliberately conservative one, but a heuristic.

---

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

**2026-09-02 update.** Two theories closed and one fact gained; still open.
The null focus (`g_Spyro + 0x21C` uninitialised, stored unchecked into
`m_Focus`) was real, is fixed, and was **not** the cause — the spasm survives
with the focus provably healthy. The follow radius goes bad **only on player
2's camera update** (34 times in a session, player 1 never), but at ~9,793 it
is far too small to explain the symptom, and it does not match the
inter-player distance. So the camera's position is going wrong by some route
that is neither its focus nor its radius. Full detail, and what to instrument
next, in `CLAUDE.md`. Stopped by agreement, not because it is unfixable.
