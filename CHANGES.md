# Spyro 1 Co-Op Mod — Catalogue of Changes

## How to use this file

This is the canonical record of everything the mod changes in *Spyro the
Dragon* (NTSC `SCUS_942.28`, SHA-1 `84e3728ab94720d0873e2514adf4aade4935e0c5`).
Every finalised change must be added here in the same pass that makes it.
If it is not in this file, it is not finished.

---

## 1. What the mod is, and how it gets into RAM

Two-player split-screen co-op: two dragons, two controllers, two cameras,
per-player HUD, nearest-player enemies and pickups, and a Multiplayer page in
the game's own pause menu. **No original code is rewritten.**

- **Hooks.** `src/asm/main.S` rebuilds the retail executable with `.incbin` and
  replaces exactly 24 four-byte instructions (Table 1). Everything else in the
  417,792-byte file is untouched.
- **Code delivery.** `LOADER` is free — the BIOS copies the EXE header to
  `0x8000B070` and ignores the padding. `BIOS2`/`BIOS2B` are delivered by
  `header.S` enlarging `t_size` (`0x65800` → `0x68000`) and redirecting `PC` to
  `Sp1x2Boot`, which copies the extra payload up, calls `FlushCache`, and jumps
  to the real entry at `0x8005b8e0`. `t_size` **must** stay a multiple of 2048
  or the BIOS freezes at the PlayStation logo.

```
cd "mod/projects/ntsc"
make          # builds the executable + runs the verification gate ONLY
make disc     # repacks build/disc/spyro1-coop.bin — the playable disc
```

- `make` alone does **not** produce a playable disc. Before handing a build to
  anyone, check `build/disc/spyro1-coop.bin` is newer than `build/rom/SCUS_942.28`.
- `make clean` after changing any compiler flag — the Makefile does not track them.
- The gate bounds *how many* bytes changed, not *where*. It cannot catch a hook
  placed four bytes off. Disassemble `build/rom/SCUS_942.28` at every new hook.
- **DuckStation** for play-testing; **PCSX-Redux** for memory viewer / debugger.

---

## 2. Table 1 — Hooks and patches

24 entries, one per replaced instruction; `EXPECTED_HOOKS = 24` agrees.
File offset = RAM address − `0x80010000` + `0x800`. Instructions were read out
of the real ROMs, not from comments.

Confidence: **OK** confirmed · **OK\*** works, known limits · **?** unproven ·
**DIAG** also carries a diagnostic counter.

