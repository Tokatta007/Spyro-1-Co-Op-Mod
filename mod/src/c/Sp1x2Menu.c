#include "Sp1.h"

/*
 * MULTIPLAYER PAUSE MENU (2026-08-19).
 *
 * Replaces the temporary SELECT/START toggles with a real settings screen,
 * reached from the game's own options menu by pressing SQUARE.
 *
 * WHY A SUBMENU OF OUR OWN, rather than extra rows in the options list:
 * the options menu owns its cursor inside PauseMenu_Update, a compiled
 * 0xa00-byte function whose bounds are hard-coded (`>= 6` wraps to 0, `< 0`
 * wraps to 5). Adding rows there means patching stock logic. Instead we run
 * our own substate, draw it ourselves, and handle our own input — the game's
 * menu code is untouched.
 *
 * The canvas is free: the pause draw only renders the options list for
 * substate 1 and the quit prompt for substate 2, so ANY other value draws
 * just the frame and the word PAUSED. We park on substate 3 and draw inside
 * it, then restore substate 1 on the way out.
 *
 * Two hooks:
 *   7. 0x8001a980  jal BuildTextSpriteChain ("PAUSED") -> Sp1x2PauseDraw
 *   8. 0x800338b8  jal PauseMenu_Update                 -> Sp1x2PauseUpdate
 *   9-12. the four main-menu item text calls             -> Sp1x2MainMenuItem
 *
 * Derived from Spyromain's Spyro2x2 (MIT) in spirit only — his mod has no
 * menu. See LICENSE.
 */

#define SP1X2_MENU_ACTIVE  (*(volatile int *)0x8000F1A0)
#define SP1X2_MENU_CURSOR  (*(volatile int *)0x8000F1A4)
#define SP1X2_MENU_SUBSTATE 3          /* our private pause substate */
#define SP1X2_MENU_ROWS    5           /* players, split, widescreen, view, done */

#define SP1_PAD_DOWN_BITS  (*(volatile int *)0x80077378)   /* g_Pad.m_Down */

#define PAD_TRIANGLE (1 << 4)
#define PAD_CROSS    (1 << 6)
#define PAD_SQUARE   (1 << 7)
#define PAD_UP       (1 << 12)
#define PAD_RIGHT    (1 << 13)
#define PAD_DOWN     (1 << 14)
#define PAD_LEFT     (1 << 15)

/* Vector3D — THREE INTS, per spyro-1's include/vector.h. Getting this wrong
   (shorts) made every coordinate garbage, so the text drew far off-screen and
   the menu appeared to do nothing at all. */
typedef struct { int x, y, z; } Sp1Vec;

static void Sp1x2SetVec(Sp1Vec *v, int x, int y, int z)
{
    v->x = x; v->y = y; v->z = z;
}

/* The mobys the text routines build into, plus the wobble inputs. Addresses
   in symbols.ld; all read straight out of the stock menu's own code. */
#define SP1_HUD_MOBYS   (*(volatile unsigned char **)0x80075710)
#define SP1_COS_TABLE   ((volatile short *)0x8006cc78)   /* SIGNED — spyro-1
    declares it `extern short D_8006CC78[256]`. Reading it unsigned turned the
    negative half of the wave into huge positives, which spun letters fully
    backwards at random. */
#define SP1_TEXT_TICKS  (*(volatile int *)0x800758b8)
#define SP1_MOBY_ROT_Z  0x46            /* m_Rotation.z, from the stock loop */
#define SP1_SOUND_TABLE (*(volatile unsigned char **)0x800761d4)
/* Sound-table offsets, counted from sound_table.h's `sound_0x27` marker
   (which fixes that field at 39): menuCursor lands on 45, menuConfirm on 46.
   Using 46/47 gave the confirm sound for movement and an unrelated sound for
   select. */
#define SP1_SND_MOVE    45              /* menuCursor  */
#define SP1_SND_PICK    46              /* menuConfirm */

/* Set by the selected row so the wobble can be applied after everything is
   drawn — exactly the stock pattern (`curMoby = g_HudMobys; mobyCount = N`).
   These live at fixed addresses, NOT as C statics: statics land in an orphan
   .bss that our linker script discards (see CLAUDE.md). */
