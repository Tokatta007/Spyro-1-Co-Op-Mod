#include "Sp1.h"

/*
 * Spyro 1 co-op: player 2's Spyro.
 *
 * DRAWING (Spyromain's approach, from Sp2x2RenderEntities.c): he does not
 * spawn player 2 as a game object or teach the engine about two player
 * characters. He draws Spyro TWICE within one pass, swapping player 2's state
 * in around the second draw. Because that happens inside every viewport, each
 * player sees both dragons.
 *
 * MOVING: Spyro's per-frame update (TickSpyroGameplayFrame) is likewise run
 * once per player, with player 2's state AND input swapped in. The game's own
 * acceleration, collision, animation and ground probing do the work both
 * times — none of it is reimplemented here.
 *
 * WHY A TABLE. Spyro's state is NOT contiguous. Spyro 2 keeps one 708-byte
 * struct; Spyro 1 scatters the equivalent across 94 named globals in 15
 * separate ranges, interleaved with unrelated data. Two hand-picked blocks
 * were not enough, and each omission produced a distinct, instructive bug:
 *
 *   - missing the flame/horn-strike block -> player 1's flame drew a SECOND
 *     time during player 2's draw, at player 1's position but pointing in
 *     player 2's direction
 *   - missing g_nSpyroIdleAnimSeqCursor  -> player 2 "walked in place" while
 *     standing still
 *   - missing g_nSpyroChargeHoldFlag, g_nSpyroGlideBankingFlag and
 *     g_nSpyroFallTrackingFlag -> player 2 stuck in an endless
 *     jump/glide/flame loop
 *
 * The table below is GENERATED from open-spyro's symbol file by merging
 * adjacent Spyro globals, never merging across a foreign symbol, and was
 * verified to contain zero non-Spyro symbols. Regenerate it the same way if
 * more state turns up — do not hand-edit ranges.
 *
 * Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
 */

typedef struct {
    /* 16-bit OFFSET from 0x80075000 rather than a 32-bit address: every
       swapped global lives in 0x80075718..0x8007AA10, and halving the entry
       from 8 bytes to 4 bought back 64 bytes of BIOS2 when player 2's Sparx
       would not fit. Sizes are all <= 676. */
    unsigned short off;
    unsigned short size;
} Sp1x2Region;
#define SP1X2_REGION_BASE 0x80075000

/* 12 ranges, 1065 bytes.
 *
 * USE BOTH DECOMPS — they are complementary, and this is the lesson that cost
 * the most time here:
 *
 *   decomps/open-spyro          splat-style, one symbol per ADDRESS. Great for
 *                               "what lives at 0x...", useless for structure.
 *   decomps/spyro-1             TheMobyCollective. Actual STRUCT definitions
 *                               with documented fields, ~18k lines of C.
 *
 * Spyro's state is THREE STRUCTS, not 94 loose globals. Grouping open-spyro's
 * symbols by name produced ranges with 112 bytes of holes in the MIDDLE of
 * g_Spyro, because open-spyro names several of its fields as though they were
 * camera globals (g_nCameraScriptStickRequestY, g_pScriptedCameraActor) — they
 * are camera-RELATED fields inside Spyro, not camera state. Name-based
 * grouping excluded them; so did a static-analysis pass, because that filtered
 * on the same names.
 *
 * The holes contained m_ControlFlags, m_noGamepadUpdateFrames and
 * m_flameableFrames — shared between both dragons, which is why player 2 got
 * stuck in an endless jump/glide/flame loop the moment he jumped.
 *
 * spyro.h carries a static_assert on sizeof(Spyro) == 0x2a4, so the size is
 * authoritative rather than inferred. Prefer struct definitions over symbol
 * names whenever the two disagree. */
static const Sp1x2Region sp1x2_spyro_state[] = {
    /* ---- the three structs (decomps/spyro-1/include/spyro.h) ---- */
    { 0x3A58, 676 },   /* g_Spyro       — sizeof == 0x2a4, static_assert'd */
    { 0x36C8, 312 },   /* g_SpyroFlame  — sizeof == 0x138 */
    { 0x5A10, 40 },   /* SpyroShadow   — D_8007AA10 */

    /* ---- Spyro state that lives OUTSIDE those structs ---- */
    { 0x0788, 4 },   /* g_nSpyroIdleAnimTimeout */
    { 0x0804, 4 },   /* g_pSpyroContactActor */
    { 0x0814, 4 },   /* g_nSpyroDrawSuppressed */
    { 0x08A0, 8 },   /* g_nSpyroTurnRateAccum, FlameBreathTimerSave */
    { 0x08C0, 4 },   /* g_nSpyroFallReferenceZ */
    { 0x0960, 4 },   /* g_nSpyroPitchRateAccum */
    { 0x0970, 4 },   /* g_nSpyroIdleAnimSeqCursor */
    { 0x20BC, 4 },   /* g_pGemPickupSpyroMirrorActor */
    /* ---- COLLISION / PHYSICS SCRATCH: Spyro 1's `sp2_calculation_results`.
       Spyromain swaps a 56-byte "calculation results" buffer around each
       player's logic (Sp2x2LogicSpyro), and we never found the Spyro 1
       equivalent — it is not one buffer here but these four scalars, the
       outputs of the last collision query. Two of them sit in the exact
       blocks our leak sweep flagged and I wrongly dismissed as "clocks we
       already handle". The flight attitude controller works from ground-slope
       data, so sharing these between two ticks is felt hardest there. */
    { 0x0718, 4 },   /* g_SurfaceBelowFlags      */
    { 0x0808, 4 },   /* g_CollisionTriangleIndex */
    { 0x1B80, 4 },   /* g_CollisionPoint         */
    { 0x2368, 16 },   /* g_CollisionNormal — the surface normal the last
                               collision produced. Shared, and each player's
                               tick overwrites it, so player 1's pitch control
                               read player 2's normal on the following frame.
                               Harmless on foot; in FLIGHT levels the attitude
                               controller works from ground-slope data, and it
                               made vertical steering feel damped.
                               Found by leak sweep: block 0x80077300 differed
                               across player 2's tick, and this is the only
                               game state in it that we were not swapping. */
};

#define SP1X2_REGIONS (sizeof sp1x2_spyro_state / sizeof sp1x2_spyro_state[0])

/*
 * BIOS3 ALLOCATION MAP (0x8000F000 .. 0x80010000, 7168 bytes verified free).
 * Keep this current — everything here is a raw address, so a buffer that grows
 * silently overruns its neighbour.
 *
 *   0x8000F000  +0x100   markers / diagnostics
 *   0x8000F100  +0x100   (free — was the retired camera-offset placeholder)
 *   0x8000F200  +0x14D   player 2 pad state: g_Pad + g_PadBackup +
 *                        g_PadSwapFlag + g_ActivePad  (Sp1x2Pad.c)
 *   0x8000E800  +0x429   player 2 Spyro shadow (swap table, below)
 *   0x8000ED00  +4       player 2 "seeded" flag
 *   0x8000ED04  +4       level id we last seeded in
 *   0x8000ED08  +4       handover-pending flag (P2 owns a sequence)
 *   0x8000ED10  +4       player 2's Sparx (Moby*), 0 = not spawned
 *   0x8000ED14  +4       g_Sparx as last seen (rebuild detector)
 *   0x8000ED18  +4       last non-zero gamestate (teleport-detector gate)
 *   0x8000ED20  +0x20    rumble-guard save buffer (Sp1x2RumbleGuard)
 *   0x8000EE00  +0x110   player 2 camera shadow (g_Camera copy)
 *   0x8000EF10  +0x10    player 2's copy of the FOUR camera globals that
 *                        live outside g_Camera (Sp1x2SwapCameraState)
 *   0x8000F800  +0x400   moby m_WasDrawn sync flags (Sp1x2Graphics.c)
 *   0x8000FA00  +0x800   moby mask stash (2 bytes per moby, below)
 *   0x8000FE00  +0x400   moby owner table — ends EXACTLY at 0x80010000,
 *                        the end of BIOS3. ZERO headroom left in this region.
 *
 * Capacity is 0x400 = 1024 mobys. The old cap of 512 was exceeded after a
 * dragon rescue (cutscene-spawned dyn mobys pushed the count over), and
 * everything past the cap — the dyn tail, i.e. the SHEEP — was silently
 * never masked, hence double-updated. (A Marker[44] diagnostic reported the
 * live count while this was debugged; the write has since been removed.)
 *
 * THIS MAP ALREADY BIT ONCE: the Spyro shadow grew and swallowed the seeded
 * flag at its old address, re-seeding P2 every frame (unison movement, no
 * collision, burned lives). Update this map BEFORE moving any buffer, and
 * leave headroom.
 */
