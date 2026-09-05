# Porting to OpenPete — what the SDK does and does not allow

Assessed against **OpenPete v0.2.0 (Windows)**, whose `sdk/` ships headers,
HTML docs and seven example mods. Everything here is read from those shipped
files, so anyone with the same download can check it.

[OpenPete](https://openpete.com/) is a native PC port of Spyro 1, built on the
same [TheMobyCollective decompilation](https://github.com/TheMobyCollective/spyro-1)
this mod is compiled inside. That shared base is why the vocabulary lines up so
exactly.

## The hooking model is a superset of ours

Every recompiled guest function dispatches through one chokepoint, so
`override_addr` intercepts every call to it — direct or indirect — with no code
patching. Pre-hook, post-hook and full replacement are the same mechanism,
distinguished by where the override calls `api->base(cpu)`. `api->call(cpu, addr)`
invokes a guest function through the same layer.

That is this mod's architecture exactly, and better: we patch 24 individual
`jal` instructions at their **call sites**, so a call from anywhere we did not
patch is missed. An override catches the function itself.

## Every address we use is already a named constant

| ours | OpenPete's `openpete_sdk_symbols.h` |
| --- | --- |
| `g_Spyro` `0x80078A58` | `OP_GADDR_g_Spyro` |
| `g_Camera` `0x80076DD0` | `OP_GADDR_g_Camera` |
| `g_Pad` `0x80077378` | `OP_GADDR_g_Pad` |
| `g_Hud` `0x80077FA8` | `OP_GADDR_g_Hud` |
| second pad buffer `0x80078E50` | `OP_GADDR_g_PadBufferSecondController` |
| `TickSpyroGameplayFrame` | `OP_FNADDR_func_8004A200` |
| the null-focus site | `OP_FNADDR_func_8003FE40` |
| both collision guards | `OP_FNADDR_func_8004BE4C` / `func_8004AE38` |

Struct layouts are pinned with `_Static_assert`, so a layout change fails the
mod's compile rather than corrupting memory silently — which is the class of
fault that cost this project the `g_PadBackup` offset bug.

## What ports, and what does not

**Ports.** The per-player state model, the swap tables, the moby ownership
partition with its hysteresis, handover, individual death and respawn, Sparx,
and the dual pad polling. `api->guest(vaddr)` gives a host pointer into guest
RAM, and `guest_alloc` provides state that savestates and rewind carry
correctly — so BIOS scratch, the boot payload, `t_size` arithmetic and every
byte-golf compromise in this codebase simply go away.

**Does not port.** Everything PS1-specific: `DRAWENV`/`DISPENV`, the ordering
table, the GTE view-matrix squash, the entry-patch technique.

**The gap, as of v0.2.0.** The mod API is 28 functions. The rendering-adjacent
ones are `postfx_*` (shaders over the finished frame), `material_*`
(re-shading what the engine draws), `draw_text` and `register_present_hook`.
**None renders the world a second time, sets a per-pass camera, or sets a
viewport rectangle** — which is what split-screen is.

That is structural, not an oversight: the engine simulates at 29.913 ticks per
second and may present several interpolated frames per tick, and mods are
explicitly forbidden from touching guest state during a present. Rendering is
the engine's business.

## What a split-screen API would need

Recorded here because it is the concrete requirement, whatever shape the
eventual API takes. Per frame:

1. render the scene **N times**, N chosen by the mod;
2. per pass, a **camera** (position and rotation);
3. per pass, a **destination rectangle** on screen;
4. per pass, a **projection scale** — this mod squashes the view matrix so a
   whole scene fits a half-height or half-width viewport;
5. the **HUD positioned per pass**, since it is screen-space.

## Practical notes

- **Windows only** at v0.2.0. A Linux AppImage is in testing and macOS on
  Apple Silicon is planned with no build yet — so this development cannot
  happen on the machine the rest of this project lives on.
- **Determinism is the engine's headline promise** ("bit-identical to original
  PlayStation hardware"), and the SDK is built around savestates, rewind,
  runahead and replays. Running the tick twice per frame with swapped state
  diverges from that by construction. Whether the engine tolerates a mod that
  does so is an open question and worth settling early.
- **Performance stops being the wall.** On PS1 this mod is CPU-bound: drawing
  the scene twice halves the framerate and the four-viewport probe halved it
  again, with the 300% overclock already at its ceiling. Native geometry and a
  host GPU remove that constraint, so 3- and 4-player split becomes a question
  of API rather than of budget.
