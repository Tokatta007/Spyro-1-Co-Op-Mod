#include "Sp1.h"

/*
 * DRAGONFLY LIFECYCLE — the first resident of BIOS2B (0x8000E600, 512 bytes),
 * the second boot-copied code region. It exists because BIOS2 filled to the
 * byte and Sp1x2Spyro.c's -Os cross-jumping made every added branch there
 * cost ~200 bytes (see CLAUDE.md). Called from Sp1x2UpdateMobys.
 *
 * Measured across three builds: a death NULLS g_Sparx first (dead-owner
 * reaction), and the old moby then plays its exit and EXPIRES a few frames
 * later. Spawn-always duplicated (spawned while the old one still lived);
 * adopt-only stranded (the old one was already dead at heal time). So:
 * clear a pointer whose moby has died in our hands, adopt the old fly if it
 * still lives, else spawn fresh. State byte 0x48 is the verified offset.
 * SP1X2_SPARX1_SEEN is provably set before any heal can run (the P2 identity
 * block records it on the first two-pass frame, when g_Sparx is loader-set).
 *
 * Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
 */
void Sp1x2SparxHeal(void)
{
    volatile unsigned char *fly = (volatile unsigned char *)g_Sparx;

    if (fly != 0 && (signed char)fly[SP1_MOBY_STATE_OFF] < 0) {
        g_Sparx = 0;                   /* adopted earlier, finished dying */
    }
    if (g_Sparx == 0) {
        /* MEASURED 2026-08-23: SEEN can be NULL here — the game re-nulls
           g_Sparx during the moby passes, which run BETWEEN this heal and
           the P2 bookkeeping, so the bookkeeping copies the null into SEEN.
           Without this check the heal "adopted" address zero whenever a
           random kernel byte at 0x48 looked alive, and the spawn arm never
           ran again: permanent no-sparx. The check was originally deleted to
           save FOUR BYTES during the space crunch — load-bearing code lost
           to byte golf; never again. */
        fly = (volatile unsigned char *)SP1X2_SPARX1_SEEN;
        if (fly != 0 && (signed char)fly[SP1_MOBY_STATE_OFF] >= 0) {
            g_Sparx = (void *)fly;     /* mid-exit but alive: keep him */
        } else {
            void *fresh = g_SpawnMoby(120, 0);
            if (fresh != 0) {
                g_Sparx = fresh;
                SP1X2_SPARX1_SEEN = fresh;   /* ours, not a rebuild */
            }
        }
    }
}


/*
 * PLAYER 2 POINTER BOOKKEEPING — v4 (2026-08-23). History: v1 cleared on
 * every reseed (orphan per sequence); v2 content-validated with a guessed
 * offset (the fountain); v3 cleared whenever g_Sparx CHANGED — including the
 * transient NULL every death causes, so each P1 death orphaned P2's healthy
 * fly and spawned him another ("P2 collects a sparx per P1 death").
 *
 * v4: only a change to a DIFFERENT LIVING fly is a rebuild (that is what a
 * level load or shared-death reload produces — LoadLevelScene assigns
 * g_Sparx). A null is a death transient: keep both SEEN (so the heal can
 * adopt) and player 2's pointer (his fly is fine).
 */
void Sp1x2P2SparxKeep(void)
{
    volatile unsigned char *fly;

    if (g_Sparx != 0 && g_Sparx != SP1X2_SPARX1_SEEN) {
        SP1X2_SPARX1_SEEN = g_Sparx;   /* real rebuild: old world is gone */
        SP1X2_P2_SPARX = 0;
    }

    fly = (volatile unsigned char *)SP1X2_P2_SPARX;
    if (fly != 0 &&
        ((void *)fly == g_Sparx ||     /* rebuild landed P1's fly on our slot */
         (signed char)fly[SP1_MOBY_STATE_OFF] < 0)) {   /* his fly died */
        SP1X2_P2_SPARX = 0;
    }

    if (SP1X2_P2_SPARX == 0 && g_Sparx != 0) {
        SP1X2_P2_SPARX = g_SpawnMoby(120, 0);
    }
}


/* ---- moved from Sp1x2Menu.c 2026-08-23: BIOS2 rebalancing (the arrival
   capture + true-start cache needed the room). Behaviour identical. ---- */
#define SP1_SOUND_TABLE (*(volatile unsigned char **)0x800761d4)
#define SP1X2_MENU_ACTIVE  (*(volatile int *)0x8000F1A0)

/* The same call the stock menu makes: PlaySound(id, 0, 16, 0). */
void Sp1x2MenuChime(int which)
{
    volatile unsigned char *table = SP1_SOUND_TABLE;

    if (table != 0) {
        PlaySound((int)table[which], 0, 16, 0);
    }
}

/*
 * Hooked over the four `jal BuildTextSprites` calls that draw the main pause
 * menu's items — CONTINUE / OPTIONS / INVENTORY / QUIT GAME — at ram
 * 0x8001b3f0, 0x8001b440, 0x8001b490 and 0x8001b5c0.
 *
 * WHY: the stock pause draw renders the MAIN menu for any substate that is
 * not options (1) or quit (2), so parking on our own substate 3 did not give
 * us a blank canvas — the main menu drew straight through our rows. Rather
 * than fight that, we simply drop those four strings while our menu is open.
 * Everything else (the frame, the word PAUSED) still draws, so the screen
 * looks like a normal pause page with our content in it.
 */
void Sp1x2MainMenuItem(const char *text, void *pos, void *spacing, int size,
                       int colour)
{
    if (SP1X2_MENU_ACTIVE && g_nGamestate == 2) {
        return;                 /* our menu owns the page */
    }
    BuildTextSprites(text, pos, spacing, size, colour);
}