#define SP1X2_P2_SPYRO      ((volatile unsigned char *)0x8000E800)
#define SP1X2_P2_READY      ((volatile int *)0x8000ED00)
#define SP1X2_P2_LAST_LEVEL (*(volatile int *)0x8000ED04)   /* level we seeded in */
#define SP1X2_P2_HANDOVER   (*(volatile int *)0x8000ED08)
/* SP1X2_P2_SPARX now defined in Sp1.h (shared with Sp1x2Sparx.c) */
/* SP1X2_SPARX1_SEEN now defined in Sp1.h (shared with the heal) */
#define SP1X2_LAST_SEQ      (*(volatile int *)0x8000ED18)   /* last gamestate != 0 */
#define SP1X2_SUBSTEPS_OWED (*(volatile int *)0x8000ED0C)   /* physics substeps
    owed this frame, captured before player 1 consumes them */   /* P2 owns a running
    global sequence; swap back (not reseed) when gameplay resumes, so P1
    returns to his ORIGINAL position instead of teleporting to the sequence */
#define SP1X2_P2_CAMERA     ((volatile unsigned char *)0x8000EE00) /* 0x110 bytes */
/* The four camera-module globals that live OUTSIDE g_Camera — see
   Sp1x2SwapCameraState. Sits directly after the struct shadow
   (0x8000EE00 + 0x110), inside the same reserved 0x200. */
#define SP1X2_P2_CAMERA_EXTRA ((volatile int *)0x8000EF10)   /* 4 words */

/* FAIRY MUTE — frames left during which the save-fairy must not trigger.
 * See Sp1x2FairyMute below. */
/* FAIRY MUTE: [0] is 0 when off, otherwise the owning player plus one
 * (1 = player 1, 2 = player 2); [1]/[2] the x,y he respawned at. Packing the
 * owner into the flag saves a slot and a load. See Sp1x2FairyMute. */
#define SP1X2_FAIRY_MUTE ((volatile int *)0x8000ED40)
/* Which dragon's tick is running: 0 = player 1, 1 = player 2. Sp1x2Die is
   called from inside TickSpyroGameplayFrame, so this is how it knows whose
   death it is handling — the live state alone cannot say. */
#define SP1X2_TICKING (*(volatile int *)0x8000ED50)

/* (The reseed-reason probe was removed 2026-08-27: MEASURED as firing only
   on level entry, never on a hit, so reseeds are not the trigger.) */

/* Diagnostics at 0x8000F000 (earlier slots are used by Sp1x2Graphics.c):
 *   +0x40 port 2 status (0 = pad present)   +0x44 player 2 buttons held
 *   +0x48 player-2 updates actually run                                  */
#define SP1X2_MARKER   ((volatile unsigned int *)0x8000F000)
#define SP1X2_PAD2_RAW ((const volatile unsigned char *)0x80078E50)
#define SP1X2_P2_HELD  (*(volatile unsigned int *)(0x8000F200 + 8))

/* THE LEAK-SWEEP INSTRUMENT THAT LIVED HERE IS DELETED (2026-08-28).
   It was a diagnostic from the flight-steering hunt, already dead code (no
   call sites), but its scratch buffer SP1X2_SUMS was declared at
   0x8000EE00 — THE SAME ADDRESS AS PLAYER 2'S CAMERA SHADOW — and would
   have written 1,440 bytes straight through the camera shadow, the camera
   extras, the markers and the pad state had anyone ever called it. Exactly
   the failure this file's memory map warns about. Resurrect from git history
   if ever needed, with an address that is actually free. */



/* SP1X2_P2_START_OFFSET now lives in Sp1.h — the portal-sequence draw uses the
 * same constant, so the two dragons cannot be spaced one way during the
 * transition and another way when gameplay resumes.
 *
 * HISTORY, because the value moved twice. It was 0x300, then set to 0 during
 * the walk-in-place investigation: player 2's m_State was oscillating 0<->1
 * every frame, and spawning him exactly on player 1 removed the blind offset
 * as a variable. That bug turned out to be the buffered-input ring index, not
 * the spawn, and 0 was never put back — so both dragons spawned on the same
 * spot and were shoved apart by Sp1x2SeparatePlayers instead, landing at
 * SP1X2_BODY_RADIUS. Restored, and now shared. */


/* Exchange every region of Spyro state with player 2's shadow copy. */
/* One walker for both jobs — seeding is copy, per-frame is exchange — because
   duplicating the loop skeleton cost ~80 bytes BIOS2 does not have. */
static void Sp1x2SpyroTableWalk(int exchange)
{
    volatile unsigned char *shadow = SP1X2_P2_SPYRO;
    unsigned int r, i;

    for (r = 0; r < SP1X2_REGIONS; r++) {
        volatile unsigned char *live = (volatile unsigned char *)
            (SP1X2_REGION_BASE + sp1x2_spyro_state[r].off);
        for (i = 0; i < sp1x2_spyro_state[r].size; i++) {
            unsigned char t = live[i];
            if (exchange) {
                live[i] = *shadow;
            }
            *shadow = t;
            shadow++;
        }
    }
}

/* THE TWO SWAPS MUST AGREE — fixed 2026-08-27, and this was the camera bug.
 *
 * This function used to swap UNCONDITIONALLY while Sp1x2SwapCameraState()
 * no-ops whenever player 2 is not seeded. Every call site pairs them and
 * assumes both move together, so in any window where the ready flag is clear
 * the SPYRO swapped and the CAMERA did not — leaving the live camera and the
 * live Spyro belonging to DIFFERENT PLAYERS.
 *
 * The camera then measures itself against the wrong dragon. Its follow radius
 * is |m_Position - *m_Focus| (func_80033F08) and m_Focus points at the LIVE
 * Spyro, so the radius becomes the DISTANCE BETWEEN THE TWO PLAYERS, and
 * func_80034204 places the camera that far out. That is measured, not
 * inferred: a probe caught player 2's radius at 16,975 against a normal
 * follow distance of ~2,560, with the target radius holding the same value —
 * a plausible inter-player separation, at the exact moment his camera flew.
 * It also explains why the fault first appears inside the camera update
 * (the moby pass and the tick were cleared by the same probe) and why it
 * needs the dragons to be apart, which is what the user's cave test showed
 * back on 2026-08-26.
 *
 * The seed does not go through here (it calls Sp1x2SpyroTableWalk directly),
 * so guarding this costs nothing and restores the invariant: either both
 * players' state is exchanged, or neither is.
 */
static void Sp1x2SwapSpyroState(void)
{
    if (*SP1X2_P2_READY == 0) {
        return;
    }
    Sp1x2SpyroTableWalk(1);
}


/* Exchange the live Camera struct with player 2's copy — the missing half of
   Sp2x2LogicSpyro's swap list (he swaps sp2_camera around P2's tick), and the
   basis of per-player camera follow. No-ops until player 2 is seeded, so the
   render pass can call it unconditionally. */
/* Offsets from 0x80075000 of the four camera globals that live OUTSIDE
   g_Camera — shared by the swap and the seed below. */
static const unsigned short sp1x2_cam_extra[4] = { 0x06B8, 0x0894, 0x0924, 0x0938 };

/* THE CAMERA IS NOT JUST g_Camera (found 2026-08-27, by auditing every store
 * in the camera module's five assembly functions rather than reasoning about
 * it). The complete set of globals that camera code WRITES is:
 *
 *     g_Camera      139 stores   swapped (the struct below)
 *     g_Spyro         9 stores   swapped (Spyro table)
 *     D_8007AA10      1 store    swapped (SpyroShadow, in the Spyro table)
 *     D_80075938      4 stores   NOT SWAPPED until now
 *     D_80075894      4 stores   NOT SWAPPED until now
 *     D_80075924      2 stores   NOT SWAPPED until now  (L2/R2 rotate speed)
 *     D_800756B8      2 stores   NOT SWAPPED until now  ("camera was forced
 *                                to its destination" flag)
 *
 * Those four are file-scope statics in camera.c, reached through $gp — which
 * is why an earlier %hi-based footprint scan missed them entirely. They are
 * per-camera state shared between our two cameras, so player 1's camera wrote
 * them and player 2's update then read them as its own.
 *
 * That is precisely the reported asymmetry: player 1's camera sometimes does
 * the correct vanilla zoom-back on a hit, while PLAYER 2'S NEVER DOES.
 * D_800756B8 is the clearest offender — CameraForceToDestination sets it, and
 * that runs on the hit path (camera mode 6), so a hit on either player leaves
 * the other player's camera update believing it was just teleported.
 *
 * Same shape as every earlier bug of this family: the walking-in-place idle,
 * the endless jump/glide loop, and the flame pointing the wrong way were all
 * "a global we forgot to swap", and all were fixed by completing the list.
 */