| Area | RAM | Offset | Was | Now | Purpose | C |
| --- | --- | --- | --- | --- | --- | --- |
| Render | `0x8001227c` | `0x02a7c` | `jal GamestateDraw` `0x8001ed5c` | `jal Sp1x2Graphics` | Renders the scene twice per frame, one viewport per player. | OK |
| Render | `0x8001a0d8` | `0x0a8d8` | `jal RasterizePairedActor` `0x80023ac4` | `jal Sp1x2DrawPortalSpyro` | Draws the live dragon twice during the portal fly-in. | OK |
| Render | `0x80058bc0` | `0x493c0` | `jal TickSparkles` `0x800584c4` | `jal Sp1x2TickSparkles` | Ages particles in pass 0 only, not once per pass. | OK |
| Input | `0x80012444` | `0x02c44` | `jal InstallVSyncCallback` `0x8005de58` | `jal Sp1x2InstallVSyncCallback` | Runs the retail pad handler twice, one player each; merges P2's buttons during menus. | OK |
| P2 logic | `0x80033ad8` | `0x242d8` | `jal TickSpyroGameplayFrame` `0x8004a200` | `jal Sp1x2TickPlayer2Spyro` | Runs Spyro's tick once per player; seeds P2, handles handovers, separates the dragons. | OK\* (P2 frozen in flight) |
| P2 logic | `0x80033aa4` | `0x242a4` | `jalr $v0` via `g_UpdateMoby` | `jal Sp1x2UpdateMobys` | Runs the moby megafunction once per player with the other's mobys masked; drives Sparx. | OK\* (single-pass in flight) |
| P2 logic | `0x80042f10` | `0x33710` | `jal TriggerRespawnOrGameOver` `0x8002c85c` | `jal Sp1x2Die` | Death site 1: individual respawn while a partner is alive and lives remain. | OK\* |
| P2 logic | `0x8004a4d8` | `0x3acd8` | `jal TriggerRespawnOrGameOver` `0x8002c85c` | `jal Sp1x2Die` | Death site 2, same handler. | OK\* |
| Sound | `0x80056528` | `0x46d28` | `jal VecMagnitude` `0x800171fc` | `jal Sp1x2SoundListenerDistance` | Voice distance = minimum over both players' cameras. | OK |
| Camera | `0x80033b4c` | `0x2434c` | `jal UpdateCameraFrame` `0x80037bd4` | `jal Sp1x2UpdateCameras` | Camera update once per player with P2's state swapped in; calls `Sp1x2FixFocus` either side of his update. Four other call sites left alone. | OK\* (focus repair `?`) |
| Menu | `0x8001a7f8` | `0x0aff8` | `bne $v1,$v0,+0x34` | `beq $v1,$zero,+0x34` | Big pause box **background** for any substate but 0 (ours is 3). | OK |
| Menu | `0x8001a894` | `0x0b094` | `bne $v1,$v0,+0x60` | `beq $v1,$zero,+0x60` | Same test for the box **border**; must match the background. | OK |
| Menu | `0x8001a980` | `0x0b180` | `jal BuildTextSpriteChain` `0x80017fe4` | `jal Sp1x2PauseDraw` | Draws "PAUSED", then appends our Multiplayer page or its SQUARE hint. | OK |
| Menu | `0x800338b8` | `0x240b8` | `jal PauseMenu_Update` `0x8002e12c` | `jal Sp1x2PauseUpdate` | Takes over pause input and housekeeping while our page is open. | OK |
| Menu | `0x8001b3f0` | `0x0bbf0` | `jal BuildTextSprites` `0x800181ac` | `jal Sp1x2MainMenuItem` | Suppresses "CONTINUE" while our page is open. | OK |
| Menu | `0x8001b440` | `0x0bc40` | `jal BuildTextSprites` | `jal Sp1x2MainMenuItem` | Suppresses "OPTIONS". | OK |
| Menu | `0x8001b490` | `0x0bc90` | `jal BuildTextSprites` | `jal Sp1x2MainMenuItem` | Suppresses "INVENTORY". | OK |
| Menu | `0x8001b4f0` | `0x0bcf0` | `jal BuildTextSprites` | `jal Sp1x2MainMenuItem` | Suppresses row-4 "QUIT" (flight levels). | OK |
| Menu | `0x8001b574` | `0x0bd74` | `jal BuildTextSprites` | `jal Sp1x2MainMenuItem` | Suppresses row-4 "EXIT LEVEL" (levels). | OK |
| Menu | `0x8001b5c0` | `0x0bdc0` | `jal BuildTextSprites` | `jal Sp1x2MainMenuItem` | Suppresses row-4 "QUIT GAME" (homeworlds). | OK |
| Guard | `0x8004ae38` | `0x3b638` | `lui $at,0x8007` | `j Sp1x2ProbeGate` | Refuses segment probes whose end vector is negative or ≥ `0x400000`; averts the freeze at `0x8004b7c8`. Counter `0x8000ED64`. | OK / DIAG |
| Guard | `0x8004be4c` | `0x3c64c` | `lui $at,0x8007` | `j Sp1x2QueryGate` | Same rule on the sphere query centre; averts the freeze at `0x8004cb68`. Counter `0x8000ED60`. | OK / DIAG |
| Retail fix | `0x80017228` | `0x07a28` | `add $at,$at,$v0` | `addu $at,$at,$v0` | Fixes the retail "Baruti crash" — signed `add` overflow-traps in `VecMagnitude`. | OK |
| Retail fix | `0x8001722c` | `0x07a2c` | `add $at,$at,$v1` | `addu $at,$at,$v1` | Second half of the same sum. | OK |

- Row 4 of the pause menu has three variants by location; two were once missed
  and "EXIT LEVEL" bled through our page until they were added.
- The two guards are **entry patches**: they replace the target's first
  instruction (`lui $at,0x8007`), so the second still runs in the delay slot
  with `$at` unset and each gate rebuilds `$at` before resuming four bytes in.
  They were retired once on a theory and the freeze came straight back — do not
  retire them again without a measurement.

---

## 3. Table 2 — Memory map

### Code regions (`projects/ntsc/coop.ld`)

| Range | Size | Contents | Used |
| --- | --- | --- | --- |
| `0x8000B070`–`0x8000B86F` | `0x800` | **LOADER.** EXE header + boot stub, pad/input, region + moby-flag + particle sync, portal draw, sound distance, respawn grounding. Free via the BIOS. | 2044 / 2048 (**4 free**) |
| `0x8000B870`–`0x8000BFFF` | — | **DO NOT USE.** BIOS `EXEC` struct. LOADER's length stops the linker here. | — |
| `0x8000C000`–`0x8000DFFF` | `0x2000` | **BIOS2.** Render hook, viewport/squash, HUD shift, flame chains, P2 Spyro/camera/moby machinery, death handler, pause menu. | 8176 / 8192 (**16 free**) |
| `0x8000E000`–`0x8000E3FF` | `0x400` | **DO NOT USE.** BIOS kernel event/thread tables. Copying code here black-screens the game. | — |
| `0x8000E400`–`0x8000E7FF` | `0x400` | **BIOS2B.** Sparx lifecycle, menu chime, main-menu-item wrapper, both collision gates, `Sp1x2FixFocus`. | 1020 / 1024 (**4 free**) |
| `0x8000E800`–`0x8000FFFF` | `0x1800` | **BIOS3.** Runtime data only, never code. No delivery needed. | fully allocated |
| `0x80010000`–`0x800757FF` | `0x65800` | The game, reassembled with 24 patched instructions. | exact |

