/*
 * Sp1x2Ground.c - putting a respawned dragon on the ground.
 *
 * Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
 *
 * WHY THIS IS ITS OWN FILE: it belongs in Sp1x2Spyro.c beside Sp1x2Die, its
 * only caller, but BIOS2 is full to the byte. Code is assigned to a region
 * per OBJECT FILE (coop.ld), so a separate file is the only way to put this
 * body in LOADER while the call site stays in BIOS2. Taking the teleport
 * detector's three stores along with it pays for the call, so BIOS2 comes out
 * ahead — it ended up with 16 bytes free where it had none.
 *
 * Both regions are now within a handful of bytes of full, which is why this
 * file is written the way it is: two values carried across the call rather
 * than three, one unsigned window test rather than two signed comparisons.
 * Those choices are commented where they appear. Nothing here is a shortcut
 * around a safety check — the guard is exactly as strict as the pair of
 * comparisons it replaces.
 *
 *
 * THE BUG THIS FIXES (user, 2026-08-30): respawning in a level dropped the
 * dragon into the air and let him fall.
 *
 * Retail never shows this because retail never respawns anyone in place: a
 * death reloads the level and plays the entrance animation, where Spyro flies
 * in and lands under the sequence's control. Our individual respawn skips the
 * reload — that is the whole point of it, since the other player is still
 * playing — so whatever position we write is where he simply appears, with
 * nothing to hide a mismatch.
 *
 * And a mismatch is expected rather than exceptional. None of our three spawn
 * sources promises to sit exactly on the floor:
 *
 *   - g_Checkpoint.m_StartingPosition, which retail consumes during a reload
 *     that re-grounds him anyway;
 *   - the cached true start and the captured arrival, both sampled on the
 *     first gameplay frame of a level, when the entrance animation has only
 *     just handed control back and he may still be settling.
 *
 * So rather than trusting any of them vertically, ask the game where the
 * floor is. func_8004D5EC is retail's own answer to exactly this question,
 * used by loaders.c:709 to place every moby in the level.
 */

#include "Sp1.h"

/* Probe from slightly above the stored point, the way loaders.c:709 does. It
   costs nothing and covers the opposite error too: a spawn point a little
   BELOW the floor probes back up to it, instead of leaving him buried. */
#define SP1X2_PROBE_RISE   1024

/* How far down a floor is accepted. The game uses 4096 for mobys, and that is
   the right order here: this corrects a spawn point that is off by a body
   length or two, not one suspended over a canyon. If the stored point really
   is high above open ground, the drop is the honest behaviour and we leave it
   alone. Spyro's own body radius is 0x1A0 for scale. */
#define SP1X2_PROBE_REACH  4096

/* Settles a dragon already standing at his spawn point onto the floor.
   Takes only the position and reads the height out of it, so the call site in
   BIOS2 is a register move and a jump and nothing else — that region is full
   to the byte, and every argument would have to be paid for there.

   The dragon's OWN position is the probe: lifting it, asking, and putting it
   back is how retail grounds a moby (moby_helpers.c:184-187), and it avoids a
   stack copy. */
void Sp1x2Ground(volatile int *spyro)
{
    volatile int *seen = (volatile int *)0x8000F1C0;
    int z = spyro[2];
    int floor;

    /* Lift, ask, put back. */
    spyro[2] = z + SP1X2_PROBE_RISE;
    floor = FindFloorBelow(spyro, SP1X2_PROBE_REACH);

    /* Accept the answer only if it lies inside the span actually searched:
       from SP1X2_PROBE_RISE above the spawn height (a point buried just under
       the ground) down to SP1X2_PROBE_REACH below the probe. Every genuine
       result falls in that window by construction, so anything outside it is
       a failed probe or a sentinel, and we keep the stored height — the
       behaviour we had before this function existed. A spawn a little too
       high is a blemish; one that drops a dragon into the void on a bad
       reading is a lost run.

       The window is one UNSIGNED comparison rather than a pair of signed
       ones, and is measured from the spawn height rather than the lifted
       probe so the compiler need only carry two values across the call
       above. Both matter in a region with no free bytes at all. */
    if ((unsigned int)(z - floor + SP1X2_PROBE_RISE)
            <= (unsigned int)SP1X2_PROBE_REACH) {
        z = floor;
    }

    spyro[2] = z;

    /* Move the TELEPORT DETECTOR's previous-position sample with him, from
       the FINAL height rather than the one the caller asked for. It reads any
       live-Spyro jump over 0x4000 in a frame as a level restart and reseeds
       player 2 onto player 1 — which is why player 1 respawning used to drag
       player 2 to the checkpoint. It samples the LIVE Spyro, and whoever is
       dying is live, so this matters for either player.

       These three stores belong to the respawn and would read more naturally
       beside it in Sp1x2Die. They are here because BIOS2 has no room and
       LOADER does. */
    seen[0] = spyro[0];
    seen[1] = spyro[1];
    seen[2] = z;
}