void Sp1x2SwapCameraState(void)
{
    volatile unsigned char *live = (volatile unsigned char *)g_Camera;
    volatile unsigned char *shadow = SP1X2_P2_CAMERA;
    unsigned char t;
    int i;

    if (*SP1X2_P2_READY == 0) {
        return;
    }
    for (i = 0; i < SP1_CAMERA_SIZE; i++) {
        t = live[i]; live[i] = shadow[i]; shadow[i] = t;
    }
    for (i = 0; i < 4; i++) {
        volatile int *l = (volatile int *)(SP1X2_REGION_BASE + sp1x2_cam_extra[i]);
        volatile int *s = SP1X2_P2_CAMERA_EXTRA + i;
        int v = *l; *l = *s; *s = v;
    }
}


/* All three per-player swaps in one call. The four sites that exchange the
   full set wrote them out longhand in opposite orders; the ORDER IS PROVABLY
   IRRELEVANT — the camera, Spyro and pad swap sets were checked and touch no
   overlapping memory (the nearest approach is g_CollisionNormal ending at
   0x80077377 and g_Pad starting at 0x80077378) — so one helper serves them
   all and saves the repeated call setup. */
static void Sp1x2SwapAll(void)
{
    Sp1x2SwapCameraState();
    Sp1x2SwapSpyroState();
    Sp1x2SwapPadState();
}


/* The HANDOVER INSTRUMENT that lived here is RETIRED — it found its bug
   (2026-08-29). It recorded which of six sites cleared SP1X2_P2_READY and
   reported reason 1, the teleport detector, at gamestate 0 when player 2
   finished talking to the balloonist. That is what identified the false
   teleport during a handover. Resurrect from git history if a reseed ever
   fires unexpectedly again; the codes were 1 teleport detector, 2 handover
   level change, 3 tick death, 4 tick level change, 5 moby-pass death,
   6 moby-pass handover. */

/* Resume from a P2-owned sequence (dragon, balloonist, portal): he stayed
 * LIVE through it. Swap back so P1 returns to his ORIGINAL position and P2
 * keeps his post-sequence spot; if the sequence changed level, reseed instead.
 *
 * MUST run before ANY per-frame work — the moby hook fires BEFORE the tick
 * hook within GamestateUpdate, and running the moby passes with identities
 * still crossed made the balloonist's "has the player left yet" check see the
 * far player, clearing its latch and re-triggering the dialogue forever. */
static void Sp1x2HandoverResume(void)
{
    /* TELEPORT DETECTION — runs before anything else each frame, including
     * during a handover, which is the case that matters.
     *
     * We reseed player 2 on a level change and on death, but a level RESTART
     * is neither: same level id, no death. Retrying a failed FLIGHT level left
     * one dragon behind. Instead of hunting every restart path, watch for what
     * they all share — the live Spyro's position jumping further in one frame
     * than any movement can manage (0x4000 is far beyond supercharge).
     *
     * Checking it HERE rather than in the tick also fixes the mirror-image
     * bug: when player 2 died in a flight level, HIS state was the live one
     * (the death handover leaves it live), so the restart moved him correctly
     * while player 1 sat stale in the shadow, and the pending swap-back then
     * restored that stale position. On a detected teleport we drop the
     * handover and reseed from whichever state is live, so both dragons start
     * together wherever the game just put the live one. */
    {
        volatile int *last = (volatile int *)0x8000F1C0;
        int k, jumped = 0;

        if (g_nGamestate != 0) {
            SP1X2_LAST_SEQ = g_nGamestate;   /* remember what just ran */
        }

        /* WHILE A HANDOVER IS PENDING, THE LIVE DRAGON IS PLAYER 2 — so the
         * sample (which tracks player 1) cannot be compared against it, and
         * every such comparison is a false teleport. That is the balloonist
         * bug, measured twice: reason 1, gamestate 0, player 1 dragged to
         * player 2. The detector fires and `return`s BEFORE the swap-back
         * runs, so re-sampling after the swap (tried first) never executed.
         *
         * Skipping detection here is safe: a genuine level change during a
         * sequence is caught by the SP1X2_P2_LAST_LEVEL test below, and the
         * swap-back re-samples so the very next frame compares like with
         * like. This also retires the fragile sequence-number exemption for
         * this path — SP1X2_LAST_SEQ is a ONE-SHOT token and cannot guard an
         * event that fires more than once. */
        if (SP1X2_P2_HANDOVER) {
            jumped = 0;
        } else
        for (k = 0; k < 3; k++) {
            int d = g_anSpyroWorldPos[k] - last[k];
            if (d < 0) {
                d = -d;
            }
            if (d > 0x4000) {
                jumped = 1;
            }
            last[k] = g_anSpyroWorldPos[k];
        }
        if (jumped && *SP1X2_P2_READY != 0) {
            /* A jump right after a dragon rescue (8), fairy prompt (11) or
               balloonist (12) is the SEQUENCE repositioning the live dragon,
               not a level restart — those sequences do not rebuild anything,
               so the other player's state in the shadow is still valid, and
               reseeding here is what snapped the pair together at the
               pedestal ("P1 teleports next to P2"). Let the normal handover
               resume swap identities back instead: each keeps his own spot.
               Everything else (flight retry via the results screen (7),
               transitions, respawns) reseeds exactly as before. Consumed on
               use, so a later genuine restart is not masked by a stale 8. */
            int seq = SP1X2_LAST_SEQ;
            SP1X2_LAST_SEQ = 0;
            if (seq != 8 && seq != 11 && seq != 12) {
                SP1X2_P2_HANDOVER = 0;
                *SP1X2_P2_READY = 0;
                return;
            }
        }
    }

    if (!SP1X2_P2_HANDOVER || g_nGamestate != 0) {
        return;
    }
    if (SP1X2_P2_LAST_LEVEL != g_LevelId) {
        *SP1X2_P2_READY = 0;
    } else {
        volatile int *last = (volatile int *)0x8000F1C0;
        int k;

        EnterCriticalSection();
        Sp1x2SwapSpyroState();
        Sp1x2SwapCameraState();
        ExitCriticalSection();

        /* MOVE THE TELEPORT DETECTOR'S SAMPLE WITH THE SWAP (2026-08-29).
         * The swap-back IS a teleport as far as the detector is concerned:
         * its last sample is player 2's position, and the live dragon is
         * suddenly player 1, potentially a whole level away. On its next
         * call — the same frame, since Sp1x2HandoverResume runs from both the
         * moby hook and the tick hook — it saw that as a level restart and
         * reseeded, which SKIPS the swap-back's own return path and leaves
         * player 2's state live. To the player that reads as "player 1
         * warped to player 2".
         *
         * MEASURED, not deduced: the instrument recorded reason 1 (teleport
         * detector) with gamestate 0 at the moment player 2 finished talking
         * to the balloonist.
         *
         * The sequence-number exemption (8/11/12) was meant to cover this and
         * does not, because SP1X2_LAST_SEQ is CONSUMED by the first jump seen
         * during the sequence — any earlier reposition eats it, and the
         * swap-back's own jump then arrives with nothing to exempt it.
         * Re-sampling here removes the dependency entirely: there is no jump
         * to explain away. Same fix Sp1x2Die already uses for its respawn
         * teleport. */
        for (k = 0; k < 3; k++) {
            last[k] = g_anSpyroWorldPos[k];
        }
    }
    SP1X2_P2_HANDOVER = 0;
}


/* Seed player 2 from player 1 the first time we run in a level — the
   counterpart of Spyromain's Sp2x2InitSpyroTwin. */
static void Sp1x2InitPlayer2Spyro(void)
{
    Sp1x2SpyroTableWalk(0);            /* copy live -> shadow */

    /* Seed his camera from player 1's, and remember which level we seeded
       in: Spyromain re-seeds the twin at THREE level-entry hook sites; our
       init is lazy, so we detect level changes ourselves. */
    {
        volatile unsigned char *cl = (volatile unsigned char *)g_Camera;
        unsigned int i;
        for (i = 0; i < SP1_CAMERA_SIZE; i++) {
            SP1X2_P2_CAMERA[i] = cl[i];
        }
        /* ...and the four camera globals that live outside the struct, or
           player 2 starts life with uninitialised BIOS RAM in them. */
        for (i = 0; i < 4; i++) {
            SP1X2_P2_CAMERA_EXTRA[i] =
                *(volatile int *)(SP1X2_REGION_BASE + sp1x2_cam_extra[i]);
        }
    }
    if (SP1X2_P2_LAST_LEVEL != g_LevelId) {
        Sp1x2CaptureSpawn();           /* arrival spot = fallback respawn */
    }
    SP1X2_P2_LAST_LEVEL = g_LevelId;

    *SP1X2_P2_READY = 1;

    /* Offset him so the dragons do not start inside one another. Done by
       swapping his state in and editing the live position, rather than
       hunting for the right byte inside the shadow copy.

       Uses Sp1x2FormationOffset — THE SAME function the portal-sequence draw
       uses — so he is seeded exactly where he was being drawn a moment
       earlier. This was a flat `+= offset` on axis 0, which ignored which way
       Spyro faced, so the wingman jumped to his other side the instant the
       sequence ended and the split screen returned. */
    Sp1x2SwapSpyroState();
    {
        int off[3];
        Sp1x2FormationOffset(off);
        g_anSpyroWorldPos[0] += off[0];
        g_anSpyroWorldPos[1] += off[1];
    }
    Sp1x2SwapSpyroState();
}


