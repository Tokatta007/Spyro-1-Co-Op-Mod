#include "Sp1.h"

/*
 * COLLISION ENTRY GATES — restored 2026-08-27, and they should never have
 * been retired.
 *
 * Both were built 2026-08-23 and MEASURED holding (50, then 168/2337
 * refusals with no fault at either crash site). They were then removed
 * during the BIOS2C retreat on the theory that repairing the camera's
 * m_Focus at the source made them redundant. That theory is DEAD: a
 * PCSX-Redux write watchpoint on m_Focus (condition Change) never fired, so
 * the focus is never corrupted and the focus repair was defending nothing.
 * With the guards gone the ram-hit freeze came straight back.
 *
 * WHAT THEY DEFEND. Two sibling collision entry points fault when handed a
 * garbage position (user-supplied CP0 dumps):
 *   func_8004BE4C  the sphere QUERY   — EPC 0x8004cb68, the bucket chain
 *                                       walk, BadVAddr garbage
 *   func_8004AE38  the segment PROBE  — EPC 0x8004b7c8, BadVAddr 0xb
 * A coordinate is turned into a bucket index by >>13, so a Y of ~13,000,000
 * indexes ~200 KB past the bucket table and level data gets walked as chain
 * heads: readable garbage spins forever (freeze), unaligned garbage faults
 * (AdEL, no handler, kernel spins — also a freeze, music still playing
 * because the CD streams in hardware).
 *
 * THE RULE: refuse any query whose position has a coordinate at or beyond
 * 0x400000, or negative — both are far outside every legitimate level
 * coordinate. Refusing returns "no collision", which the callers all handle
 * (they test the result). One unsigned compare per axis catches both cases
 * at once, since a negative reads as a huge unsigned value.
 *
 * Refusals are COUNTED so the underlying scramble stays measurable in the
 * wild: each tick is one averted freeze.
 *   0x8000ED60  query refusals   0x8000ED64  probe refusals
 * (The old counters lived at 0x8000E4C8/E4EC — that block is now CODE.)
 *
 * ENTRY PATCH SHAPE. Both functions begin identically:
 *     insn 0   lui   at, 0x8007          <- replaced by `j <gate>`
 *     insn 1   addiu at, at, 0x7DD8      <- runs in the delay slot
 *     insn 2   sw    s0, 0(at)           <- body resumes here
 * Insn 1 runs with $at NOT yet set by insn 0, so it computes garbage — which
 * is why each gate rebuilds $at fully (lui + addiu) before resuming. The
 * body needs at = 0x80077DD8, its static s-register save area.
 *
 * REGISTER SAFETY (the bug that cost two builds on 2026-08-26): a gate may
 * only clobber registers that are dead at a function entry. $t0-$t4 and $at
 * qualify; $a0/$a1 are the arguments and are only read. Nothing here touches
 * $v1 or the s-registers — the body saves those itself, at the resume point.
 *
 * Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
 */

__asm__(
    ".set noreorder\n"
    ".set noat\n"

    /* ---- func_8004BE4C: sphere query. a0 = centre vector ---- */
    ".global Sp1x2QueryGate\n"
    "Sp1x2QueryGate:\n"
    "lw    $t0, 0($a0)\n"
    "lw    $t1, 4($a0)\n"
    "lw    $t2, 8($a0)\n"
    "lui   $t3, 0x40\n"               /* 0x400000 — past any real level */
    "sltu  $t4, $t0, $t3\n"           /* unsigned: negatives read as huge */
    "beqz  $t4, 1f\n"
    "sltu  $t4, $t1, $t3\n"           /* delay slot: next axis */
    "beqz  $t4, 1f\n"
    "sltu  $t4, $t2, $t3\n"
    "beqz  $t4, 1f\n"
    "lui   $at, 0x8007\n"             /* delay slot: rebuild the save-area
                                         pointer insn 1 failed to build */
    "j     CollisionQueryBody\n"
    "addiu $at, $at, 0x7DD8\n"
    "1:\n"
    "lui   $t0, 0x8001\n"
    "lw    $t1, -0x12A0($t0)\n"       /* 0x8000ED60 */
    "move  $v0, $zero\n"              /* "no collision" */
    "addiu $t1, $t1, 1\n"
    "jr    $ra\n"
    "sw    $t1, -0x12A0($t0)\n"

    /* ---- func_8004AE38: segment probe. a0 = start, a1 = END ----
       Checking the END alone suffices: the caller builds it as start plus an
       offset, so a scrambled start shows up there too. */
    ".global Sp1x2ProbeGate\n"
    "Sp1x2ProbeGate:\n"
    "lw    $t0, 0($a1)\n"
    "lw    $t1, 4($a1)\n"
    "lw    $t2, 8($a1)\n"
    "lui   $t3, 0x40\n"
    "sltu  $t4, $t0, $t3\n"
    "beqz  $t4, 2f\n"
    "sltu  $t4, $t1, $t3\n"
    "beqz  $t4, 2f\n"
    "sltu  $t4, $t2, $t3\n"
    "beqz  $t4, 2f\n"
    "lui   $at, 0x8007\n"
    "j     CollisionProbeBody\n"
    "addiu $at, $at, 0x7DD8\n"
    "2:\n"
    "lui   $t0, 0x8001\n"
    "lw    $t1, -0x129C($t0)\n"       /* 0x8000ED64 */
    "move  $v0, $zero\n"
    "addiu $t1, $t1, 1\n"
    "jr    $ra\n"
    "sw    $t1, -0x129C($t0)\n"

    ".set at\n"
    ".set reorder\n"
);

