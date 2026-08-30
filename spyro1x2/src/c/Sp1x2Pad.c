#include "Sp1.h"

/*
 * Spyro 1 co-op: player-2 input.
 *
 * Spyromain's trick, ported. He never wrote controller-reading code for
 * Spyro 2 — he swapped the two pad port buffers and called the game's OWN
 * handler a second time, so the game processes player 2's input believing it
 * is player 1's. Everything downstream (calibration, sticks, button history,
 * movement) comes along for free.
 *
 * Why it works here, verified rather than assumed. Scanning every instruction
 * that references either buffer:
 *   port 1 (0x800786A0): 3 refs — init, 0x8003354c, and 0x80053f00 which
 *                        falls inside PollPadAndDistributeInput
 *   port 2 (0x80078E50): 1 ref  — the init, and nothing else in the game
 * So the pad handler reads ONLY port 1. Swap the buffers, call it again, and
 * it consumes player 2's data through the full pipeline.
 *
 * THIS BUILD IS A DELIBERATE HALF-STEP. Both polls write the same derived
 * input state, so the second overwrites the first: expect SPYRO TO BE DRIVEN
 * BY CONTROLLER 2. That is the visible proof the swap works. Keeping both
 * players' input means also swapping the derived state out to shadow storage
 * between the polls — the next step, and the same borrow-and-restore pattern
 * used everywhere else in this mod.
 *
 * Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
 */



/* Exchange the two raw BIOS pad buffers.
   32 bytes covers status, type, buttons and both analog sticks, and sits
   inside the 34 the BIOS uses. The buffers are 0x7B0 apart, so no overlap. */
static void Sp1x2SwapPadPorts(void)
{
    volatile unsigned int *a = (volatile unsigned int *)g_abPadPort1Buffer;
    volatile unsigned int *b = (volatile unsigned int *)g_abPadPort2Buffer;
    unsigned int t;
    int i;

    for (i = 0; i < SP1_PAD_BUF_WORDS; i++) {
        t = a[i];
        a[i] = b[i];
        b[i] = t;
    }
}


/* Player 2's derived input state, parked in verified-free BIOS RAM.
   Zero at boot, which is a fine starting state. */
#define SP1X2_P2_PAD_STATE ((volatile unsigned int *)0x8000F200)


/* Exchange the live derived input block with player 2's shadow copy.
   Non-static: Sp1x2Spyro.c calls this to run Spyro's update against
   player 2's input. */
void Sp1x2SwapPadState(void)
{
    /* The game has TWO Gamepad structs, both 0xa4 bytes (static_assert'd in
     * decomps/spyro-1/include/gamepad.h):
     *
     *   g_Pad        0x80077378  live input
     *   g_PadBackup  0x800776D8  stashed during input lockout
     *
     * func_8004A200 (Spyro's update) copies g_Pad into g_PadBackup and RESETS
     * g_Pad whenever m_noGamepadUpdateFrames != 0. With one shared backup
     * buffer, the two players clobber each other's stashed input.
     *
     * g_PadSwapFlag (0x80075944) tracks whether that stash happened, and the
     * update clears it unconditionally. open-spyro misnames it
     * g_bSpyroPadSnapshotReverseFlag, which is why it was previously swapped
     * as SPYRO state — wrong, because the Spyro swap also runs during drawing
     * while the pad swap does not. It belongs here. */
    /* 16-bit offsets from 0x80075000 (same trick as the Spyro swap table) —
       halving this table paid for the BIOS2B boot copy. */
    static const struct { unsigned short off; unsigned short size; } blocks[] = {
        { 0x2378, 0xa4 },   /* g_Pad */
        { 0x26D8, 0xa4 },   /* g_PadBackup — 0x800776D8. WAS 0x16D8, a typo
             that resolved to 0x800766D8 and swapped 164 bytes of the actor
             mesh table instead. Found 2026-08-28 by the CHANGES.md audit,
             confirmed against spyro-1's game.bss.s AND the instruction pair
             at 0x8004A278 (lui 0x8007 / addiu 0x76D8). The swap was
             symmetric so nothing crashed, but player 2's pad BACKUP was
             never swapped and mesh pointers carried shadow bytes through his
             tick. */
        { 0x0944,    1 },   /* g_PadSwapFlag */
        { 0x07e0,    4 },   /* g_ActivePad — THE input pointer.
             spyro-1 calls it "Active input pointer, selects a frame of
             buffered inputs"; open-spyro calls it g_pPadSubstepState.
             Spyro's own code dereferences it 36 times — it is how the update
             reads input, not g_Pad directly. Left unswapped, player 2's
             substeps read whichever frame player 1's tick left it pointing
             at. A frame with jump held then repeats forever. */
    };
    volatile unsigned char *shadow = (volatile unsigned char *)SP1X2_P2_PAD_STATE;
    unsigned int b, i;

    for (b = 0; b < sizeof blocks / sizeof blocks[0]; b++) {
        volatile unsigned char *live =
            (volatile unsigned char *)(0x80075000 + blocks[b].off);
        for (i = 0; i < blocks[b].size; i++) {
            unsigned char t = live[i];
            live[i] = *shadow;
            *shadow = t;
            shadow++;
        }
    }
}