Payload arithmetic, all of which must agree: `PAYLOAD_BYTES = 0x2800`;
`t_size = 0x68000` in `header.S`; `--pad-to=0x68800`; `.bios2` at file offset
`0x66000` (`0x2000`), `.bios2b` at `0x68000` (`0x400`).

### Runtime data (BIOS3) — read from the C source, not the map comment

| Range | Size | Contents | Owner |
| --- | --- | --- | --- |
| `0x8000E800`–`0x8000EC43` | 1092 | P2's Spyro shadow — every region of Table 3, packed. | `Sp1x2Spyro.c` |
| `0x8000ED00` | 4 | `SP1X2_P2_READY` — seeded flag; the mod's master switch. | `Sp1x2Spyro.c` |
| `0x8000ED04` | 4 | Level id P2 was last seeded in. | `Sp1x2Spyro.c` |
| `0x8000ED08` | 4 | Handover-pending flag (P2 owns a running sequence). | `Sp1x2Spyro.c` |
| `0x8000ED0C` | 4 | `SP1X2_SUBSTEPS_OWED` — substep budget captured before P1 consumes it. | `Sp1x2Spyro.c` |
| `0x8000ED10` | 4 | P2's Sparx (`Moby *`), 0 = not spawned. | `Sp1x2Spyro.c` / `Sp1x2Sparx.c` |
| `0x8000ED14` | 4 | `g_Sparx` as last seen — moby-array rebuild detector. | `Sp1x2Sparx.c` |
| `0x8000ED18` | 4 | Last non-zero gamestate; gates the teleport detector. | `Sp1x2Spyro.c` |
| `0x8000ED20`–`0x8000ED2F` | 16 | Arrival capture (x, y, z, rot) — fallback respawn point. | `Sp1x2Graphics.c` |
| `0x8000ED30`–`0x8000ED43` | 20 | True-start cache (levelId, x, y, z, rot) — preferred respawn point. | `Sp1x2Graphics.c` / `Sp1x2Spyro.c` |
| `0x8000ED60` | 4 | Collision **query** gate refusals. | `Sp1x2Gates.c` |
| `0x8000ED64` | 4 | Collision **probe** gate refusals. | `Sp1x2Gates.c` |
| `0x8000ED70` | 4 | Focus repairs before P1's update — reserved; nothing calls slot 0. | `Sp1x2Gates.c` |
| `0x8000ED74` | 4 | Focus repairs **before** P2's camera update. | `Sp1x2Gates.c` |
| `0x8000ED78` | 4 | Focus repairs **after** P2's camera update. If only this climbs, the null is written inside the update itself. | `Sp1x2Gates.c` |
| `0x8000ED7C` | 4 | P2's camera Z at an after-repair — how far it had flown. | `Sp1x2Gates.c` |
| `0x8000EE00`–`0x8000EF0F` | `0x110` | P2's camera shadow (full `g_Camera` copy). | `Sp1x2Spyro.c` (read `+0x28` by `Sp1x2Pad.c`) |
| `0x8000EF10`–`0x8000EF1F` | 16 | P2's copy of the four camera globals outside `g_Camera`. | `Sp1x2Spyro.c` |
| `0x8000F000`–`0x8000F0FF` | `0x100` | Markers / diagnostics. **Declared only — nothing writes here now.** | `Sp1x2Graphics.c` / `Sp1x2Spyro.c` |
| `0x8000F100`–`0x8000F17F` | 128 | Flame matrix chains: one 5-word chain per (dragon, viewport), at `+player*0x40 + pass*0x20`. | `Sp1x2Graphics.c` |
| `0x8000F180` | 4 | `SP1X2_SPLIT_MODE` — 1 = vertical, else horizontal. | `Sp1.h` / `Sp1x2Menu.c` |
| `0x8000F188` | 4 | `SP1X2_WIDESCREEN` — anamorphic 16:9 on/off. | `Sp1.h` / `Sp1x2Menu.c` |
| `0x8000F190` | 4 | `SP1X2_VIEW_FIT` — 0 FULL, 1 BALANCED, 2 CROPPED. | `Sp1.h` / `Sp1x2Menu.c` |
| `0x8000F198` | 4 | `SP1X2_PLAYERS` — 1 = solo, anything else (incl. 0) = 2. | `Sp1.h` / `Sp1x2Menu.c` |
| `0x8000F1A0` | 4 | `SP1X2_MENU_ACTIVE` — is our page open. | `Sp1x2Menu.c`, `Sp1x2Sparx.c` |
| `0x8000F1A4` | 4 | Multiplayer menu cursor row. | `Sp1x2Menu.c` |
| `0x8000F1A8`–`0x8000F1B7` | 16 | Selected row's label/value mobys + counts, for the letter wobble. Fixed addresses because a `static` would land in a discarded `.bss`. | `Sp1x2Menu.c` |
| `0x8000F1C0`–`0x8000F1CB` | 12 | Teleport detector: live Spyro's position last frame (jump > `0x4000` = restart). | `Sp1x2Spyro.c` |
| `0x8000F1D8` | 4 | `SP1X2_RENDER_PASS` — 0 = player 1, 1 = player 2. | `Sp1.h` / `Sp1x2Graphics.c` |
| `0x8000F200`–`0x8000F34C` | 333 | P2's derived pad state: `g_Pad` + `g_PadBackup` + `g_PadSwapFlag` + `g_ActivePad`. | `Sp1x2Pad.c` |
| `0x8000F3F0` | 4 | Region-table valid bits (one per pass). | `Sp1x2Pad.c` |
| `0x8000F400`–`0x8000F4FF` | 256 | Pass 0's saved region-visibility table. | `Sp1x2Pad.c` |
| `0x8000F500`–`0x8000F5FF` | 256 | Pass 1's saved region-visibility table. | `Sp1x2Pad.c` |
| `0x8000F600`–`0x8000F7FF` | 512 | Particle snapshot (`m_Life`, `m_03`) × 256, so two passes cannot age a particle twice. | `Sp1x2Pad.c` |
| `0x8000F800`–`0x8000F9FF` | 512 | Moby `m_WasDrawn` sync flags × 512; seen by *either* player counts as seen. | `Sp1x2Pad.c` |
| `0x8000FA00`–`0x8000FDFF` | 1024 | Moby mask stash (`m_WasDrawn`, `m_UpdateDistance`) × 512. | `Sp1x2Spyro.c` |
| `0x8000FE00`–`0x8000FFFF` | 512 | Moby owner table × 512. Ends **exactly** at `0x80010000`. | `Sp1x2Spyro.c` |