/* Draw player 2's Spyro. Called once per viewport, after the game has already
   drawn player 1's. */
void Sp1x2DrawPlayer2Spyro(int pass)
{
    /* SOLO: the only place player 2 is ever seeded, so refusing here keeps
       SP1X2_P2_READY at 0 — and every other co-op path (the tick, the camera
       update, the moby partition, the portal-sequence draw) already returns
       early on that same flag. One guard therefore switches the whole mod off,
       and switching back to two players re-seeds him beside player 1 through
       the normal path. That is drop-in/drop-out for free. */
    if (SP1X2_SOLO) {
        return;
    }

    if (*SP1X2_P2_READY == 0) {
        Sp1x2InitPlayer2Spyro();
    }

    Sp1x2SwapSpyroState();
    if (g_nSpyroDrawSuppressed == 0) {   /* checked with P2's state swapped in */
        /* Player 2's dragon, same rule — his state is swapped in, so the
           matrix being nudged here is HIS. */
        Sp1x2FlameChainLoad(1, pass);
        RasterizePairedActor();
        Sp1x2FlameChainStore(1, pass);
        DrawSpyroDropShadow();
        /* The flame is a THIRD, separate renderer — Spyromain's per-Spyro
           draw is Sp2RenderSpyro + Sp2RenderSpyroShadow + a conditional
           Sp2RenderFlames, and we had only ported the first two. Without
           this call player 2's flame showed smoke (particles, spawned by his
           tick into the shared world system) but no fire jet. The guard byte
           is inside the flame block, so with P2 swapped in it reads HIS
           flame-active state. */
        if (*(volatile unsigned char *)0x80078760 != 0) {
            DrawSpyroFlames();
        }
    }
    Sp1x2SwapSpyroState();
}


/*
 * PLAYER-TO-PLAYER COLLISION (2026-08-19).
 *
 * Spyromain never implemented this — his dragons pass through each other — so
 * this is our own design rather than a port.
 *
 * Spyro is NOT a moby, so none of the game's actor-collision machinery applies
 * to him: there is nothing to register with and nothing to hook. What we do
 * instead is the simplest thing that behaves right — after BOTH players have
 * ticked, if the dragons are closer than SP1X2_BODY_RADIUS horizontally, push
 * each of them half of the overlap apart along the line between them.
 *
 * Deliberate choices:
 *   - HORIZONTAL ONLY (axes 0 and 1; axis 2 is up, proven by the early camera
 *     experiment). Pushing vertically would launch players or shove them into
 *     the floor, and it lets one dragon stand on the other harmlessly.
 *   - POSITION, not velocity. Velocity nudges feel mushy and fight the game's
 *     own physics; a direct half-overlap correction reads as a firm push and
 *     is self-limiting, since it stops the moment they separate.
 *   - Small per-frame corrections, so the game's own terrain collision can
 *     resolve anything we push them into on the following frame.
 *   - No damage, no PvP: purely positional, as asked.
 *
 * SP1X2_BODY_RADIUS is centre-to-centre separation in world units. The engine
 * treats 1024 as a natural step (ray marching divides by it) and 2048 is well
 * above Spyro's head, so a couple of hundred is dragon-sized. Tune this one
 * constant if they feel too sticky or too far apart.
 */
#define SP1X2_BODY_RADIUS  0x1A0    /* 416 world units, centre to centre */
#define SP1X2_BODY_HEIGHT  0x2A0    /* ignore each other beyond this height gap */

static void Sp1x2SeparatePlayers(void)
{
    volatile int *p1 = (volatile int *)0x80078A58;      /* live = player 1  */
    volatile int *p2 = (volatile int *)SP1X2_P2_SPYRO;  /* shadow = player 2 */
    int d[3];
    int dist, overlap, push, i;

    if (*SP1X2_P2_READY == 0 || SP1X2_PAD2_RAW[0] != 0 || g_nGamestate != 0) {
        return;
    }

    /* NOT IN FLIGHT LEVELS. On foot the dragons meet occasionally and a firm
     * push reads well. In a flight level they fly side by side constantly, so
     * the push fights the flight physics every frame — which is what made
     * vertical steering feel damped. The tell was in the report: holding a
     * direction long enough made the resistance VANISH (the dragons finally
     * separated beyond the radius) and respawning brought it back (they start
     * together again). Three leak sweeps found nothing wrong with player 2's
     * tick, which is what pointed here — the early-return "fix" that appeared
     * to cure steering had also been skipping this call. */
    if (g_nFlightLevelActive != 0) {
        return;
    }

    d[0] = p2[0] - p1[0];
    d[1] = p2[1] - p1[1];
    d[2] = p2[2] - p1[2];

    /* Cheap rejects first — they also keep the maths well inside 32 bits,
       since level coordinates are far larger than any radius we care about. */
    if (d[0] > SP1X2_BODY_RADIUS || d[0] < -SP1X2_BODY_RADIUS ||
        d[1] > SP1X2_BODY_RADIUS || d[1] < -SP1X2_BODY_RADIUS ||
        d[2] > SP1X2_BODY_HEIGHT || d[2] < -SP1X2_BODY_HEIGHT) {
        return;
    }

    d[2] = 0;                       /* horizontal separation only */
    dist = VecMagnitude(d, 1);

    if (dist >= SP1X2_BODY_RADIUS) {
        return;
    }
    if (dist <= 0) {
        /* Exactly co-incident: pick an axis so they cannot lock together. */
        p1[0] -= SP1X2_BODY_RADIUS / 2;
        p2[0] += SP1X2_BODY_RADIUS / 2;
        return;
    }

    overlap = SP1X2_BODY_RADIUS - dist;

    for (i = 0; i < 2; i++) {
        push = (d[i] * overlap) / (dist * 2);
        p1[i] -= push;
        p2[i] += push;
    }
}


/*
 * Hooked over `jal TickSpyroGameplayFrame` at ram 0x80033ad8, inside
 * GamestateUpdate. Runs Spyro's per-frame update ONCE PER PLAYER.
 *
 * Only Spyro's own update repeats — deliberately not the whole
 * GamestateUpdate, which would double-tick enemies, world animation and
 * timers. Spyromain draws the same line in Spyro 2.
 */
/* The HIT LATCH (moby-ownership freeze during a hit reaction) is REMOVED
   for the baseline, 2026-08-27. Its premise still stands as a READ FACT —
   Spyro 1's enemy AI writes camera state directly (func_level_20_8007E3A0
   calls func_800342F8 and func_80033F08; seven overlays do the same) — but
   freezing ownership did not change the symptom, and the baseline has to be
   free of our guesses. See CLAUDE.md before re-adding it. */

/* Release the pad-swap window and catch up any poll the callback had to
   skip. Lives here rather than beside the callback because LOADER is full and
   BIOS2 is where removing the old critical sections freed space. */
void Sp1x2PadRelease(void)
{
    volatile int *held = (volatile int *)0x8000ED54;
    int was = *held;

    *held = 0;                         /* release before polling, or the
                                          catch-up call would skip itself */
    if (was == 2) {                    /* the callback had to skip one */
        Sp1x2PadCallback();
    }
}