/*
 * Our replacement VSync callback: runs the game's pad handler once per player,
 * with each player's derived state swapped in around its own poll.
 *
 * The ordering matters, and not only to keep the players apart. The handler
 * computes edge events ("pressed", "released") by comparing the new reading
 * against whatever is currently in the derived block. Each poll must therefore
 * see ITS OWN player's previous state, or the edges come out as the difference
 * between player 1 and player 2 — which is what the previous build did, and
 * why controller 1's jump and flame still fired while charge and look-around
 * did not.
 *
 *   start:  live = P1 previous, shadow = P2 previous
 *   1 poll        -> live = P1 new          (edges vs P1 previous, correct)
 *   2 swap state  -> live = P2 previous, shadow = P1 new
 *   3 swap ports  -> port 1 buffer now holds P2's raw data
 *   4 poll        -> live = P2 new          (edges vs P2 previous, correct)
 *   5 swap ports  -> raw buffers restored; the game reads these
 *   6 swap state  -> live = P1 new, shadow = P2 new
 *
 * The game then runs its frame against player 1's input exactly as retail,
 * and player 2's fully-processed input waits in the shadow copy.
 */
/* Main loop holds swapped state / the callback wanted to run and could not.
   Sp1x2PadRelease lives in Sp1x2Spyro.c (BIOS2) — LOADER has no room, and
   BIOS2 is exactly where removing the old critical-section calls freed some. */
#define SP1X2_PAD_HELD   (*(volatile int *)0x8000ED54)
/* 0 = free, 1 = main loop holds the swap, 2 = holds it AND we had to skip a
   poll. One word rather than two: LOADER is down to its last bytes and this
   check sits in the interrupt path. */