### Overlap check — no live collisions; everything above is disjoint

- `0x8000FE00` has **zero headroom**: any growth writes into the game's RAM.
- Moby tables are sized for **512** in code (`SP1X2_MOBY_MAX`, `n < 0x200`); the
  source comment claims 1024. Acting on the comment would overrun BIOS3.
- `0x8000C000` is **code**, not markers, despite a stale comment in
  `Sp1x2Graphics.c`; the `#define` below it correctly says `0x8000F000`.
- Fixed 2026-08-28: a dead leak-sweep instrument in `Sp1x2Spyro.c` declared a
  1,440-byte buffer at `0x8000EE00` — P2's camera shadow. Deleted. Delete
  instruments when they are retired.

---

## 4. Table 3 — Swapped per-player state

Before player 2's tick, camera update or draw, each region below is **exchanged**
with his shadow, then exchanged back. The rule: *either both players' state is
exchanged or neither is* — every helper shares one `SP1X2_P2_READY` guard.

### Spyro — `sp1x2_spyro_state[]`, `Sp1x2Spyro.c`

15 regions, 1092 bytes, stored as 16-bit offsets from `0x80075000`. Regenerate
by merging adjacent globals from the decomp symbol files; never hand-edit a range.

| Address | Size | What |
| --- | --- | --- |
| `0x80078A58` | 676 | `g_Spyro` — whole struct (size `static_assert`ed). |
| `0x800786C8` | 312 | `g_SpyroFlame` — omitting it drew P1's flame again in P2's pass. |
| `0x8007AA10` | 40 | `SpyroShadow` — the drop shadow. |
| `0x80075718` | 4 | `g_SurfaceBelowFlags` — collision scratch. |
| `0x80075788` | 4 | `g_nSpyroIdleAnimTimeout`. |
| `0x80075804` | 4 | `g_pSpyroContactActor`. |
| `0x80075808` | 4 | `g_CollisionTriangleIndex` — collision scratch. |
| `0x80075814` | 4 | `g_nSpyroDrawSuppressed`. |
| `0x800758A0` | 8 | `g_nSpyroTurnRateAccum` + flame-breath timer save. |
| `0x800758C0` | 4 | `g_nSpyroFallReferenceZ`. |
| `0x80075960` | 4 | `g_nSpyroPitchRateAccum`. |
| `0x80075970` | 4 | `g_nSpyroIdleAnimSeqCursor` — omitting it made P2 walk in place. |
| `0x80076B80` | 4 | `g_CollisionPoint` — collision scratch. |
| `0x800770BC` | 4 | `g_pGemPickupSpyroMirrorActor`. |
| `0x80077368` | 16 | `g_CollisionNormal` — shared, so P1's pitch read P2's normal. |

The last four scalars together are Spyro 1's equivalent of Spyro2x2's 56-byte
`sp2_calculation_results`.

### Camera

| Address | Size | What |
| --- | --- | --- |
| `0x80076DD0` | 272 | `g_Camera` — one struct here (unlike Spyro 2), focus pointer at `+0xD0`. |
| `0x800756B8` | 4 | "Camera forced to destination" flag — set on the hit path. |
| `0x80075894` | 4 | Camera-module file-scope static. |
| `0x80075924` | 4 | L2/R2 rotate speed. |
| `0x80075938` | 4 | Camera-module file-scope static. |

Those four are `$gp`-relative statics, which is why a `%hi` footprint scan
missed them.

### Pad — `Sp1x2SwapPadState`, `Sp1x2Pad.c`

