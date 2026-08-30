# PvP flame damage — attempted 2026-08-22, REVERTED

Optional friendly fire: let one dragon's flame breath hurt the other, as a
switch on the Multiplayer pause menu. Built, never worked, then froze the game.
Reverted the same day at the user's request.

**Everything below is verified fact unless it says otherwise.** The reverted
code is gone, but none of the reverse-engineering is — that is the point of
this file. If you pick this up again, start at "If you try again".

---

## What was built

- `SP1X2_PVP` setting at `0x8000F1F0`, toggled from a fifth Multiplayer-menu
  row ("FLAME PVP ON/OFF")
- `Sp1x2PvpFlame()` in `Sp1x2Pad.c`, called from the end of
  `Sp1x2TickPlayer2Spyro` (just after `Sp1x2SeparatePlayers()`), which:
  measures the distance between the dragons, checks each one's flame-active
  byte, and applies damage to the other with the victim swapped in.

Reverted in full: `Sp1x2Pad.c` (the whole block), the call site in
`Sp1x2Spyro.c`, three declarations and `SP1X2_PVP` in `Sp1.h`, two symbols in
`symbols.ld`, and the menu back to four rows. `Sp1x2SwapSpyroState` went back
to `static`.

---

## What we PROVED (all measured live in PCSX-Redux, not assumed)

These parts worked. Do not re-derive them.

| Fact | How it was confirmed |
| --- | --- |
| The setting reaches the logic | marker read 1 after toggling the menu |
| The function runs every frame | call counter climbed continuously |
| **Flame-active byte for P1 = `0x80078760`** (`g_SpyroFlame + 0x98`) | read 1 exactly while breathing fire |
| **Flame-active byte for P2 = `0x8000E800 + 676 + 0x98`** (his shadow copy) | read 1 exactly while P2 breathed fire |
| Distance between dragons computes correctly | value tracked the dragons moving apart |

So detection was never the problem. **The problem is entirely in applying the
damage.**

## The damage system, as actually disassembled

`HandleSpyroDamage` = `0x80040F68` (decompiled in `spyro-1/src/pete.c:1027`;
address decoded from the `jal` inside `func_80041670`).

Its guard chain, read straight off the disassembly at `0x80040F68`-`0x80040FE4`:

```
if (g_NextLevelId != g_LevelId)      return 0;      // level transition
if (m_invulverabilityTimer != 0)     pFlags &= 0xFFFFFE0E;   // kills 0x20
pFlags &= g_Spyro.m_DamageFlags;                    // <-- THE BLOCKER
if ((pFlags & 0x5F1) == 0)           goto done;     // real-damage bits
if (g_Spyro.m_health < 0)            goto done;
```

**The key discovery: `m_DamageFlags` (`g_Spyro + 0x2C` = `0x80078A84`) is NOT
"what damage is allowed".** It is the set of hazards *physically touching*
Spyro this frame, written by the collision and special-surface passes
(`spyro-1/src/special_surfaces.c` writes it as
`(m_DamageFlags & 0x3FF) | 0x400` etc.). spyro-1's header comment — "the damage
that's been applied" — reads the other way round and is what misled us.

Nothing is touching Spyro when we ask, so our flag was ANDed to zero and the
handler returned immediately. That is why the call ran every frame and did
nothing at all.

The flag value `0x20` was never wrong: it is in the accepted set `0x5F1`
(= bits `0x1|0x10|0x20|0x40|0x80|0x100|0x400`) and is the hazard case, which
plays the shock sound and hurt state.

**Useful offsets, all confirmed from the disassembly rather than a header:**

| Field | Offset | Address |
| --- | --- | --- |
| `m_DamageFlags` | `g_Spyro + 0x2C` | `0x80078A84` |
| `m_invulverabilityTimer` | `g_Spyro + 0x160` | `0x80078BB8` |
| `m_health` | `g_Spyro + 0x164` | `0x80078BBC` |
| god mode | — | `0x800756A0` |

The whole `0x20` branch is only three effects (`pete.c:1064-1087`):