void Sp1x2PadCallback(void)
{
    /* DEFERRED POLL — the fix for the 2026-08-30 interrupt deadlock.
     *
     * This runs in the VSync interrupt and swaps pad state, so it must not
     * fire while the main loop holds player 2's state swapped in — it would
     * poll player 1's input into player 2's slot. That used to be prevented
     * by wrapping the main loop's whole tick and camera update in
     * EnterCriticalSection, i.e. RUNNING ENTIRE GAME FUNCTIONS WITH
     * INTERRUPTS DISABLED. Spyro's tick can play sounds, kill him and load
     * levels; the camera update raycasts. Anything in there that waits on an
     * interrupt then waits forever — a hard freeze with the music stopped,
     * confirmed by a CP0 dump landing on EnterCriticalSection's syscall.
     *
     * Now the main loop just raises a flag. If we fire inside that window we
     * record the miss and return immediately; the main loop performs the poll
     * itself the moment it releases (Sp1x2PadRelease). Interrupts stay
     * ENABLED throughout, so the deadlock is structurally impossible.
     *
     * Cost: on a frame where this lands mid-tick, the poll happens just after
     * the tick instead of just before — one frame of latency on that frame
     * only, against a freeze that kills the console. */
    if (SP1X2_PAD_HELD != 0) {
        SP1X2_PAD_HELD = 2;            /* held, and a poll was skipped */
        return;
    }

    /* GLOBAL clocks that PollPadAndDistributeInput advances. These are NOT
     * per-player — they measure real elapsed time — so the second poll must
     * not tick them again. Left unprotected they run at double rate, the game
     * believes twice as much time has passed as really has, and movement
     * integration goes wrong: Spyro walks normally, then slides onward as
     * though decelerating without ever stopping. (Charging is unaffected
     * because it is a held state rather than something integrated over time.)
     *
     * Save before the second poll, restore after, so poll 2 contributes only
     * to the derived input block that we swap. */
    volatile int *frame_ticks  = (volatile int *)0x80075760;  /* ACTUALLY
        g_UnprocessedFrames (spyro-1 name) — the buffered-input ring index.
        open-spyro's name g_nFrameTicks misled us twice. Verified: it feeds
        the `slti < 4` ring-bound check at 0x80054224 inside the pad poll. */
    volatile int *vblank_ticks = (volatile int *)0x800758c8;  /* g_nVblankTickCount */
    volatile int *cd_watchdog  = (volatile int *)0x8007588c;  /* g_nCdStallWatchdogTicks */
    int saved_frame, saved_vblank, saved_cd, pre_ring;

    pre_ring = *frame_ticks;         /* ring index BEFORE player 1's poll */

    PollPadAndDistributeInput();     /* 1: player 1, from port 1 */

    Sp1x2SwapPadState();             /* 2 */
    Sp1x2SwapPadPorts();             /* 3 */

    saved_frame  = *frame_ticks;
    saved_vblank = *vblank_ticks;
    saved_cd     = *cd_watchdog;

    /* Poll player 2 at the SAME ring index player 1 just used. Without this,
       P1's poll has already advanced the index, so P2's fresh data lands one
       slot later than P1's — and his substep loop reads ring[0..dt-1], making
       his FIRST substep every frame read stale input. Measured consequence:
       held jumps cut to minimum height (first substep sees "not held"), and
       the repeating one-step idle walk. */
    *frame_ticks = pre_ring;

    PollPadAndDistributeInput();     /* 4: player 2, port 2 masquerading as 1 */

    *frame_ticks  = saved_frame;
    *vblank_ticks = saved_vblank;
    *cd_watchdog  = saved_cd;

    Sp1x2SwapPadPorts();             /* 5: always restore */
    Sp1x2SwapPadState();             /* 6 */

    /* EITHER PLAYER ADVANCES DIALOGUE — the port of Sp2x2LogicController's
     * button merge:
     *
     *     if (game_state != 0)  sp2_pad[52] |= sp2x2_pad[52];
     *
     * Outside gameplay the game reads only the LIVE pad, which is always
     * player 1's, so player 2 could trigger a dragon rescue or talk to the
     * balloonist and then be unable to advance his own conversation. Merging
     * player 2's buttons in while a sequence is running lets either player
     * press on — which is how it should feel in co-op, and what Spyromain
     * does.
     *
     * Only the button words are merged: m_Down (edges) and m_Held. Sticks and
     * calibration stay player 1's, so nothing about movement changes, and
     * gameplay (state 0) is untouched. */
    if (g_nGamestate != 0) {
        volatile unsigned int *live = (volatile unsigned int *)0x80077378;
        volatile unsigned int *p2   = (volatile unsigned int *)0x8000F200;

        live[0] |= p2[0];        /* m_Down */
        live[2] |= p2[2];        /* m_Held */
    }
}


/*
 * Hooked over `jal InstallVSyncCallback` at ram 0x80012444, inside
 * InitPadSystem. The original argument (PollPadAndDistributeInput) is still
 * computed into a0 by the two instructions before the call; we ignore it and
 * register our own callback instead, which calls it twice.
 */
void Sp1x2InstallVSyncCallback(void (*original)(void))
{
    (void)original;
    InstallVSyncCallback(Sp1x2PadCallback);
}