| Address | Table offset | Size | What |
| --- | --- | --- | --- |
| `0x80077378` | `0x2378` | 164 | `g_Pad` — the live derived input block. |
| `0x800776D8` | `0x26D8` | 164 | `g_PadBackup` — input stashed during a lockout. **Was `0x16D8` until 2026-08-28**, a mistyped digit that resolved to `0x800766D8` inside `g_apActorMeshTable`: `g_PadBackup` was never swapped and ~41 mesh pointers were swapped in its place. |
| `0x80075944` | `0x0944` | 1 | `g_PadSwapFlag` — whether that stash happened. |
| `0x800757E0` | `0x07E0` | 4 | `g_ActivePad` — the pointer Spyro's update dereferences (36×). |

### Deliberately *not* per-player

Three real-time clocks are saved and restored around P2's pad poll rather than
swapped, so they cannot tick twice: `g_UnprocessedFrames` (`0x80075760`, also
the buffered-input ring index), `g_nVblankTickCount` (`0x800758c8`),
`g_nCdStallWatchdogTicks` (`0x8007588c`). The player anchor `0x80077798` is
restored to P1's value after P2's tick, so followers track P1.

---

## 5. Known issues and open items

- **Flight-level steering.** Flight/speedway are single-player: P2 frozen,
  mobys single-pass. Measured framerate-sensitive (pitch integrated per substep,
  steering per frame), not shared state. ~10 theories tested. Parked.
- **Simultaneous double death charges one life.** ~7 instructions, but ~190
  bytes of lost cross-jumping in `Sp1x2Spyro.c` and BIOS2 is full. Space, not
  understanding.
- **Camera stratosphere on hits.** `Sp1x2FixFocus` (2026-08-28) repairs a
  null/impossible `g_Camera.m_Focus` either side of P2's camera update. The null
  was measured (focus 0, camera Z 54,150,062 vs a live Spyro Z of 21,058) and
  comes from `func_8003FE40` in Spyro's tick copying `g_Spyro+0x21C`, null for
  P2. **The spasm persists** — read `0x8000ED74`/`0x8000ED78` before concluding.
- **Pedestal respawn dialogue.** Individual respawn at a rescued pedestal
  sometimes auto-opens the save-fairy prompt. A suppression gate froze P2 and
  was reverted. Accepted: press through it.
- **Widescreen edge pop-in.** Distant terrain blinks at the edges at the widest
  fit. Scaling the pure camera matrix was tried twice and reverted (the renderer
  *draws* with it). A real fix widens `r_environment`'s visibility test. Parked.
- **Framerate.** CPU-bound on geometry, drawn twice. Use a 300% overclock, no
  more. The inter-pass `DrawSync` was removed, measured at zero gain, restored.
- **Three/four players.** Rendering is ready (`Sp1x2SetViewport`/`Sp1x2SquashView`
  take quadrants), but four viewports measured exactly half framerate.
  Optimisation first, multitap second.
- **PvP flame** was tried and reverted — see `PVP_FLAME_NOTES.md` before retrying.

Cosmetic, logged, accepted:

- P2's respawn shows a brief odd view — his camera in transit; retail hides it
  behind a level-reload fade we skip.
- The "+3" pickup text appears in both viewports (enqueued once, drawn twice).
- The second dragon snaps into formation on a portal entrance's first frames.
- After a P1-triggered dragon rescue, P2 snaps to P1 (trips the teleport detector).
- Looping voices still attenuate against P1's camera only; one-shots are fixed.
- The HUD cannot be drawn half size: shrinking drags it toward screen centre,
  and compensating pushes it where the moby renderer culls it.

---

## 5b. Fixed 2026-08-29 — handover teleport false-positive

**Symptom:** player 2 talks to the balloonist, and when the conversation ends
player 1 warps to him. Player 1 talking left player 2 alone. Player 2 dying in
water respawned him beside player 1 instead of at the spawn point.

**Cause:** `Sp1x2HandoverResume`'s level-restart detector compares "the live
dragon" against a single stored sample. While a handover is pending the live
dragon is PLAYER 2 but the sample tracks PLAYER 1, so the comparison is a
false teleport by construction. The detector then cleared the ready flag and
returned — skipping the swap-back — which left player 2's state live. On
screen: "player 1 warped to player 2".

**Fix:** skip teleport detection entirely while `SP1X2_P2_HANDOVER` is set,
and re-sample after the swap-back. A real level change in that window is still
caught by the separate `SP1X2_P2_LAST_LEVEL` test.

**Found by instrument, not inspection.** A temporary probe at 0x8000ED40
recorded which of six sites cleared the flag; it reported reason 1 (the
detector) with gamestate 0. A first fix that re-sampled *after* the swap-back
failed, because the detector `return`s before reaching it — ordering, not
arithmetic. Confidence: **OK** (user-confirmed).

**General rule this establishes:** any code that moves or exchanges a live
dragon must update the teleport detector's sample at `0x8000F1C0` in the same
breath — and any code that runs while the players' identities are swapped must
not compare live state against player-1 samples at all.

## 5c. Added 2026-08-29 — death feedback and double-death lives