#define sp1x2_sel_mobys (*(volatile unsigned char **)0x8000F1A8)
#define sp1x2_sel_count (*(volatile int *)0x8000F1AC)
/* The value text is a SEPARATE BuildTextSprites call, so it gets its own
   mobys — wobbling only the label left "HORIZONTAL" sitting still next to a
   rippling "SPLIT". */
#define sp1x2_sel_mobys2 (*(volatile unsigned char **)0x8000F1B0)
#define sp1x2_sel_count2 (*(volatile int *)0x8000F1B4)

static int Sp1x2StrLen(const char *s)
{
    int n = 0;
    while (s[n] != 0) {
        n++;
    }
    return n;
}

/* The stock shimmer, ported: each letter of the selected item rotates on a
   cosine, phase-shifted along the word so it ripples. */
static void Sp1x2WobbleRange(volatile unsigned char *mobys, int count,
                             int phase_base)
{
    int i;

    if (mobys == 0) {
        return;
    }
    for (i = 0; i < count; i++) {
        int phase = (SP1_TEXT_TICKS * 8 + (phase_base + i) * 12) & 0xFF;
        int rot = ((int)SP1_COS_TABLE[phase] * 3) >> 9;
        mobys[i * SP1_MOBY_STRIDE + SP1_MOBY_ROT_Z] = (unsigned char)rot;
    }
}

static void Sp1x2WobbleSelected(void)
{
    /* The value continues the label's phase, so the ripple runs across the
       whole row rather than restarting in the middle. */
    Sp1x2WobbleRange(sp1x2_sel_mobys, sp1x2_sel_count, 0);
    Sp1x2WobbleRange(sp1x2_sel_mobys2, sp1x2_sel_count2, sp1x2_sel_count);
}

/* One menu row: label on the left, current value on the right, with the
   selected row marked by a leading arrow (the stock menus animate spinning
   moby icons for this; text is simpler and needs no moby allocation). */
/* Shade indices for BuildTextSprites' last argument. That argument becomes the
   letter moby's m_SpecularMetalType, which — because the text builder memsets
   the moby, leaving the renderer flags at 0 (the SHADED path) — is an index
   into D_8006E44C, described in spyro-1's moby.h as the "specular shaded color
   list", 17 entries.
   11 is what every stock menu uses. 12 was found via moby_helpers.c:1363,
   which colours gems with D_8006E44C[12 + (m_Class - MOBYCLASS_GEM_1)] — so
   entries from 12 up are gem-related. OBSERVED: 12 renders GREY, not the red
   the 1-point gem would suggest, so the list is not simply the gem colours in
   class order and the entries above 12 are unmapped. Grey reads as "disabled"
   at least as well as red, so it stayed. Anything else here is trial and
   error, not lookup. */
#define SP1X2_SHADE_NORMAL   11
#define SP1X2_SHADE_DISABLED 12   /* grey — verified on screen */

static const char sp1x2_digits[] = "1\0" "2";

static void Sp1x2MenuRow(int row, short y, const char *label, const char *value,
                         int shade)
{
    Sp1Vec pos, spacing;

    Sp1x2SetVec(&spacing, 15, 1, 0x1600);

    /* No cursor glyph: the wobble marks the selection, exactly as the stock
       menus do, and dropping it buys code space in a very tight BIOS2. */
    Sp1x2SetVec(&pos, 114, y, 0x1400);
    BuildTextSprites(label, &pos, &spacing, 16, shade);

    if (SP1X2_MENU_CURSOR == row) {
        /* g_HudMobys now points at the letters just built. */
        sp1x2_sel_mobys = SP1_HUD_MOBYS;
        sp1x2_sel_count = Sp1x2StrLen(label);
    }

    if (value != 0) {
        Sp1x2SetVec(&pos, 272, y, 0x1400);
        BuildTextSprites(value, &pos, &spacing, 16, shade);

        if (SP1X2_MENU_CURSOR == row) {
            sp1x2_sel_mobys2 = SP1_HUD_MOBYS;
            sp1x2_sel_count2 = Sp1x2StrLen(value);
        }
    }
}