```c
g_Spyro.m_health--;                                    // unless god mode
func_8003EA68(7);                                      // state 7: hurt by hazard
if (m_invulverabilityTimer < 90) m_invulverabilityTimer = 90;
```

`func_8003EA68` = `0x8003EA68`, the state setter (still `INCLUDE_ASM` in
spyro-1). Every damage path ends in a call to it: 7 = hazard hurt,
14 = generic hurt, 0x1F = death.

## The two attempts, and how each failed

**Attempt 1 — force the contact bit.** Set `0x20` in the victim's
`m_DamageFlags`, call `HandleSpyroDamage(0x20)`, restore the field.

*Never actually tested.* `make` in this project stops at the verification
gate; the disc is a separate `make disc`. The build under test was 18 minutes
stale, so this was reported as failing when it had never run. **Lesson: `make`
alone does not produce a testable disc.**

**Attempt 2 — apply the three effects directly**, bypassing the handler:
`health--`, `SetSpyroState(7)`, `invuln = 90`, guarded by the invulnerability
timer.

**Result: complete freeze the moment P1 flamed P2.**

## Why it probably froze — UNVERIFIED, the best lead for next time

`SetSpyroState(7)` was called from the **end of the tick hook, outside the
state machine's own update**, and for player 2 with his state swapped in.

This is the same shape as the two death-handling crashes recorded in
`CLAUDE.md`: forcing a state transition from outside the sequence that owns it
leaves the animation, camera and sequence bookkeeping half-initialised. Those
crashes were only fixed by letting the game's own sequence run and *not*
swapping the state back out.

Suspicion, untested: state 7 sets up a hurt sequence that expects to be entered
from inside the tick, and entering it after the tick — then immediately swapping
that dragon's state back to shadow — leaves the live state inconsistent.

## If you try again

Follow the research order in `CLAUDE.md` (Spyro2x2 first, then spyro-1, then
build). Specifically:

1. **Re-read `Sp2x2Flame` properly.** Spyromain does NOT call a damage
   function. He sets a bit on the victim and lets the engine notice:

   ```c
   Sp2Vec3Sub(d, sp2x2_spyro, flame_pos);
   if (Sp2Hypot(d) <= radius)  sp2x2_spyro[36] |= 1;
   ```

   That is the whole implementation, and it is almost certainly the right
   shape here too: **set the contact bit and let the victim's own tick consume
   it**, rather than reaching in and applying damage ourselves.

2. **So: set `m_DamageFlags |= 0x20` on the victim BEFORE his tick runs, and
   leave it.** The engine's tick then calls `HandleSpyroDamage` at its own
   natural point, inside the state machine, with every guard intact — no
   forced state transition, no freeze. Our first attempt had the right idea
   but did it at the wrong moment and then called the handler anyway.

3. **Find out what clears `m_DamageFlags` each frame** before relying on this.
   `HandleSpyroDamage` never writes it back (checked — no store to
   `0x80078A84` anywhere in the function), and the special-surface writers
   preserve the low 10 bits, so something else must reset it or the bit would
   stick and damage forever. Not yet identified.

4. Range: `0x600` world units, centre to centre, felt about right in code but
   was never play-tested. `SP1X2_BODY_RADIUS` (the push-apart distance) is
   `0x1A0`, so `0x400` required the dragons to be nearly touching.

5. Consider whether flame should read as `0x20` (hazard: shock sound, state 7)
   or something gentler. `0x5F1` lists the alternatives.

## Process notes worth keeping

- **Check `make disc` ran.** Comparing `build/rom/SCUS_942.28` against
  `build/disc/spyro1-coop.bin` timestamps catches a stale disc instantly. An
  untested change reported as "not working" costs a whole cycle and, worse,
  discards a theory that was never actually tried.
- The five-marker diagnostic (setting / call count / both flame flags /
  distance) worked exactly as intended — it eliminated the entire detection
  half in one reading and pointed straight at the damage call. Keep using that
  pattern: one marker per decision step, not one per guess.