| Change | Where | Notes | Confidence |
| --- | --- | --- | --- |
| Life HUD appears on death | `Sp1x2Die` | **60 bytes total.** The game already opens the life counter whenever the HUD's copy of the count differs from the real one (`hud.c:253`) — but we must assign them equal or the display rolls up to 99, so a death showed nothing. Nudging the state machine into `HDS_Opening` gets retail's own slide-open, ~60-tick hold and slide-shut for free. The number does not roll DOWN (that animation only increments, `hud.c:284`), but the counter appears showing the new total. Only fires from `HDS_Hidden`, so it cannot interrupt a slide. | `?` untested |
| Double death costs two lives | `Sp1x2Die` | **48 bytes.** When both dragons die together the second arrives with the first already dead, and the stock trigger charged one life for the pair. We now charge the partner's life too. Guarded against driving the count below zero — with one life left a double death is the game over it already was. | `?` untested |
| Handover instrument retired | `Sp1x2Spyro.c` | Freed ~180 bytes. It found the balloonist bug; codes are recorded in CLAUDE.md if it is ever needed again. |

## 5d. Added 2026-08-30 — save-fairy suppression after respawn

| Change | Where | Notes | Confidence |
| --- | --- | --- | --- |
| Save fairy stays quiet until the player leaves his respawn spot | `Sp1x2FairyMute`, `Sp1x2Spyro.c` | Respawning on a rescued dragon platform opened the save prompt at once. **The level overlay's own guard requires the player within 0x200 of the pad AND idling** — `blez` on `g_Spyro.m_idleTimer` (+0x80) at level_10 `0x80080A0C`. Holding that timer at zero fails the guard, so the prompt is suppressed **before the overlay sets any state**. That is the whole point: a 2026-08-22 attempt patched `InitFairyCutscene`'s entry instead and froze player 2, because the overlay writes moby state 6 and freezes the player *before* calling it. **Distance, not a timer** — the first version muted for 120 frames and the fairy simply waited them out, and it only covered player 1's moby pass so player 2 still got the instant prompt. Now: quiet while within 0x800 (horizontal) of the respawn point, re-arms on leaving. That mirrors retail, where the fairy moby holds a "recently talked" state (`+0x48 == 2`) until a counter at `+0x49` reaches 0x10 — which is what walking away does. Evaluated **only in the owning player's pass** — running it in both meant the *other* dragon, far from the respawn point, tripped the distance test and disarmed the mute before it could act (measured: active = 0 with a good position still stored). Owner is packed into the active flag; `Sp1x2Die` learns whose death it is from `SP1X2_TICKING` (0x8000ED50), since the live state alone cannot say. | **OK** |

## 5e. Changed 2026-08-30 — deferred pad poll (interrupt-deadlock fix)

**Symptom:** rare hard freeze where the screen AND the music stop. Every other
freeze kept playing music, because the CD streams in hardware.

**Diagnosis (CP0 at the freeze):** Cause `0x20` = ExcCode 8 = **syscall**,
BadVAddr 0, EPC `0x8005DBA8` — which disassembles to the `syscall` inside
**EnterCriticalSection**. Music stopping means interrupts are off. A deadlock,
not memory corruption.

**Cause (ours):** `Sp1x2TickPlayer2Spyro` and `Sp1x2UpdateCameras` wrapped
*entire game functions* — Spyro's tick and the camera update — in critical
sections. Those can play sounds, kill Spyro, load levels and raycast. Anything
inside that waits on an interrupt waits forever.

**Why they were wide:** the pad callback runs in the VSync interrupt and swaps
pad state; if it fires while player 2 is swapped in, it polls player 1's input
into player 2's slot.

**Fix — deferred poll.** The main loop raises a flag (`0x8000ED54`: 0 free,
1 held, 2 held-and-a-poll-was-skipped) instead of disabling interrupts. The
callback checks it, records the miss and returns. `Sp1x2PadRelease` clears the
flag and performs the skipped poll itself. **Interrupts now stay enabled
throughout, so this deadlock is structurally impossible.** Cost: on a frame
where the callback lands mid-tick, that frame's poll happens just after the
tick instead of just before.

`Sp1x2HandoverResume` keeps its critical section — it wraps two swaps only,
with nothing inside that can wait.

**Paid for by retiring `Sp1x2FixFocus`** (the null-focus repair). It was real —
its counters showed ~6 repairs a session — but it stopped neither the camera
spasm nor any crash, and this freeze had nothing to do with the focus.
`Sp1x2MaskWalk` and `Sp1x2SyncMobyFlags` were relocated between regions to fit.
Confidence: `?` untested.

## 5f. Added 2026-08-30 — release patch pipeline

`make patch` produces `build/release/spyro1-coop.xdelta`, the distributable
artifact, plus a copy of the `.cue`.

An xdelta patch encodes only the **difference** between the player's own disc
and the modded one, so it carries no game data. Measured: **13,111 bytes** for
a 661 MB disc — consistent with it containing our 10,240-byte payload, the 24
patched instructions and xdelta's own overhead, and nothing else.

