# Recolouring Spyro — what we found

Investigated 2026-08-30, prompted by the Mod the Dragon Discord: the Spyro 1
Archipelago mod changes Spyro's colour "via a filter hex at 0x78a80". That
turned out to be a real, documented field, and the tip saved us from a much
worse approach.

Two methods exist. **The first works today and costs almost nothing. The
second is the "proper" one and is a real project.**

---

## Method 1 — the colour filter (works, nearly free)

`g_Spyro.m_colorFilter`, four bytes at **`0x80078A80`** (`g_Spyro + 0x28`):

| Offset | Byte | Meaning |
| --- | --- | --- |
| `+0x28` | red | 0-255 |
| `+0x29` | green | 0-255 |
| `+0x2A` | blue | 0-255 |
| `+0x2B` | strength | 0 = off, 0xFF = fully replaces his colours |

`spyro.h` documents it as *"interpolated with vertex colours, used by Fairy
Kiss effect"* — it is the flash you see when a fairy kisses you.

**Why it is nearly free for us:** the field lives *inside* `g_Spyro`, and the
mod already swaps that entire structure between the two players every frame.
Per-player colour therefore needs **no new memory** — just a different value
in each player's own state.

**It must be re-applied every frame.** The game clears it on certain state
changes (`pete.c:1859` zeroes the strength, `pete.c:1901` clears all four
bytes). Confirmed in play: level transitions, entering a level and dying all
reset it.

**Its limitation, and the reason Method 2 exists:** it tints the *whole*
dragon uniformly — horns, belly and eyes shift with the body. Lower strengths
(`0x20`-`0x60`) read as a wash rather than a repaint and keep him recognisable,
but you cannot recolour only the purple this way. The next section explains
why not.

**To try it by hand:** in PCSX-Redux write four bytes at `0x80078A80` — e.g.
`ff 00 00 40` for a gentle red. It affects whichever dragon is currently live.

---

## Method 2 — the texture palette (the real fix, not yet attempted)

### Why the model data cannot do it

Spyro's model is `g_Models[0]`. Following it to an animation and reading its
colour table gives **123 entries**, and every single one is greyscale:

```
595959  4c4c4c  5e5e5e  c4c4c4  e2e2e2  ffffff  000000
fcfcfc  070707  eaeaea  7a7a7a  3a3a3a  7f7f7f  444444
```

Red = green = blue throughout, with `ff` opacity. **That is a brightness
table, not a colour palette** — per-vertex lighting applied on top of the
texture. Spyro's purple is not in the model at all, which is also why the
filter washes him uniformly: there is no colour in this data to preserve.

### Where the colour actually is

In his **texture**, and specifically in the texture's **CLUT** (colour lookup
table) in VRAM. There his purples, golds and creams *are* separate entries, so
changing only the purple ones would give exactly what we want: a red dragon
with gold horns and a cream belly.

### What a real implementation needs

1. **Find Spyro's CLUT in VRAM.** Not main RAM — reaching it means a GPU
   transfer, not a memory write.
2. **A second CLUT in spare VRAM**, holding the recoloured entries. Editing
   the original in place will not work: both dragons are drawn from the same
   texture in the same frame, so they would change together.
3. **Point player 2's draw at the second CLUT.** PS1 texture primitives carry
   a CLUT id, so this is how palette swaps were done on the hardware — and it
   is O(1) per draw rather than a per-frame upload. It needs a hook wherever
   `r_pete` sets that id.

**Open question for anyone who knows the hardware better:** the Discord
mentioned spare VRAM being available. The blocker may be VRAM *space*, or it
may be that we simply have not worked out how to write into it — those are
different problems and we have not established which we face.

---

## Caveat on the addresses above

The walk that found the colour table was:

```
0x80076378              g_Models
  -> [0]                g_Models[0], Spyro's model
  -> +0x38              first animation header
  -> +0x18              its colour table (123 entries)
```

The **intermediate addresses are level-specific** — models are loaded per
level, so the pointers differ every time. Only `0x80076378` (the model table)
and `0x80078A80` (the filter) are fixed. Re-walk the chain rather than reusing
noted values.

Struct layouts are in `reference/spyro-1/include/moby.h` (`Model`,
`AnimationHeader`) and `include/spyro.h` (`m_colorFilter`).

---

## Recommendation

Ship **Method 1** as a Multiplayer-menu option — it works, it is cheap, and a
well-chosen tint makes the two dragons clearly distinguishable, which is the
actual goal. Keep **Method 2** as a future feature for when there is code
space and appetite for VRAM work.
