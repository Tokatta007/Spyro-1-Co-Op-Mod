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

/* How far down to look. Spyro's OWN code probes the floor beneath him with
   exactly this reach (pete.c:1487), so it is the figure the game itself
   considers adequate for him. The first version of this fix used 4096, the
   value used for MOBYS, and it was far too short: a spawn point captured
   while he was still descending from a level's fly-in can sit thousands of
   units up, the probe found nothing, and the respawn was left exactly as
   broken as before. */
#define SP1X2_PROBE_REACH  0x10000

/* How far Spyro's origin sits ABOVE the floor when he is standing on it.
   Not a guess: checkpoint.c:21 adds precisely this to the checkpoint moby's
   position when saving a checkpoint, commented "move the starting position up
   a bit to accommodate for Spyro's hitsphere". His own tick agrees — pete.c
   treats him as grounded while m_Position.z - m_surfaceBelowSpyro is within
   512, and special_surfaces.c uses 400.

   Omitting this is what made the first attempt worse rather than better. It
   put him at floor level, which is 356 units INSIDE the ground, and the
   collision promptly ejected him — reported as being bounced up and landing
   off the pad. */
#define SP1X2_STAND_HEIGHT 356

void Sp1x2Ground(volatile int *spyro)
{
    volatile int *seen = (volatile int *)0x8000F1C0;
    int z = spyro[2];
    int floor;

    /* Ask from where he already stands, as pete.c:1487 does, rather than
       lifting him first. Lifting is retail's idiom for MOBYS, whose origins
       sit at floor level; Spyro's does not, so there is nothing to lift him
       out of and a lift only risks catching a surface above him. */
    floor = FindFloorBelow(spyro, SP1X2_PROBE_REACH);

    /* Accept the answer only if it lies inside the span actually searched.
       Every genuine result does by construction, so anything outside is a
       failed probe or a sentinel, and we keep the stored height — the
       behaviour we had before this function existed. A floor ABOVE him goes
       negative and, read as unsigned, fails the same test. A spawn a little
       too high is a blemish; one that drops a dragon into the void on a bad
       reading is a lost run. */
    if ((unsigned int)(z - floor) <= (unsigned int)SP1X2_PROBE_REACH) {
        z = floor + SP1X2_STAND_HEIGHT;
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