/*
 * (Lives in this file only because it is linked into LOADER, which has room
 *  to spare, while BIOS2 is full.)
 *
 * Hooked over the `jal TickSparkles` at ram 0x80058bc0, inside
 * func_80058BA8 ("render glows and sparkles") — itself the final call of
 * ComposeFrameScene.
 *
 * That function does two things: it DRAWS the glows, then it ADVANCES every
 * particle by g_DeltaTime. Drawing belongs to each viewport; ageing belongs to
 * the frame. Calling ComposeFrameScene once per player therefore aged every
 * particle in the game at double rate — the dragon-head nostrils strobed
 * instead of trailing smoke, and it is a candidate for the flight chests
 * vanishing too, if their appear animation runs on the same clock.
 *
 * Exactly the flame-matrix lesson in a different renderer: per-frame state
 * being advanced once per draw.
 */
void Sp1x2TickSparkles(int delta)
{
    if (SP1X2_RENDER_PASS != 0) {
        return;                 /* pass 2 draws them; it does not age them */
    }
    TickSparkles(delta);
}


/*
 * PER-VIEWPORT REGION VISIBILITY (2026-08-19).
 *
 * D_800771C8 is a 256-entry table of "is this world region visible", written
 * by the WORLD renderer (SetupFrameOT / r_environment) for whichever camera
 * just drew. "Queue render mobys" then consults it per moby:
 *
 *     lbu  t1, 0(D_800771C8 + region)
 *     bgtz t1, keep            ; region visible -> stay queued
 *     sb   zero, 0x51(moby)    ; else DROP it, and clear its drawn flag
 *
 * With one camera that is fine — the table always describes the view about to
 * be drawn. With two passes it is not: each pass queues against the table the
 * OTHER pass left behind, so objects in regions only one player can see get
 * dropped, and objects that flip between the two tables STROBE. The dragon
 * head's nostril smoke strobed; the flight-level chests vanished outright,
 * because player 2 is frozen there and his region set never includes them.
 *
 * So each viewport gets its own copy: restore this pass's table before the
 * queue reads it, save the freshly computed one after the world renderer
 * writes it. Same shape as the flame chains — per-frame state that must not
 * be shared between passes.
 */
#define SP1X2_REGIONS      ((volatile unsigned char *)0x800771c8)
#define SP1X2_REGION_BYTES 0x100
#define SP1X2_REGION_VALID (*(volatile int *)0x8000F3F0)

#define Sp1x2RegionSave(pass) \
    ((volatile unsigned char *)(0x8000F400 + (pass) * SP1X2_REGION_BYTES))

void Sp1x2RegionsLoad(int pass)
{
    volatile unsigned char *save = Sp1x2RegionSave(pass);
    int i;

    if ((SP1X2_REGION_VALID & (1 << pass)) == 0) {
        return;             /* nothing saved yet — use whatever is live */
    }
    for (i = 0; i < SP1X2_REGION_BYTES; i++) {
        SP1X2_REGIONS[i] = save[i];
    }
}


void Sp1x2RegionsStore(int pass)
{
    volatile unsigned char *save = Sp1x2RegionSave(pass);
    int i;

    for (i = 0; i < SP1X2_REGION_BYTES; i++) {
        save[i] = SP1X2_REGIONS[i];
    }
    SP1X2_REGION_VALID |= (1 << pass);
}


#define SP1X2_MOBY_FLAGS ((volatile unsigned char *)0x8000F800)
#define SP1X2_MOBY_MAX   0x200

/* Sp1x2SyncMobyFlags moved to Sp1x2Gates.c (BIOS2B) 2026-08-30 — LOADER ran
   out when the deferred-poll check landed in the interrupt path. */