**The target verifies itself.** It applies the patch it just built to a fresh
copy of the source disc and checks the result matches the built disc
byte-for-byte, failing the build if not. A patch that does not round-trip is
worse than no patch, because it fails on a stranger's machine after they have
downloaded it. It also prints the source disc's SHA-1, so the release notes
can state exactly which copy of the game the patch expects.

Requires `xdelta3` (`brew install xdelta` on macOS).

## 5g. Fixed 2026-08-30 — respawn lands on the ground

**Reported:** respawning in a level dropped the dragon into the air, and he
fell to the floor. Retail never shows this, because retail never respawns
anyone in place — a death reloads the level and plays the entrance animation,
where Spyro flies in and lands under the sequence's control. Our individual
respawn skips the reload, which is the entire point of it, so whatever height
we write is simply where he appears.

A mismatch was expected rather than exceptional: none of the three spawn
sources promises to sit on the floor. `g_Checkpoint.m_StartingPosition` is
consumed by retail during a reload that re-grounds him anyway, and the cached
true start and captured arrival are both sampled on the first gameplay frame
of a level, when the entrance animation has only just handed back control.

**Fix:** ask the game where the floor is, using its own answer to exactly this
question — `func_8004D5EC`, declared in the decompilation's `collision.h` as
"looks for the floor below the specified position" — and stand him on it.

**Two constants make or break this, and the first attempt got both wrong.**

*How far Spyro's origin sits above the floor: 356.* Not a guess —
`checkpoint.c:21` adds exactly this to the checkpoint moby's position when
saving a checkpoint, commented *"move the starting position up a bit to
accommodate for Spyro's hitsphere"*. His own tick agrees: `pete.c` treats him
as grounded while `m_Position.z - m_surfaceBelowSpyro` is within 512.
Omitting it placed him at floor level — 356 units *inside* the ground — and
the collision ejected him upward, reported as being bounced up off the pad.

*How far down to search: `0x10000`.* This is the reach Spyro's own code uses
(`pete.c:1487`). The first attempt used 4096, the figure retail uses for
**mobys**, which was far too short: a spawn point captured while he was still
descending from a level's fly-in sits thousands of units up, the probe found
nothing, and the respawn was left exactly as broken as before.

The result is accepted only if it lands inside the span actually searched;
anything else keeps the stored height, so a failed probe or a sentinel leaves
the previous behaviour rather than dropping a dragon into the void. A floor
found *above* him goes negative and fails the same unsigned test.

**Related, not fixed:** the arrival capture is taken on the first gameplay
frame of a level, which can be before he has finished descending from the
fly-in. Grounding the respawn corrects the symptom wherever the stored height
came from; capturing the position only once `m_airTime` is zero would fix it
at source, and is the better change if space ever allows.

**Where it lives.** `Sp1x2Ground.c` exists as its own file purely for
placement: code is assigned to a region per object file, BIOS2 was full to the
byte, and LOADER had room. It takes the teleport detector's three position
stores with it, which pays for the call — BIOS2 came out of the change with 16
free bytes where it had none. Both regions are now within a few bytes of full,
which is why that file carries two values across the call rather than three
and uses one unsigned window test rather than two signed comparisons. Neither
is a shortcut around a safety check.

No new hook: this rides the existing death handler on hooks 17 and 18.

## 5h. 2026-09-01 — a second build of the mod exists, inside the decompilation

**This file describes the STANDALONE mod** — the one that patches 24
instructions into the retail executable, and the one v0.1 ships. That remains
the released artifact and everything above still describes it exactly.

There is now a second construction of the same mod, built from
TheMobyCollective's decompilation, in `reference/spyro-1` on the `port`
branch. Same behaviour, same logic, different assembly:

| | standalone (this file) | decomp port |
| --- | --- | --- |
| Hooks | 24 patched instructions | direct calls in C, 3 in assembly |
| Game symbols | 99 raw addresses | real symbols |
| Collision guards | entry patches | ordinary wrappers |
| Delivery | hand-built payload + boot stub | linker sections + boot copier |
| Overlays | cannot be touched | rebuilt from source |

**The memory map in section 3 is unchanged and still authoritative for both.**
The port's code lives in the same BIOS scratch regions at the same addresses,
because the executable turns out not to be able to grow — see
`docs/decomp/README.md`, which is the port's own record.

Two scripts support it: `mod/scripts/check_names.py` (every ported name
resolves to the address the mod expects, and no raw game addresses survive)
and `mod/scripts/overlay_map.py` / `inject_overlays.py` (locating and
replacing overlays inside `WAD.WAD`).

## 6. Build verification — what `make` checks every time

`scripts/verify.py` runs at the end of every `make`. Each check exists because
something once went wrong that the previous gate could not see.

| Check | Catches |
| --- | --- |
| Payload size is a multiple of 2048 | a `t_size` that hangs the BIOS at the PlayStation logo |
| **Every changed byte lies inside a declared hook** | a hook placed 4 bytes off — the old gate counted bytes but never located them, and that exact bug caused a soft-lock |
| **Each hook reaches the function `main.S` names** | right address, wrong target |
| Real free space per region | measured from the built payload, not by summing symbols (which understates by the padding between objects) |
| **No function of ours calls itself** | a scripted edit once made a helper recurse; it looked like a 752-byte saving and would have crashed on frame one |
| Memory map has no overlaps | two allocations claiming one address, as happened with a retired instrument and player 2's camera |
| **Every hook is documented here** | this file silently drifting out of date |