void Sp1x2TickPlayer2Spyro(void)
{
    unsigned int pad2_status;



    Sp1x2HandoverResume();

    /* SUBSTEP BUDGET. g_UnprocessedFrames (0x80075760) is how many physics
     * substeps are owed this frame, and Spyro's update CONSUMES it — so
     * player 1's tick empties it and player 2's tick then runs with nothing
     * left. Pitch is integrated per substep, which is why flight steering
     * suffered while yaw (applied per frame) felt fine.
     *
     * The earlier attempt saved this AFTER player 1's tick, which was already
     * too late — it restored an empty budget. This is the same shape as the
     * pad-ring fix in Sp1x2Pad.c: remember the value BEFORE the first
     * consumer, hand the same one to the second. */
    SP1X2_SUBSTEPS_OWED = *(volatile int *)0x80075760;

    TickSpyroGameplayFrame();          /* player 1, live state and input */

    /* DEATH = Spyromain's behaviour: EITHER player's death runs the game's
     * normal sequence untouched — a life is spent, play resumes at the
     * checkpoint. We only drop the seeded flag so player 2 re-seeds from
     * player 1 on the first gameplay frame back (his InitSpyroTwin hooks the
     * level-entry/respawn sites for exactly this; our lazy re-seed is the
     * same mechanism). Skipping P2's tick during gamestates 4/5 is what
     * prevents the old re-trigger loop.
     *
     * DO NOT try to cancel the sequence instead (first attempt):
     * TriggerRespawnOrGameOver initialises fade/camera/sequence state beyond
     * g_nGamestate, and force-reverting mid-setup CRASHES the game — user
     * verified. Let the game run its own machinery. */
    if (g_nGamestate == 4 || g_nGamestate == 5) {
        *SP1X2_P2_READY = 0;           /* death: both respawn at checkpoint */
        return;
    }
    if (g_nGamestate != 0) {
        return;                        /* P1-initiated cutscene/menu: P2
                                          simply holds where he is */
    }

    pad2_status = SP1X2_PAD2_RAW[0];

    if (*SP1X2_P2_READY == 0) {
        return;                        /* seeded on the first draw */
    }



    /* Level changed? Force a reseed, or player 2 keeps the OLD level's
       position and falls out of the new world. (Spyromain avoids this by
       hooking InitSpyroTwin at the level-entry sites.) */
    if (SP1X2_P2_LAST_LEVEL != g_LevelId) {
        *SP1X2_P2_READY = 0;
        return;
    }

    /* No controller on port 2 -> the derived input is meaningless. Buttons are
       ACTIVE LOW, so an untouched buffer reads as every button held, and
       player 2 jumps and flames forever. Leave him standing instead. */
    if (pad2_status != 0) {
        return;
    }


    /* m_State is at g_Spyro + 0x78 — confirmed from ChangeSpyroState
       (0x8003ea68), which loads 0x80078AD0 = g_Spyro + 0x78.
       g_Spyro is region 0 of the swap table, so shadow offset == struct
       offset. Compare the two players while BOTH stand still. */

    /* The Spyro update branches on these three (from its disassembly at
       0x8004A200). g_Spyro is region 0, so shadow offset == struct offset. */

    /* CRITICAL SECTION — not optional. Our pad callback runs in the VSync
       interrupt and touches the same derived pad block. If it fires mid-swap,
       it polls player 1 into player 2's slot and scrambles both. */
    /* Hold the pad callback off rather than disabling interrupts — running
       TickSpyroGameplayFrame with interrupts off deadlocked the console when
       something inside it waited on one. See Sp1x2PadCallback. */
    SP1X2_PAD_HOLD = 1;

    Sp1x2SwapAll();


    {
        /* PLAYER ANCHOR save/restore. g_anCameraLatchedAnchorPos
         * (0x80077798, XYZ) is written by the Spyro tick every frame —
         * open-spyro named it for the camera, but it behaves as the general
         * "where the player is" position that followers home on. With both
         * ticks writing it, it alternated P1/P2 per frame and SPARX settled
         * at the midpoint, twitching toward each player. Restore P1's value
         * after P2's tick so followers track player 1 (Sparx is his).
         * NOT restored on the handover path below — a sequence P2 triggers
         * may legitimately latch HIS anchor. */
        volatile int *anchor = (volatile int *)0x80077798;
        int ax = anchor[0], ay = anchor[1], az = anchor[2];

        /* SUBSTEP COUNTER. g_UnprocessedFrames (0x80075760) is how many
         * physics substeps are owed this frame, and Spyro's update CONSUMES
         * it. We restore it around player 2's pad POLL (Sp1x2Pad.c) but never
         * around his TICK, so the frame's substeps were consumed twice.
         * That fits the flight symptom precisely: pitch is integrated per
         * SUBSTEP, so halving them made vertical steering mushy, while yaw is
         * applied per frame and felt fine. */
        volatile int *substeps = (volatile int *)0x80075760;
        int after_p1 = *substeps;

        *substeps = SP1X2_SUBSTEPS_OWED;   /* same budget player 1 had */

        SP1X2_TICKING = 1;
        TickSpyroGameplayFrame();      /* player 2 */
        SP1X2_TICKING = 0;             /* back to player 1 for the rest of the
                                          frame — cheaper than setting it on
                                          player 1's path every frame */

        /* Leave the frame consumed exactly once, as the rest of the game
           expects. */
        *substeps = after_p1;
        if (g_nGamestate == 0) {
            anchor[0] = ax; anchor[1] = ay; anchor[2] = az;
        }
    }

    if (g_nGamestate != 0) {
        /* P2 triggered a GLOBAL sequence during HIS tick — death, a portal,
         * a dragon rescue, the balloonist. Generalised from the death-only
         * check after testing showed the same failure class everywhere:
         * dragon cutscenes playing with no Spyro in them, the balloonist
         * dialogue looping forever, portal entries floating. All were the
         * sequence running against the WRONG (swapped-out) player.
         *
         * Original death-crash note: TriggerRespawnOrGameOver wrote
         * its setup (dying state, death-cam, sequence data) into the LIVE
         * spyro + camera — currently P2's. Swapping them back out strands the
         * sequence with P1's un-dying state and no death-cam setup, which
         * CRASHES (user-verified; began exactly when the camera swap was
         * added — before that, the camera writes landed in the shared live
         * camera and the sequence survived).
         *
         * So: leave spyro + camera LIVE. The dying dragon plays its own
         * death — which is also visually correct — the respawn restores the
         * live set at the checkpoint, and both players re-seed from it.
         * Only the pad swaps back, preserving the VSync callback's
         * live-is-P1 discipline. The stale shadow (P1's old state) is
         * discarded by the re-seed. */
        Sp1x2SwapPadState();
        if (g_nGamestate == 4 || g_nGamestate == 5) {
            *SP1X2_P2_READY = 0;       /* death: both respawn at checkpoint */
        } else {
            SP1X2_P2_HANDOVER = 1;     /* cutscene/portal: P1 restored to his
                                          own spot when gameplay resumes */
        }
        Sp1x2PadRelease();
        return;
    }

    Sp1x2SwapAll();

    Sp1x2PadRelease();

    /* Both dragons have now moved this frame — resolve any overlap. */
    Sp1x2SeparatePlayers();
}


/*
 * Hooked over `jal UpdateCameraFrame` at ram 0x80033b4c, inside
 * GamestateUpdate — the game's once-per-frame camera update. Runs it ONCE PER
 * PLAYER: player 1 against live state, player 2 with his camera + Spyro + pad
 * swapped in, so the game's own follow/spring/collision logic drives his
 * camera. This is the port of Sp2x2LogicCamera (which swaps pad + spyro +
 * camera around Sp2LogicCamera in exactly this shape).
 */
/* Sp1x2FixFocus lives in Sp1x2Gates.c (BIOS2B) — see there for the
   measurement that found this. */

void Sp1x2UpdateCameras(void)
{
    UpdateCameraFrame();               /* player 1, live */

    if (*SP1X2_P2_READY == 0 || SP1X2_PAD2_RAW[0] != 0) {
        return;
    }

    SP1X2_PAD_HOLD = 1;            /* not a critical section — see the tick */

    Sp1x2SwapAll();

    UpdateCameraFrame();               /* player 2 */

    /* Consume player 2's edge latches HERE — after his LAST reader this frame
       (tick first, then camera). m_Down/m_Released accumulate until consumed
       (see the latch saga in Sp1x2Pad.c); without this they fill once and
       stay full forever. */
    *(volatile unsigned int *)0x80077378 = 0;   /* g_Pad.m_Down     */
    *(volatile unsigned int *)0x8007737c = 0;   /* g_Pad.m_Released */

    Sp1x2SwapAll();

    Sp1x2PadRelease();

}


/*
 * Batch 4 — the port of Sp2x2UpdateMobys' CORE mechanism: every moby
 * interacts with its NEAREST player.
 *
 * Spyromain rebuilds Spyro 2's active-moby list per player and runs the
 * game's update twice. Spyro 1 builds its active list INSIDE the level
 * megafunction (func_80051FEC, called first thing), so we cannot rebuild it —
 * instead we use the builder's own skip rule: a moby with m_WasDrawn == 0 and
 * m_UpdateDistance == 0 is left off the list. Assign each moby to its nearest
 * player, then run the megafunction twice, each pass masking the OTHER
 * player's mobys. Each moby updates exactly once per frame, seeing exactly
 * one Spyro — the nearest.
 *
 * Deliberate first-cut scope (Spyromain needed ~15 moby-id special cases,
 * accumulated by playtesting — expect to need our own):
 *   - Sparx is pinned to player 1 (his one special case we ported).
 *   - Dynamic mobys and any global work at the megafunction head run in BOTH
 *     passes; if double-ticking artefacts appear, that is where to look.
 *   - No critical section around the megafunction: it is far too long to run
 *     with interrupts masked, and the VSync pad callback touches only pad
 *     state, which this function does not swap while it runs.
 */