/*
 * PARTICLE SYNC BETWEEN PASSES — the port of Sp2x2RenderParticles (2026-08-22).
 *
 * Spyromain's version, which we noted long ago and deliberately deferred
 * "until an artefact shows a need":
 *
 *     player 1: for each particle, remember byte[3]
 *     player 2: for each particle, if remembered > current, put it back
 *
 * Particles AGE while they are drawn, so drawing the scene twice ages every
 * particle twice — they die at double rate and the effect flickers. Taking the
 * MAX after the second pass undoes the extra ageing without disturbing
 * anything that legitimately advanced.
 *
 * The artefact finally showed up: the dragon-head boss entrance in the
 * homeworld should trail smoke from its nostrils and instead strobes white.
 * Retail does not do it.
 *
 * Spyro 1's list is the same shape as Spyro 2's — 32-byte entries, 0xFF in
 * byte 1 terminates, 256 max (spyro-1 loaders.c) — so this is close to a
 * verbatim port. We sync BOTH byte 2 (m_Life, documented "remaining life in
 * ticks") and byte 3 (m_03, the one he syncs), since the two games may not
 * label the same field the same way and taking the max can only restore a
 * pass-1 value.
 *
 * Snapshot lives at 0x8000F600: 2 bytes x 256, ending exactly at the moby
 * flags at 0x8000F800.
 */
#define SP1X2_PARTICLE_SNAP ((volatile unsigned char *)0x8000F600)
#define SP1X2_PARTICLE_MAX  256
#define SP1_PARTICLE_STRIDE 0x20
#define SP1_PARTICLE_TYPE   1
#define SP1_PARTICLE_LIFE   2
#define SP1_PARTICLE_M03    3

void Sp1x2SyncParticles(int pass)
{
    volatile unsigned char *p = *(volatile unsigned char **)0x80075824;
    int i;

    if (p == 0) {
        return;
    }

    for (i = 0; i < SP1X2_PARTICLE_MAX; i++, p += SP1_PARTICLE_STRIDE) {
        if (p[SP1_PARTICLE_TYPE] == 0xFF) {
            break;                  /* terminator */
        }
        if (pass == 0) {
            SP1X2_PARTICLE_SNAP[i * 2]     = p[SP1_PARTICLE_LIFE];
            SP1X2_PARTICLE_SNAP[i * 2 + 1] = p[SP1_PARTICLE_M03];
        } else {
            if (SP1X2_PARTICLE_SNAP[i * 2] > p[SP1_PARTICLE_LIFE]) {
                p[SP1_PARTICLE_LIFE] = SP1X2_PARTICLE_SNAP[i * 2];
            }
            if (SP1X2_PARTICLE_SNAP[i * 2 + 1] > p[SP1_PARTICLE_M03]) {
                p[SP1_PARTICLE_M03] = SP1X2_PARTICLE_SNAP[i * 2 + 1];
            }
        }
    }
}