static void Sp1x2MenuAdjust(int delta)
{
    switch (SP1X2_MENU_CURSOR) {
    case 0:
        /* Drop-in / drop-out. Going to 1 also clears the seeded flag, which is
           what actually removes player 2 — every co-op path returns early on
           it. Going back to 2 leaves it clear, so he is re-seeded beside
           player 1 on the next drawn frame. */
        SP1X2_PLAYERS = SP1X2_SOLO ? 2 : 1;
        *(volatile int *)0x8000ED00 = 0;      /* SP1X2_P2_READY */
        break;
    case 1:
        SP1X2_SPLIT_MODE = (SP1X2_SPLIT_MODE == SP1X2_SPLIT_VERTICAL)
                           ? 0 : SP1X2_SPLIT_VERTICAL;
        break;
    case 2:
        SP1X2_WIDESCREEN = !SP1X2_WIDESCREEN;
        break;
    case 3: {
        int fit = SP1X2_VIEW_FIT + delta;
        if (fit < 0) {
            fit = SP1X2_FIT_CROPPED;
        } else if (fit > SP1X2_FIT_CROPPED) {
            fit = 0;
        }
        SP1X2_VIEW_FIT = fit;
        break;
    }
    default:
        break;
    }
}

extern void Sp1x2MenuChime(int which);   /* moved to Sp1x2Sparx.c (BIOS2B) */


static void Sp1x2MenuClose(void)
{
    SP1X2_MENU_ACTIVE = 0;
    g_nPauseMenuSubstate = 1;      /* back to the options screen */
    g_nPauseMenuCursor = 0;
}


/* Hooked over `jal PauseMenu_Update` at ram 0x800338b8. */
void Sp1x2PauseUpdate(void)
{
    int down = SP1_PAD_DOWN_BITS;
    int chime;

    if (SP1X2_MENU_ACTIVE == 0) {
        PauseMenu_Update();

        /* SQUARE on the options screen opens our menu. SQUARE is unused
           there — the stock handler only looks at UP/DOWN/LEFT/RIGHT/CROSS
           and TRIANGLE. */
        if (g_nGamestate == 2 && g_nPauseMenuSubstate == 1 &&
            (down & PAD_SQUARE)) {
            SP1X2_MENU_ACTIVE = 1;
            SP1X2_MENU_CURSOR = 0;
            g_nPauseMenuSubstate = SP1X2_MENU_SUBSTATE;
            /* Park the GAME's cursor somewhere with no wobble case.
             * The stock menu makes the selected item's letters wobble by
             * rotating mobys: `curMoby = g_HudMobys; mobyCount = <letters>`,
             * then spinning that many. Those mobys are whatever text was
             * built most recently — which is now OURS. With the game's cursor
             * left on 0-3 it wobbled a matching COUNT of our letters (hence
             * "pped" in CROPPED, and DONE, shimmering). Cursor 4/5 has no
             * case in the main-menu branch, so mobyCount stays 0 and nothing
             * wobbles. */
            g_nPauseMenuCursor = 5;
            Sp1x2MenuChime(SP1_SND_PICK);
        }
        return;
    }

    /* Our menu owns input while it is open, so the stock update is NOT
       called — otherwise it would act on the same presses. But the stock
       update also does the pause screen's HOUSEKEEPING, and skipping it froze
       the HUD icons and the shimmer on "PAUSED". These three lines are that
       housekeeping, lifted from the top of PauseMenu_Update. */
    HudTick();
    SpecularUpdate(3);
    g_nPauseMenuIdleFrames += 1;

    /* One chime call at the end rather than one per branch — BIOS2 is tight
       (0x8000C000-0x8000E000 is a hard ceiling; see Sp1x2Boot.c). */
    chime = -1;
    if (down & PAD_DOWN) {
        SP1X2_MENU_CURSOR = (SP1X2_MENU_CURSOR + 1) % SP1X2_MENU_ROWS;
        chime = SP1_SND_MOVE;
    } else if (down & PAD_UP) {
        SP1X2_MENU_CURSOR =
            (SP1X2_MENU_CURSOR + SP1X2_MENU_ROWS - 1) % SP1X2_MENU_ROWS;
        chime = SP1_SND_MOVE;
    } else if (down & PAD_RIGHT) {
        Sp1x2MenuAdjust(1);
        chime = SP1_SND_PICK;
    } else if (down & PAD_LEFT) {
        Sp1x2MenuAdjust(-1);
        chime = SP1_SND_PICK;
    } else if (down & PAD_TRIANGLE) {
        chime = SP1_SND_MOVE;
        Sp1x2MenuClose();
    } else if (down & PAD_CROSS) {
        chime = SP1_SND_PICK;
        if (SP1X2_MENU_CURSOR == 4) {
            Sp1x2MenuClose();
        } else {
            Sp1x2MenuAdjust(1);
        }
    }
    if (chime >= 0) {
        Sp1x2MenuChime(chime);
    }
}