#define SP1X2_MOBY_STASH ((volatile unsigned char *)0x8000FA00)  /* 2B x 1024 */
#define SP1X2_MOBY_OWNER ((volatile unsigned char *)0x8000FE00)  /* 1B x 1024 */

static int Sp1x2AssignMobys(unsigned char *base)
{

    volatile int *p2pos = (volatile int *)SP1X2_P2_SPYRO;  /* world pos = +0 */
    unsigned char *m = base;
    int n;

    for (n = 0; n < 0x200; m += SP1_MOBY_STRIDE, n++) {
        signed char st = (signed char)m[SP1_MOBY_STATE_OFF];
        int *pos = (int *)(m + SP1_MOBY_POS_OFF);
        int d1 = 0, d2 = 0, k, d;

        /* TERMINATION — measured the hard way. Only state 0xFF (-1) ends the
         * array; any OTHER negative state is a dead moby still holding its
         * slot (collected gem, freed dragon's statue), and the game's own
         * walk SKIPS it and continues (builder asm at 0x8005212c:
         * `addi v1,-1; bne v1,v0 -> continue`). Treating every negative as
         * the sentinel made our walk stop at the first dead moby — the count
         * marker read ~175 at level start and DROPPED to ~47 after a dragon
         * rescue — leaving the whole tail (the dynamic sheep) unassigned,
         * unmasked, and double-updated.
         *
         * Dead slots keep an index (alignment with the array) but are marked
         * owner 2 = NEVER MASKED: a mid-pass spawn can reuse a dead slot, and
         * unmasking stale stashed bytes onto a freshly spawned moby would
         * corrupt it. */
        if (st == -1) {
            break;
        }
        /* (A chain-pointer sweep rode this walk 2026-08-23 and PROVED, over
           two sessions including a freeze, that moby m_CollisionChainNext is
           never corrupted — repair count at 0x8000E4A0 stayed 0. Removed to
           pay for the collision-query logger; the finding stands.) */


        if (st < 0) {
            SP1X2_MOBY_OWNER[n] = 2;
            continue;
        }

        for (k = 0; k < 3; k++) {
            d = g_anSpyroWorldPos[k] - pos[k]; if (d < 0) d = -d; d1 += d;
            d = p2pos[k]             - pos[k]; if (d < 0) d = -d; d2 += d;
        }
        /* Manhattan distance — cheap. HYSTERESIS (punch-list item 2): a moby
         * equidistant between the players used to flip owner every frame, so
         * its AI re-targeted constantly — the observed aimless wandering.
         * The owner table persists (in BIOS3, across frames AND across the
         * between-pass reassign), so: keep the current owner unless the other
         * player is at least 25% closer. A stale value on a reused dyn slot
         * just biases one frame, then washes out. */
        if (m == (unsigned char *)g_Sparx) {
            SP1X2_MOBY_OWNER[n] = 0;
        } else if (m == (unsigned char *)SP1X2_P2_SPARX) {
            SP1X2_MOBY_OWNER[n] = 1;
        } else {
            unsigned char prev = SP1X2_MOBY_OWNER[n];
            if (prev == 0) {
                SP1X2_MOBY_OWNER[n] = (d2 * 4 < d1 * 3) ? 1 : 0;
            } else if (prev == 1) {
                SP1X2_MOBY_OWNER[n] = (d1 * 4 < d2 * 3) ? 0 : 1;
            } else {
                SP1X2_MOBY_OWNER[n] = (d2 < d1) ? 1 : 0;
            }
        }
    }
    return n;
}

/* KEEP THE SAVE-FAIRY QUIET UNTIL THE PLAYER LEAVES HIS RESPAWN SPOT.
 *
 * Respawning on a rescued dragon platform opened the save prompt at once.
 * Retail hides this because a death RELOADS the level and drops you at a
 * checkpoint that is rarely the pedestal; our individual respawn skips the
 * reload and puts the dragon straight back on it.
 *
 * WHERE TO INTERVENE — the level overlay's own guard (level_10 at 0x800809D4)
 * fires only when the player is inside 0x400 of the pad AND
 *     lw   $v0, g_Spyro + 0x80      ; m_idleTimer
 *     blez $v0, skip                ; not idling -> no trigger
 * so she needs Spyro SETTLED. Zeroing that timer fails the guard at the
 * overlay's own condition, BEFORE any setup happens. A 2026-08-22 attempt
 * patched InitFairyCutscene's ENTRY instead and froze player 2 on the
 * pedestal, because the overlay writes moby state 6 and freezes the player
 * BEFORE calling it. **Suppress a sequence at its trigger condition, never at
 * the function it eventually calls.**
 *
 * DISTANCE, NOT A TIMER (revised on user feedback — the first version muted
 * for 120 frames and the fairy simply waited them out). Retail's own rule is
 * positional: the fairy moby carries a "recently talked" state (byte +0x48 ==
 * 2) that re-arms only once a counter at +0x49 reaches 0x10, which is what
 * walking away does. So: stay quiet while the player is near where he
 * respawned, re-arm when he leaves. Walk off and come back and she talks,
 * exactly as she does normally.
 *
 * Horizontal only — the pad is flat, and axis 2 is up. 0x800 is comfortably
 * outside her own 0x400 trigger radius, so re-arming cannot happen while she
 * could still fire. Called from BOTH moby passes, each with its own dragon
 * live; the first version covered only player 1, so player 2 still got the
 * instant prompt. The muted idle timer only delays the idle animation
 * (pete.c:1438 is its sole increment). */
static void Sp1x2FairyMute(int who)
{
    volatile int *mute = (volatile int *)0x8000ED40;
    int dx, dy;

    /* ONLY THE DRAGON THAT RESPAWNED. This runs once per moby pass, once per
       player — and the OTHER player is nowhere near the respawn point, so his
       pass tripped the distance test and switched the mute off before it
       could suppress anything. Measured: the block read active = 0 with a
       perfectly good position still stored beside it. That is also why
       sending the other dragon far away made the bug WORSE, not better. */
    if (mute[0] != who + 1) {
        return;
    }
    dx = g_anSpyroWorldPos[0] - mute[1];
    if (dx < 0) {
        dx = -dx;
    }
    dy = g_anSpyroWorldPos[1] - mute[2];
    if (dy < 0) {
        dy = -dy;
    }
    if (dx + dy > 0x800) {
        mute[0] = 0;                                /* left: re-arm */
    } else {
        *(volatile int *)(0x80078A58 + 0x80) = 0;   /* live m_idleTimer */
    }
}


/*
 * Batch 3: keep moby animation/behaviour flags consistent across the two
 * render passes — the port of the sync loop at the tail of
 * Sp2x2RenderEntities.
 *
 * Each pass recomputes every moby's m_WasDrawn for ITS camera, so pass 2
 * would overwrite pass 1's values: a moby visible only in player 1's viewport
 * would read as "off screen" — freezing its culling grace and, per moby.h,
 * its enemy behaviour ("enemies will not attack when they are off-screen").
 * Spyromain's fix, ported: record after pass 1, take the max after pass 2, so
 * a moby seen by EITHER player counts as seen.
 *
 * Spyro 2 also synced a global particle array; Spyro 1's particles are
 * level-overlay code behind g_UpdateParticle, a genuinely different design —
 * deliberately NOT ported until an artefact shows a need.
 *
 * Flag store: 0x8000F800 (+0x400), BIOS3 — see the allocation map in
 * Sp1x2Spyro.c. (A C `static` would land in a discarded .bss section.)
 */
#define SP1X2_MOBY_FLAGS ((volatile unsigned char *)0x8000F800)
#define SP1X2_MOBY_MAX   0x200

void Sp1x2SyncMobyFlags(int player)
{

    unsigned char *moby = *(unsigned char **)&g_LevelMobys;
    int i;

    if (moby == 0) {
        return;
    }
    for (i = 0; i < SP1X2_MOBY_MAX; moby += SP1_MOBY_STRIDE, i++) {
        signed char st = (signed char)moby[SP1_MOBY_STATE_OFF];
        if (st == -1) {
            break;             /* 0xFF alone is the sentinel...            */
        }
        if (st < 0) {
            continue;          /* ...other negatives are dead slots — skip */
        }
        if (player == 0) {
            SP1X2_MOBY_FLAGS[i] = moby[SP1_MOBY_WASDRAWN];
        } else if (SP1X2_MOBY_FLAGS[i] > moby[SP1_MOBY_WASDRAWN]) {
            moby[SP1_MOBY_WASDRAWN] = SP1X2_MOBY_FLAGS[i];
        }
    }
}