/* The CAMERA ENTRY GATE that lived here is DELETED (2026-08-27) along with
   its hook. It carried, at various times, a focus-pointer repair (the
   watchpoint proved the focus is never corrupted), the shake-trio zeroing
   (those globals are never written non-zero anywhere in the game — dead
   code), and spherical clamps on radius and elevation. The elevation clamps
   were shown by the user's own probe reading to be GENERATING camera
   spasms, because elevation is a wrapping angle and they treated it as a
   magnitude. Nothing here ever fixed anything. Baseline restored. */

/* THE FOCUS PROBE LIVED HERE AND IS RETIRED — IT FOUND THE BUG (2026-08-27).
   Called right after player 2's camera update while his camera was live, it
   caught m_Focus == 0 with camera mode 0x8000000A, radius 16,975 and camera
   Z 54,150,062 against a live Spyro Z of 21,058. That is the whole answer:
   the camera was following a null pointer. See Sp1x2FixFocus in
   Sp1x2Spyro.c. */

/* THE STAGE PROBE LIVED HERE AND IS RETIRED — it found the bug (2026-08-27).
   Its readings, in order: span 4 (player 2's camera update) is where his
   camera position exploded to 54 million, AND where his follow radius first
   went bad at 16,975 — a plausible distance between the two dragons rather
   than a camera distance. That pointed straight at a camera measuring itself
   against the wrong Spyro, which the swap-guard asymmetry in Sp1x2Spyro.c
   explains exactly. Resurrect from git history if a camera fault returns:
   it sampled player 2's shadow camera at the entry of all four doubled
   subsystems and latched the first span to move the position or spoil the
   radius. THE ONE RULE IT TAUGHT: clear its block by hand before each test,
   or it latches the level entry (a legitimate 140,000-unit seed move) and
   tells you nothing. */

/* THE NULL-FOCUS REPAIR (Sp1x2FixFocus) LIVED HERE AND IS RETIRED 2026-08-30.
   It was real — a probe caught g_Camera.m_Focus holding NULL with the camera
   at Z 54 million — and its counters showed ~6 repairs in a session, so the
   nulls genuinely happen. But it stopped NEITHER symptom: the camera spasm
   continued unchanged, and the freeze that followed turned out to be an
   interrupt deadlock (Cause 8, syscall, music stopped) with nothing to do
   with the focus at all. Defending a fault that never manifests is not worth
   176 bytes when BIOS2 is full. The two collision entry gates above still
   cover the memory-fault class. Resurrect from git history if a camera fault
   is ever traced to the focus again; the counters were 0x8000ED74 (before
   player 2's update) and 0x8000ED78 (after). */

/* THE FOCUS PROBE LIVED HERE AND IS RETIRED — IT FOUND THE BUG (2026-08-27).
   Called right after player 2's camera update while his camera was live, it
   caught m_Focus == 0 with camera mode 0x8000000A, radius 16,975 and camera
   Z 54,150,062 against a live Spyro Z of 21,058. That is the whole answer:
   the camera was following a null pointer. See Sp1x2FixFocus in
   Sp1x2Spyro.c. */

/* THE STAGE PROBE LIVED HERE AND IS RETIRED — it found the bug (2026-08-27).
   Its readings, in order: span 4 (player 2's camera update) is where his
   camera position exploded to 54 million, AND where his follow radius first
   went bad at 16,975 — a plausible distance between the two dragons rather
   than a camera distance. That pointed straight at a camera measuring itself
   against the wrong Spyro, which the swap-guard asymmetry in Sp1x2Spyro.c
   explains exactly. Resurrect from git history if a camera fault returns:
   it sampled player 2's shadow camera at the entry of all four doubled
   subsystems and latched the first span to move the position or spoil the
   radius. THE ONE RULE IT TAUGHT: clear its block by hand before each test,
   or it latches the level entry (a legitimate 140,000-unit seed move) and
   tells you nothing. */

/* The moby tables this walker touches — same addresses as the BIOS3 map in
   Sp1x2Spyro.c, repeated here because the function moved regions. */
#define SP1X2_MOBY_STASH ((volatile unsigned char *)0x8000FA00)  /* 2B x 512 */
#define SP1X2_MOBY_OWNER ((volatile unsigned char *)0x8000FE00)  /* 1B x 512 */

/* Mask and unmask share one walker — the split version cost a duplicated
   loop skeleton BIOS2 could no longer afford. */
void Sp1x2MaskWalk(unsigned char *base, int n, int owner, int unmask)
{
    unsigned char *m = base;
    int i;

    for (i = 0; i < n; i++, m += SP1_MOBY_STRIDE) {
        if (SP1X2_MOBY_OWNER[i] == owner) {
            if (unmask) {
                m[SP1_MOBY_WASDRAWN] = SP1X2_MOBY_STASH[i*2];
                m[SP1_MOBY_UPDDIST]  = SP1X2_MOBY_STASH[i*2+1];
            } else {
                SP1X2_MOBY_STASH[i*2]   = m[SP1_MOBY_WASDRAWN];
                SP1X2_MOBY_STASH[i*2+1] = m[SP1_MOBY_UPDDIST];
                m[SP1_MOBY_WASDRAWN] = 0;
                m[SP1_MOBY_UPDDIST]  = 0;
            }
        }
    }
}