/* Sp1x2MainMenuItem moved to Sp1x2Sparx.c (BIOS2B) — 2026-08-23 rebalance. */


/*
 * Hooked over the `jal BuildTextSpriteChain` at ram 0x8001a980 — the call that
 * draws the word "PAUSED".
 *
 * WHY HERE, and not around the whole pause draw: wrapping
 * Gamestate02_03_06_Draw was tried and MEASURED — the hook ran (counter
 * climbing, gamestate 2), but nothing appeared, not even a string drawn with
 * this very routine. By the time that function returns the frame is already
 * composed, so anything appended is too late. This call site sits INSIDE the
 * pause draw, runs in every substate, and is immediately followed by the
 * substate's own content — exactly where our rows belong.
 */
void Sp1x2PauseDraw(const char *text, void *pos, int size, int colour)
{
    Sp1Vec p;
    Sp1Vec spacing;

    BuildTextSpriteChain(text, pos, size, colour);   /* the original "PAUSED" */

    if (g_nGamestate != 2) {
        return;
    }

    Sp1x2SetVec(&spacing, 15, 1, 0x1600);

    if (SP1X2_MENU_ACTIVE == 0) {
        /* A hint on the options screen, so the menu is discoverable. */
        if (g_nPauseMenuSubstate == 1) {
            Sp1x2SetVec(&p, 118, 206, 0x1100);
            /* Restored 2026-08-29. This was trimmed to "SQUARE" during a
               space crunch; with the .rodata move there is room again, and
               the hint reads as intended. */
            BuildTextSprites("SQUARE  MULTIPLAYER", &p, &spacing, 14, 11);
        }
        return;
    }

    /* Laid out for the BIG box (x 84..428, y 67..198) — the same frame the
       options list gets. Two instruction patches make the stock code draw it
       for our substate as well; see main.S hooks 13/14. Full-length values
       fit again at this width.

       The title is drawn with BuildTextSpriteChain — the same call the game
       uses for "PAUSED" — so it picks up the same shimmer. */
    /* BuildTextSpriteChain derives letter spacing from the size, and at 18 the
       letters ran into each other (it suits the big "PAUSED" at 28). Use the
       spaced routine at the menu's own size instead. */
    Sp1x2SetVec(&p, 174, 108, 0x1400);
    BuildTextSprites("MULTIPLAYER", &p, &spacing, 16, 11);

    sp1x2_sel_mobys = 0;
    sp1x2_sel_mobys2 = 0;

    /* Pitch 14, not the stock 16: the box is a fixed height sized for a title
       plus five rows, and we also draw a "MULTIPLAYER" subtitle. At 16 the
       fifth row falls below the bottom border. */
    /* SPLIT and VIEW only mean anything with a divided screen, so they go red
       when solo to show they are inert. WIDESCREEN stays live: it changes the
       projection, which is independent of how the screen is divided. */
    int off = SP1X2_SOLO ? SP1X2_SHADE_DISABLED : SP1X2_SHADE_NORMAL;

    /* "1" and "2" as two literals cost 8 bytes of .rodata (each padded to a
       4-byte boundary); one 4-byte literal holding both, indexed, costs 4 —
       and BIOS2 was exactly 1 byte over. */
    Sp1x2MenuRow(0, 120, "PLAYERS", SP1X2_SOLO ? sp1x2_digits : sp1x2_digits + 2,
                 SP1X2_SHADE_NORMAL);
    Sp1x2MenuRow(1, 134, "SPLIT",
                 (SP1X2_SPLIT_MODE == SP1X2_SPLIT_VERTICAL) ? "VERTICAL"
                                                            : "HORIZONTAL", off);
    Sp1x2MenuRow(2, 148, "WIDESCREEN", SP1X2_WIDESCREEN ? "ON" : "OFF",
                 SP1X2_SHADE_NORMAL);
    Sp1x2MenuRow(3, 162, "VIEW",
                 (SP1X2_VIEW_FIT == SP1X2_FIT_CROPPED)  ? "CROPPED" :
                 (SP1X2_VIEW_FIT == SP1X2_FIT_BALANCED) ? "BALANCED" : "FULL", off);
    Sp1x2MenuRow(4, 176, "DONE", 0, SP1X2_SHADE_NORMAL);

    Sp1x2WobbleSelected();
}