/* Sp1x2MaskWalk moved to Sp1x2Gates.c (BIOS2B) 2026-08-30 — BIOS2 was full,
   and this is a self-contained loop with two macro call sites. */


/* The segment-probe gate was RETIRED 2026-08-23 when the focus guard went
   in: its faults were downstream of the camera's garbage focus, which is
   now repaired at the source (Sp1x2CamGate). */



/* The collision-query gate was RETIRED 2026-08-23 with the probe gate:
   both defended against queries fed by the camera's garbage focus, now
   repaired at the source (Sp1x2CamGate). Its refusal counter averted 168+
   freezes while the root was hunted. If AdEL freezes at 8004cb68 ever
   return, a second scramble source exists — resurrect the gates from git
   history / this file's older revisions. */


#define Sp1x2MaskMobys(b, n, o)   Sp1x2MaskWalk((b), (n), (o), 0)
#define Sp1x2UnmaskMobys(b, n, o) Sp1x2MaskWalk((b), (n), (o), 1)

/* Hooked over the `jalr` through g_UpdateMoby at ram 0x80033aa4. */
void Sp1x2UpdateMobys(void)
{
    unsigned char *base = *(unsigned char **)&g_LevelMobys;
    int n;



    Sp1x2HandoverResume();             /* FIRST hook each frame — see helper */

    if (g_nGamestate != 0 || *SP1X2_P2_READY == 0
        || SP1X2_PAD2_RAW[0] != 0 || base == 0
        || g_nFlightLevelActive != 0
        || SP1X2_P2_LAST_LEVEL != g_LevelId) {  /* the tick, which clears
              P2_READY on a level change, runs AFTER this hook — without this
              the first frame of a new level swapped a dangling Sparx in */
        /* FLIGHT LEVELS RUN THE UPDATE ONCE (2026-08-19).
         *
         * g_UpdateMoby points at the LEVEL's own overlay code, and flight
         * levels are a different kind of level: their update drives the
         * timer, the destructible objects and the flight state, not just
         * per-moby AI. Our nearest-player partition masks per-moby work, but
         * anything the overlay does GLOBALLY still ran twice per frame.
         *
         * Symptoms that fits: the destructible chests appeared for a single
         * frame then stayed invisible while remaining flameable (their appear
         * state advanced twice), and vertical steering felt heavily damped
         * with the resistance vanishing after a long hold (an accumulator
         * being drained twice per frame, then saturating).
         *
         * Cost of running it once: in flight levels, moby logic sees only
         * player 1. That is a fair trade there — the levels are about
         * destroying objects, and either player can still destroy them. */
        g_UpdateMoby();                /* retail behaviour, single pass */
        return;
    }

    /* HEAL g_Sparx. MEASURED (PCSX-Redux, 2026-08-22): after a death it reads
       NULL — and the decomp shows why: EVERY level overlay contains a
       `sw $zero, %lo(g_Sparx)`, the game's own cleanup when the dragonfly
       moby dies. Fighting that (declare-dead + eager respawn) produced the
       stealable orphan: the overlay nulled the pointer to our replacement.
       So work WITH it: the overlay kills the moby and nulls the pointer; we
       respawn only on seeing the null. This path is non-flight gameplay, and
       retail never has a null g_Sparx here except after a sparx death — the
       exact case retail cures with the level reload we skip. The result is
       assigned only if the spawn SUCCEEDED: an unchecked assign is how one
       failed spawn nulled the pointer permanently. */
    Sp1x2SparxHeal();              /* dragonfly lifecycle — Sp1x2Sparx.c */


    n = Sp1x2AssignMobys(base);

    Sp1x2MaskMobys(base, n, 1);        /* hide P2's mobys...            */
    Sp1x2FairyMute(0);                 /* ...and hold off the save fairy */
    g_UpdateMoby();                    /* ...update P1's, seeing P1     */
    Sp1x2UnmaskMobys(base, n, 1);

    if (g_nGamestate != 0) {
        return;                        /* a P1-side moby (dragon, portal...)
                                          started a sequence — skip P2's pass */
    }

    /* REASSIGN before player 2's pass: player 1's pass may have SPAWNED
       dynamic mobys (fodder — the sheep), which extend the array past the
       count we recorded, leaving them unmasked in this pass and therefore
       updated twice per frame — the observed fast-forward sheep. A fresh walk
       assigns and masks the newcomers too. */
    n = Sp1x2AssignMobys(base);

    /* Camera swapped too (batch 5): sound attenuation is computed from the
       CAMERA position (spu.c, PlaySound), so any sound a P2-owned moby fires
       — an enemy dying next to him — must attenuate against HIS camera, not
       player 1's. Without this, P2's distant kills were silent.
       Known residual: SoundsUpdate re-attenuates LOOPING voices per frame
       against the live (P1) camera only; one-shots dominate in practice. */
    Sp1x2SwapCameraState();
    Sp1x2SwapSpyroState();

    /* PLAYER 2'S SPARX — Spyromain's sp2x2_rayz, ported. Spawned lazily with
       the game's own factory (class 120, exactly how retail spawns player
       1's in LoadLevelScene), HERE so player 2's Spyro is live if the spawn
       reads him, and so the overlay owning g_SpawnMoby is certainly loaded.
       g_Sparx == 0 means this level has no Sparx (retail skips the spawn);
       follow suit. */
    Sp1x2FairyMute(1);             /* player 2's turn */
    Sp1x2P2SparxKeep();            /* pointer bookkeeping — Sp1x2Sparx.c */

    /* For this pass, player 2's dragonfly IS "the" Sparx: the megafunction
       identifies the followed Sparx by the g_Sparx pointer, and homes it on
       the anchor at 0x80077798 — which holds player 1's latched position
       (the tick restores it), so point it at player 2 for the pass. Health
       colour needs nothing: his Spyro is live, so m_health reads his. */
    {
        void *sparx1 = g_Sparx;
        volatile int *anch = (volatile int *)0x80077798;
        int a0 = anch[0], a1 = anch[1], a2 = anch[2];
        int k;

        if (SP1X2_P2_SPARX != 0) {
            g_Sparx = SP1X2_P2_SPARX;
            for (k = 0; k < 3; k++) {
                anch[k] = g_anSpyroWorldPos[k];   /* live = player 2 */
            }
        }

        Sp1x2MaskMobys(base, n, 0);    /* hide P1's mobys...            */
        g_UpdateMoby();                /* ...update P2's, seeing P2     */
        Sp1x2UnmaskMobys(base, n, 0);

        /* g_Sparx is OUR bookkeeping and must never leak — restore it on
           every path, including the handover below. The anchor is left as
           player 2's only on the handover path, where his sequence owns it. */
        g_Sparx = sparx1;
        if (g_nGamestate == 0) {
            anch[0] = a0; anch[1] = a1; anch[2] = a2;
        }
    }

    if (g_nGamestate != 0) {
        /* A P2-side moby (dragon statue, balloonist, portal) started a
         * global sequence DURING HIS PASS, with his spyro+camera live. Same
         * handover as the tick: leave them live so the cutscene has its
         * player — HIS dragon stands in the rescue, HIS dialogue advances —
         * and re-seed the pair when gameplay resumes. */
        if (g_nGamestate == 4 || g_nGamestate == 5) {
            *SP1X2_P2_READY = 0;
        } else {
            SP1X2_P2_HANDOVER = 1;
        }
        return;
    }

    Sp1x2SwapSpyroState();
    Sp1x2SwapCameraState();

}


/*
 * Hooked over the `jal VecMagnitude` at ram 0x80056528 — the ONE call site
 * where TickActiveSoundVoices measures a 3D voice's distance for range-kill
 * and falloff. The caller has just computed diff = mobyPos - liveCamPos, so
 * the moby's position is recoverable and we can measure against the OTHER
 * camera too, returning the minimum: a sound near EITHER player is near.
 * This is Spyromain's Sp2x2GetSoundVolumeFromDistance semantic, per voice,
 * with the vanilla kill radius (the temporary 4x radius patch made ambient
 * loops audible across half the map — user-rejected, reverted).
 *
 * min() is symmetric, so this stays correct even on handover frames when the
 * live/shadow cameras are momentarily exchanged.
 */