/*
 * PORTAL / LEVEL-TRANSITION SEQUENCE: draw every dragon, not just player 1.
 *
 * The sequence runs in GS_LevelTransition (1) and GS_EntranceAnimation (9) —
 * both dispatch to the same draw handler at 0x8001a050 — and our render hook
 * bails straight to stock GamestateDraw for any non-zero gamestate, so only
 * the live (player 1) dragon was ever drawn. Player 2 popped into existence
 * when the split screen came back after the backflip.
 *
 * Spyromain's Sp2x2RenderEntities draws both dragons in FIVE game states, not
 * just gameplay, which is the same conclusion: the second dragon belongs in
 * the sequence views too.
 *
 * The handler draws Spyro with a single unconditional `jal
 * RasterizePairedActor` at 0x8001a0d8 (delay slot is a nop, so it is a clean
 * one-instruction hook), and this is a single-camera view — so a second dragon
 * is just that same renderer called again.
 *
 * WE DRAW THE SAME DRAGON TWICE rather than swapping player 2's state in.
 * The first version swapped, and player 2 came out standing in whatever pose
 * he was frozen in while player 1 played the portal flying animation. During a
 * scripted sequence both dragons should be doing the same thing, and the
 * simplest way to guarantee that is to reuse the live dragon — animation
 * frames, rotation, pose and all — moved sideways. It is also markedly less
 * code, which matters here (see below). Spyro 1 has no per-player colouring,
 * so the two are visually identical anyway.
 *
 * LIVES IN THIS FILE, not next to Sp1x2DrawPlayer2Spyro, purely for space:
 * BIOS2 is full (code ceiling 0x8000E000, and growing the payload black-
 * screens the game). Sp1x2Pad.c is LOADER-resident and had room.
 *
 * TWO THINGS ARE RESTORED AFTERWARDS, so this leaves no trace in real state:
 *
 * 1. POSITION. Offset by SP1X2_P2_START_OFFSET — the SAME constant that places
 *    player 2 when he is seeded into the level, so the spacing during the
 *    sequence is the spacing after the backflip and the dragons do not jump
 *    apart when the split screen returns.
 *
 *    ALONG THE DRAGON'S OWN LATERAL AXIS — his wing line.
 *
 *    THE FLIGHT IS FAKE, and that is the whole reason this was hard. During
 *    the fly-in the dragon does not travel anywhere: camera.c:452-460 parks him
 *    at a fixed m_SpyroPosition with a fixed m_SpyroRotation, then ORBITS THE
 *    CAMERA around him on spherical coordinates while the cyclorama scrolls.
 *    The sense of motion is entirely the moving background.
 *
 *    So there is no stable "beside him" in world space, and three attempts each
 *    failed differently for the same underlying reason:
 *      - along world axis 0: the camera orbit periodically looks straight down
 *        that axis, and the gap collapses into DEPTH;
 *      - along row 0 of the matrix at g_Camera + 0x14: came out VERTICAL. That
 *        matrix is not even the one Spyro is drawn with — a footprint scan of
 *        r_pete shows it reads g_Camera + 0x00 (the projection matrix) and
 *        + 0x28 (m_Position), and nothing else;
 *      - perpendicular to the camera->dragon line: correct on screen, and
 *        MEASURED correct (a full 768-unit offset square to the view), but it
 *        is anchored to the camera, so as the camera orbits the second dragon
 *        slides around the first. Stable on screen, wrong in the scene.
 *
 *    Anchoring to the dragon instead makes the pair a RIGID FORMATION that
 *    turns with him. The orbiting camera then simply views that formation from
 *    changing angles, which is what two dragons flying together should look
 *    like, and the illusion survives.
 *
 * 2. THE FLAME MATRIX CHAIN. RasterizePairedActor nudges the running matrix at
 *    g_SpyroFlame + 0xB8 every time it draws (see the chain machinery in
 *    Sp1x2Graphics.c — this cost us the bent-flame hunt). Retail nudges it
 *    once here; a second nudge would leave the chain somewhere retail never
 *    puts it. Save it before, restore it after.
 */
#define SP1X2_P2_READY_F  ((volatile int *)0x8000ED00)  /* Sp1x2Spyro.c owns it */
#define SP1X2_PORTAL_MTX  ((volatile int *)0x80078780)  /* g_SpyroFlame + 0xB8 */

/* g_Spyro.m_Physics.m_SpeedAngle.m_RotZ — his yaw, 0x1000 to the full turn.
   Offset 0x11C, cross-checked rather than counted: spyro-1's pete.c reads
   m_Physics.m_TrueVelocity.z in the invulnerability branch, and the
   disassembly of that branch loads 0x80078B6C = g_Spyro + 0x114, which fixes
   m_Physics at 0x C8 and m_SpeedAngle at 0x118.
   THE FLY-IN SETS THIS FIELD DIRECTLY (camera.c:460), so it is exactly the
   orientation the dragon is drawn with during the sequence. */
#define SP1_SPYRO_YAW  (*(volatile int *)(0x80078A58 + 0x11C))

/* COSINE_8: 256 entries, SIGNED shorts, 0x1000 = 1.0. Reading it unsigned is
   what once spun the pause-menu letters backwards. */
#define SP1_COS8       ((volatile short *)0x8006cc78)

/* Where the second dragon belongs relative to the LIVE one: out along his own
   lateral axis, his wing line.
 *
 * SHARED ON PURPOSE, and this is the point of it. Two places decide where
 * player 2 goes — this sequence draw, and Sp1x2InitPlayer2Spyro when he is
 * actually seeded into a level — and they used DIFFERENT rules: body-relative
 * here, a flat world +X shove there. The two only agreed when Spyro happened
 * to be facing one particular way, so the wingman visibly jumped to the other
 * side at the moment the sequence ended and the split screen returned. One
 * function, so they cannot disagree again.
 *
 * Caller must have the dragon it is asking about swapped in. */