The last one makes the rule at the top of this document enforceable: a hook
added without a row here fails the build.

**Independently verified 2026-08-29 — every byte of the difference between our
build and vanilla Spyro 1 is accounted for:**

| Source of difference | Bytes |
| --- | --- |
| PS-EXE header (`header.S` patches `t_size` and `PC`) | 4 |
| LOADER code, living in the header padding `0x80-0x7FF` | 1,309 |
| The 24 declared hooks | 55 |
| Appended payload (BIOS2 + BIOS2B) | 10,240 |
| **Unexplained** | **0** |

## 7. Build-system changes (2026-08-28)

| Change | File | Effect |
| --- | --- | --- |
| `-ffunction-sections -fdata-sections` added | `env.mk` | Each function gets its own section so the linker packs them without per-object padding. **+152 bytes, all in BIOS2B.** Verified safe by a symbol-by-symbol diff: nothing was dropped. |
| `.o(.text)` -> `.o(.text .text.*)` | `coop.ld` | Required by the above — with function-sections the code lands in `.text.<name>`, and `--orphan-handling=discard` would otherwise silently delete it. |
| `.rodata` moved BIOS2 -> BIOS2B | `coop.ld` | **+240 bytes in BIOS2**, the region that was full. Costs nothing: `-G0` addresses all data absolutely, so relocating it changes a constant, never an instruction — the functions are byte-identical either side. |
| Unsigned halving in `Sp1x2SetViewport` | `Sp1x2Graphics.c` | **+64 bytes.** Dividing a *signed* value by two makes the compiler emit a round-toward-zero correction at each of six sites; a screen width is never negative, so the correction was dead weight. |
| `Sp1x2SwapAll()` helper | `Sp1x2Spyro.c` | **+28 bytes.** Four sites wrote the camera+Spyro+pad swap longhand in opposing orders. The three swap sets were verified to touch **no overlapping memory** (nearest approach: `g_CollisionNormal` ends 0x80077377, `g_Pad` starts 0x80077378), so order is irrelevant and one helper serves all. |
| `scripts/rm.py` restored | new file | **`make clean` had been broken since the start** — `env.mk` defines `RM = python rm.py` but the file did not exist, so every `make clean` failed. It fails loudly on the command line but was easy to miss when output was redirected. The visible symptom was a stale-object link error after editing a header, which `make clean` was supposed to fix and could not. |

`--gc-sections` was deliberately NOT added: the mod has no conventional entry
point, so the linker would have no root set and could discard everything.

## 6. Documentation inconsistencies (comment vs code)

Fixed on 2026-08-28: the `g_PadBackup` offset typo; the stale `symbols.ld`
symbols `InitFairyCutsceneBody`, `CameraFrameBody` and `sp1x2_fairy_mute` (the
last aliased the live `SP1X2_LAST_SEQ`); the dead leak-sweep instrument.

Still outstanding:

- `main.S` header says "HOOK LIST (15)" — there are **24** (the Makefile is right).
- `Sp1x2Spyro.c` map header says BIOS3 is `0x8000F000`/7168 — it is
  `0x8000E800`/6144, and the map's own entries start below its stated range.
- Same map calls `0x8000F100 +0x100` free — it holds the four flame chains.
- Same map calls `0x8000ED20` the rumble-guard buffer — retired; it holds the
  arrival capture, and the true-start cache runs 4 bytes past it.
- Same map claims 1024-moby capacity and sizes `0x400`/`0x800`/`0x400` — the
  code caps at **512** everywhere; real sizes are `0x200`/`0x400`/`0x200`.
- Same map's swap-table note says "12 ranges, 1065 bytes" — it is 15 / 1092.
- Same map omits `0x8000ED0C`, `0x8000ED60`–`0x8000ED7C`,
  `0x8000F180`–`0x8000F1D8`, `0x8000F3F0`, `0x8000F400`/`F500`, `0x8000F600`.
- `Sp1x2Graphics.c:29` says "Marker RAM at `0x8000C000`" — the `#define` below
  correctly uses `0x8000F000`; `0x8000C000` is BIOS2 code.
- `Sp1x2Boot.c`/`Sp1x2Sparx.c` prose still says BIOS2B `0x8000E600`/512 and
  BIOS3 `0x8000F000`/7168; the code itself is correct.
- Two `Sp1x2Spyro.c` comments still credit `Sp1x2CamGate`, deleted 2026-08-27.
- `Sp1x2Gates.c:125` says `Sp1x2FixFocus` is in `Sp1x2Spyro.c` — it is further
  down that same file.
- `CLAUDE.md` still lists `Sp1x2HitLatch` at `0x8000ED50` (removed; unallocated)
  and gate counters at `0x8000E4C8`/`E4EC` (moved to `0x8000ED60`/`64` — the old
  addresses are now BIOS2B **code**).