/*
 * INDIVIDUAL DEATH AND RESPAWN (2026-08-22).
 *
 * Before this, either player dying ran the game's own sequence: one life gone
 * and BOTH players back at the checkpoint. That is what Spyromain does too —
 * he has no death handling at all — so this is our own design.
 *
 * WHY THE OLD ATTEMPTS CRASHED, and why this one is different. The notes
 * record two crashes from trying to CANCEL the sequence after it started: the
 * trigger initialises fade, camera and sequence state beyond the gamestate, so
 * reverting g_nGamestate leaves half-built state behind. Here the trigger is
 * never called at all on the individual path, so none of that is set up in the
 * first place. We replace the decision, we do not undo it.
 *
 * The pieces are all the game's own (spyro-1 loaders.c:428-458, which is the
 * respawn that happens as part of the level reload):
 *     position  <- g_Checkpoint.m_StartingPosition
 *     rotation  <- m_StartingRotation, >>4 if a checkpoint was stood on
 *     health    <- 3
 *     ResetSpyroState(1) — memsets the struct, keeps those three, state 0
 * so the respawned dragon is in exactly the state the game would have given
 * him, minus the level reload.
 *
 * THE LIVE DRAGON IS THE DYING ONE. Both call sites are inside Spyro's tick,
 * and our tick hook swaps player 2 in for his, so whoever is live here is the
 * one who died — no player index needed. By the same token the OTHER player is
 * always the shadow copy, which is where we read his health from.
 */
#define SP1_SPYRO_HEALTH_OFF  0x164
#define SP1_SPYRO_INVULN_OFF  0x160
#define SP1_SPYRO_BODYROT_Z   0x0E
#define SP1X2_RESPAWN_INVULN  90    /* the engine's own i-frame count */

void Sp1x2Die(void)
{
    volatile unsigned char *spyro = (volatile unsigned char *)0x80078A58;
    volatile int *checkpoint_pos  = (volatile int *)(0x80077888 + 0x50);
    volatile int  *checkpoint_rot = (volatile int *)(0x80077888 + 0x5C);
    volatile int  *stood          = (volatile int *)0x80077888;
    volatile int *other_health =
        (volatile int *)(SP1X2_P2_SPYRO + SP1_SPYRO_HEALTH_OFF);
    volatile int *health = (volatile int *)(spyro + SP1_SPYRO_HEALTH_OFF);
    int rot, i;


    /* Fall back to the stock sequence when there is no one to carry on:
       solo, player 2 not in play, the other player is dead too (the "both at
       once" case, which should behave as it always has), or the last life is
       gone and this is a game over. */
    /* Stock sequence only when there is nobody left to carry on: solo, player
       2 not in play, or the OTHER player is already down — which is both the
       "died at the same time" case and the real game over. */
    /* Stock sequence when there is nobody to carry on OR the lives are gone.
       A last-life "survivor plays on" mode was built 2026-08-22 and REMOVED
       the same day by user decision: it mis-attributed which player was down,
       dragged the survivor around, and the space to debug it did not exist.
       Any death at zero lives is simply the normal game over. */
    if (SP1X2_SOLO || *SP1X2_P2_READY == 0 || *other_health < 0 ||
        g_SpyroLifeCount == 0) {
        /* DOUBLE DEATH COSTS TWO LIVES (asked for twice). When both dragons
           go down together, the second one arrives here with the first
           already dead and the stock trigger charges a SINGLE life for the
           pair. Charge the partner's life here so the total is right; the
           stock call then charges the second and runs the shared respawn
           exactly as before. Guarded so it can never drive the count below
           zero — with one life left, a double death is the game over it
           already was. */
        if (*other_health < 0 && g_SpyroLifeCount > 0) {
            g_SpyroLifeCount--;
            *(volatile int *)(0x80077FA8 + 0x28) = g_SpyroLifeCount;
        }
        TriggerRespawnOrGameOver();
        return;
    }

    g_SpyroLifeCount--;             /* lives are shared, as asked */

    /* Tell the HUD directly, the way HudReset does (hud.c:131). Its rolling
       animation can ONLY COUNT UP (hud.c:284: `m_LifeCount += 1` and nothing
       decrements it) because in retail losing a life always reloads the level,
       and the reload assigns the count outright. We skip that reload, so left
       alone the display rolls up to 99 chasing a smaller number — while the
       real count keeps falling, which makes the life total unreadable exactly
       when it matters most. */
    *(volatile int *)(0x80077FA8 + 0x28) = g_SpyroLifeCount;   /* m_LifeCount */

    /* ...and ASK THE HUD TO SHOW ITSELF (2026-08-29, user request: a death
     * should visibly cost a life). The game already does this — hud.c:253
     * opens the life group whenever m_LifeCount differs from the real count,
     * then holds it and slides it shut. Assigning the count above, which we
     * must do or the display rolls up to 99, makes them equal and so the
     * group never opens: a death showed nothing at all.
     *
     * Nudging the state machine into HDS_Opening gets the retail animation
     * for free. With the counts already equal, hud.c:265 seeds
     * m_LifeSteadyTicks at -40 and hud.c:274 counts to 20, so it sits open
     * for 60 ticks and closes itself. The number will not roll DOWN — that
     * animation only increments (hud.c:284) — but the counter appears
     * showing the new total, which is what was asked for.
     *
     * Only from HDS_Hidden: interrupting an open or closing slide mid-way
     * would jump the icons. Offsets are from hud.h's struct — display state
     * +0x02, progress +0x07 — and m_LifeCount at +0x28 above independently
     * confirms the layout. */
    {
        volatile unsigned char *life_state =
            (volatile unsigned char *)(0x80077FA8 + 0x02);
        if (*life_state == 0) {                 /* HDS_Hidden */
            *(volatile unsigned char *)(0x80077FA8 + 0x07) = 0;  /* progress */
            *life_state = 1;                    /* HDS_Opening */
            /* REDRAW THE DIGITS. The counter showed the OLD number because
               the sprites are only re-printed by the ROLL animation
               (hud.c:286), which runs solely while the displayed and real
               counts differ — and we make them equal. So the digits kept
               whatever was last drawn. This is HudReset's own refresh call
               (hud.c:138): group 8, two digits, plain mode. */
            HudPrint(8, 2, g_SpyroLifeCount, 1);
        }
    }

    /* `last` is the TELEPORT DETECTOR's previous-position sample. It treats a
       live-Spyro jump over 0x4000 in one frame as a level restart and reseeds
       player 2 onto player 1 — which is why player 1 respawning also dragged
       player 2 to the checkpoint. Our respawn IS such a jump, so move the
       sample with him. It only matters for player 1, since the detector reads
       the live state; for player 2 this aliases his own position and the
       second write is harmless. */
    if (*stood == 0) {
        /* No checkpoint stood on: the game's slot may hold STALE LEVEL
           coordinates after an exit-to-homeworld (retail repairs this in its
           death-reload; we skip the reload). Prefer this world's TRUE START
           (cached on fresh loads — keeps respawns off the portal lip, where
           a player could stumble straight back in); else the captured
           arrival spot. All three sources carry rot at pos[3]. */
        volatile int *cache = (volatile int *)0x8000ED30;
        checkpoint_pos = (cache[0] == g_LevelId)
                       ? cache + 1
                       : (volatile int *)0x8000ED20;
    }
    for (i = 0; i < 3; i++) {
        ((volatile int *)spyro)[i] = checkpoint_pos[i];
    }

    /* Move the TELEPORT DETECTOR's previous-position sample with him. It reads
       any live-Spyro jump over 0x4000 in one frame as a level restart and
       reseeds player 2 onto player 1 — which is why player 1 respawning also
       dragged player 2 to the checkpoint. The detector samples the LIVE
       Spyro, and whoever is dying here is live, so update it either way. */
    for (i = 0; i < 3; i++) {
        ((volatile int *)0x8000F1C0)[i] = checkpoint_pos[i];
    }

    /* Arm the fairy mute AT THE RESPAWN POINT — see Sp1x2FairyMute. It must
       be armed HERE, not earlier: the first attempt stored g_anSpyroWorldPos
       before this function had moved him, so it recorded his DEATH position.
       He then respawned far from it, the distance test tripped immediately
       and the mute disarmed itself before it could suppress anything, which
       is why the prompt still fired. `checkpoint_pos` is whichever source
       won above — real checkpoint, cached true start, or captured arrival. */
    SP1X2_FAIRY_MUTE[0] = SP1X2_TICKING + 1;  /* armed, and for whom */
    SP1X2_FAIRY_MUTE[1] = checkpoint_pos[0];
    SP1X2_FAIRY_MUTE[2] = checkpoint_pos[1];
    rot = checkpoint_pos[3];        /* every source carries rot at [3] —
                                       the game's own slot included (+0x5C) */
    if (*stood != 0) {
        rot >>= 4;                  /* loaders.c shifts only for a real
                                       checkpoint, not a level start */
    }
    spyro[SP1_SPYRO_BODYROT_Z] = (unsigned char)rot;
    *health = 3;

    ResetSpyroState(1);             /* keeps the position/rotation/health we
                                       just wrote; clears the dying state */

    *(volatile int *)(spyro + SP1_SPYRO_INVULN_OFF) = SP1X2_RESPAWN_INVULN;
}