void Sp1x2FormationOffset(int *out)
{
    int b = (SP1_SPYRO_YAW >> 4) & 0xFF;   /* 0x1000 per turn -> 256 */
    int c = SP1_COS8[b];                   /* cos(yaw), 0x1000 = 1.0 */
    int s = SP1_COS8[(b - 64) & 0xFF];     /* sin(yaw) = cos(yaw - 90) */

    /* HEADING IS (cos, -sin), established by observation rather than by
       reasoning about Atan2's argument order: offsetting along that vector
       drew the second dragon directly BEHIND the first, nose to tail, so it is
       the forward axis. The wing line is that turned 90 degrees —
       (x, y) -> (-y, x) — giving (sin, cos). */
    out[0] = (s * SP1X2_P2_START_OFFSET) >> 12;
    out[1] = (c * SP1X2_P2_START_OFFSET) >> 12;
    out[2] = 0;
}


void Sp1x2DrawPortalSpyro(void)
{
    volatile int *pos = (volatile int *)0x80078A58;   /* g_Spyro.m_Position */
    int saved[3], mtx[5];
    int right[3];
    int i;

    if (*SP1X2_P2_READY_F == 0) {
        RasterizePairedActor();      /* one dragon: exactly what stock does */
        return;
    }

    Sp1x2FormationOffset(right);

    /* DRAW ORDER IS THE DEPTH SORT. The PS1 has no Z-buffer; everything is
       ordered through the OT, and Spyro's model evidently enters it at a
       coarse depth rather than per-polygon — retail never had to care, having
       only ever one Spyro. With two in the same bin the later draw wins
       outright, so the second dragon showed through the first even when he was
       behind him. Whichever is FARTHER from the camera therefore has to be
       drawn first, and the nearer one paints over him.
       (Harmless if the OT does sort finely after all: then order is moot.) */
    {
        int d[3], dist_p1, dist_p2, k;

        for (i = 0; i < 3; i++) {
            d[i] = pos[i] - g_anCameraPos[i];
        }
        dist_p1 = VecMagnitude(d, 1);
        for (i = 0; i < 3; i++) {
            d[i] += right[i];
        }
        dist_p2 = VecMagnitude(d, 1);

        for (k = 0; k < 2; k++) {
            /* k == 0 is the first draw. Put the extra dragon there when he is
               the farther of the two. */
            int extra = (dist_p2 > dist_p1) ? (k == 0) : (k == 1);

            if (!extra) {
                RasterizePairedActor();      /* player 1, retail's own draw */
                continue;
            }

            /* The extra dragon must leave no trace: neither his position nor
               the flame chain his draw nudges. */
            for (i = 0; i < 5; i++) {
                mtx[i] = SP1X2_PORTAL_MTX[i];
            }
            for (i = 0; i < 3; i++) {
                saved[i] = pos[i];
                pos[i]   = saved[i] + right[i];
            }

            RasterizePairedActor();

            for (i = 0; i < 3; i++) {
                pos[i] = saved[i];
            }
            for (i = 0; i < 5; i++) {
                SP1X2_PORTAL_MTX[i] = mtx[i];
            }
        }
    }
}

/* MOVED HERE FROM Sp1x2Spyro.c PURELY FOR SPACE (2026-08-22): BIOS2 ran out
   while adding the last-life handling, and this is the most self-contained
   thing in it — a leaf hooked straight from main.S that needs only the two
   camera positions. LOADER had room. Nothing about it is pad-related. */
int Sp1x2SoundListenerDistance(int *diff, int flag)
{
    volatile int *live_cam   = (volatile int *)(0x80076dd0 + 0x28);
    volatile int *shadow_cam = (volatile int *)(0x8000EE00 + 0x28);
    int v[3], i, d1, d2;

    d1 = VecMagnitude(diff, flag);
    if (*SP1X2_P2_READY_F == 0 ||
        ((volatile unsigned char *)0x80078E50)[0] != 0) {
        return d1;
    }
    for (i = 0; i < 3; i++) {
        v[i] = diff[i] + live_cam[i] - shadow_cam[i];  /* mobyPos - otherCam */
    }
    d2 = VecMagnitude(v, flag);
    return (d2 < d1) ? d2 : d1;
}
