#include "Sp1.h"

/*
 * Spyro 1 co-op: render hook. Replaces the game's only call to
 * GamestateDraw() (at ram 0x8001227c).
 *
 * PHASE B2 (current): renders the scene TWICE into one frame, from two camera
 * positions, into the top and bottom halves of the draw area.
 *
 * Measured geometry (see SPYRO1_PORT_PLAN.md): the game is 512x240 with a
 * 512x224 draw area, double buffered — buffer 0 displays at VRAM y=0 / draws
 * at y=8, buffer 1 displays at y=240 / draws at y=248. Half of 224 is 112
 * lines per player.
 *
 * Views are SQUASHED to fit, not cropped — see Sp1x2SquashViewY below. That
 * turned out far cheaper than Spyromain's approach in Spyro 2 (walking the GPU
 * primitive list halving every Y coordinate), because Spyro 1 already scales
 * the view matrix's Y row for aspect correction and we simply scale it more.
 *
 * Known limitations of this build, by design:
 *   - There is a DrawSync(0) stall between passes, which costs framerate.
 *     See the comment at the stall for why, and how to remove it later.
 *   - Player 2 has his own camera (Sp1x2UpdateCameras) and his own Spyro.
 *
 * Line-for-line reference: decomps/open-spyro/src/c/GamestateDraw.c
 * Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
 */

/* Marker RAM at 0x8000C000 — free BIOS memory, verified. Read in PCSX-Redux.
 *   +0x00 0xDEADBEEF   +0x04 frame counter
 *   +0x08..+0x24 draw/display geometry (x,y,w,h each)
 *   +0x28 ofs.x        +0x2c ofs.y      <- new in B2 */
#define SP1X2_MARKER ((volatile unsigned int *)0x8000F000)


/* DRAWENV as shorts: [0]=clip.x [1]=clip.y [2]=clip.w [3]=clip.h
                      [4]=ofs.x  [5]=ofs.y                          */
#define SP1_CLIP_X  0
#define SP1_CLIP_Y  1
#define SP1_CLIP_W  2
#define SP1_CLIP_H  3
#define SP1_OFS_X   4
#define SP1_OFS_Y   5

/*
 * SPLIT ORIENTATION — a runtime mode, not a compile-time constant, because
 * this is what the planned pause-menu "Multiplayer" settings will write into.
 * Anything other than SP1X2_SPLIT_VERTICAL means horizontal, so uninitialised
 * BIOS3 garbage falls back to the proven top/bottom layout.
 *
 * Set from the Multiplayer pause menu (Sp1x2Menu.c). The SELECT-button
 * scaffolding that predated the menu was removed 2026-08-22.
 */

/*
 * WIDESCREEN (16:9), also a runtime mode for the same pause-menu reason.
 *
 * This is ANAMORPHIC, the standard PS1 approach: we compress the horizontal
 * scale so MORE of the world fits into the same framebuffer width, and the
 * display stretches it back out to 16:9. Vertical field of view is untouched,
 * so it matches how the game was framed.
 *
 * 3/4 is the exact 4:3 -> 16:9 ratio. It scales the SAME matrix row that a
 * side-by-side split halves, so the two compose correctly: vertical split
 * plus widescreen ends up at 0.5 * 0.75.
 *
 * Set from the Multiplayer pause menu.
 */

/*
 * VIEW FIT — how a full-size scene is made to fit a half-size viewport. The
 * third runtime setting, and the third pause-menu entry.
 *
 *   0 FULL      squash to 1/2  — the whole scene, horizontally or vertically
 *                               compressed. What we have shipped so far.
 *   1 BALANCED  squash to 3/4  — mild distortion, mild crop. Middle ground.
 *   2 CROPPED   no squash      — correct proportions, but each player sees
 *                               only the middle slice of the scene.
 *
 * Anything outside 0..2 behaves as FULL, so uninitialised BIOS3 keeps the
 * proven behaviour.
 *
 * Set from the Multiplayer pause menu.
 */


/* Squash the scene vertically so a full 224-line view fits in a 112-line
   viewport — otherwise each player sees a CROP of the top half rather than
   the whole scene.
 *
 * Row 1 of the world-to-camera matrix produces camera-space Y. The game
 * already scales that row by 320/512 for aspect correction (see Sp1.h), so
 * scaling it again is the engine's own idiom, not a hack.
 *
 * Must be called AFTER every BuildCameraViewMatrix(), because that function
 * rewrites the matrix from the Euler angles each time and would undo this.
 */
static void Sp1x2SquashView(void)
{
    /* Row 0 of the matrix produces camera-space X, row 1 camera-space Y. A
       top/bottom split halves the available height, so squash Y; a
       side-by-side split halves the width, so squash X instead. */
    int base = (SP1X2_SPLIT_MODE == SP1X2_SPLIT_VERTICAL) ? 0 : 3;
    int fit  = SP1X2_VIEW_FIT;
    int num  = (fit == SP1X2_FIT_BALANCED) ? 3 : 1;
    int den  = (fit == SP1X2_FIT_BALANCED) ? 4 :
               (fit == SP1X2_FIT_CROPPED)  ? 1 : 2;
    int i;

    /* CROPPED leaves the matrix alone: the scene renders at full scale and the
       viewport simply shows the middle of it.
     *
     * ONLY g_anWorldToCameraRotMtx is scaled. DO NOT scale the pure copy at
     * g_Camera+0x14 — this was tried twice and both times produced blocky
     * green geometry in the distance at the widest settings.
     *
     * Why it looked promising: with widescreen we draw a wider view than the
     * game culls for, so terrain and portal doors at the new left/right edges
     * pop in and out, and scaling the pure copy did reduce that.
     * Why it is wrong: that copy is NOT a culling matrix. r_environment.s
     * loads it straight into the GTE ("Load in the camera matrix", 0x80025934
     * -> ctc2 C2_R11R12) and TRANSFORMS distant world geometry with it, and
     * r_cyclorama and r_shadows read it too. Scaling it therefore distorts
     * what it draws. The edge popping is a known, accepted widescreen
     * trade-off; see the punch list in CLAUDE.md for the proper approach. */
    /* SHIFTS, NOT DIVISIONS (2026-08-30, to buy back bytes). Dividing a
       SIGNED value emits a round-toward-zero correction at every site; these
       matrix rows are signed, so `/2` and `/4` each cost several extra
       instructions. Shifting rounds toward negative instead, which differs by
       at most one unit in 4096 — far below anything visible, and the aspect
       correction the game itself applies (`* 0x140 >> 9`) is a shift for the
       same reason. BALANCED's 3/4 keeps its divide: there is no shift for it. */
    if (den != 1 && !SP1X2_SOLO) {
        /* SOLO gets the whole screen, so there is no lost half to compensate
           for — the split squash would just shrink the picture. Widescreen
           below still applies: it is a projection choice, independent of how
           the screen is divided. */
        for (i = 0; i < 3; i++) {
            int v = g_anWorldToCameraRotMtx[base + i];
            g_anWorldToCameraRotMtx[base + i] =
                (short)((den == 2) ? (v >> 1) : ((v * num) / den));
        }
    }

    /* Widescreen: compress X by 3/4 so a wider slice of the world fits the
       same framebuffer, for the display to stretch back to 16:9. Applied
       after the split squash, so the two multiply cleanly. */
    if (SP1X2_WIDESCREEN) {
        for (i = 0; i < 3; i++) {
            g_anWorldToCameraRotMtx[i] =
                (short)((g_anWorldToCameraRotMtx[i] * 3) >> 2);
        }
    }
}


/* The two render passes share a head and a tail. Factored 2026-08-30 to buy
   bytes for the per-player colour option; behaviour is unchanged. */
static void Sp1x2AimCamera(void)
{
    BuildCameraViewMatrix();
    Sp1x2SquashView();          /* must follow EVERY BuildCameraViewMatrix —
                                   it rebuilds the matrix from the Euler
                                   angles and would undo the squash */
}


/* Point the draw env at one player's half of the screen. `saved` holds the
   env's original clip.x/y/w/h and ofs.x/y, so both layouts are expressed as
   offsets from whatever the game itself set up. */
static void Sp1x2SetViewport(short *e, int pass, const short *saved)
{
    /* UNSIGNED halving, deliberately. Dividing a SIGNED value by two makes
       the compiler emit a rounding-toward-zero correction (add the sign bit
       before shifting) at every site — 60 bytes across the six divisions
       here. A screen width and height are never negative, so the correction
       is dead weight and unsigned arithmetic gives an identical result. */
    unsigned int half, quarter;

    if (SP1X2_SPLIT_MODE == SP1X2_SPLIT_VERTICAL) {
        half = (unsigned int)(unsigned short)saved[SP1_CLIP_W] / 2;
        quarter = half / 2;
        e[SP1_CLIP_X] = (short)(saved[SP1_CLIP_X] + (pass ? (int)half : 0));
        e[SP1_CLIP_W] = (short)half;
        e[SP1_OFS_X]  = (short)(saved[SP1_OFS_X] +
                                (pass ? (int)quarter : -(int)quarter));
    } else {
        half = (unsigned int)(unsigned short)saved[SP1_CLIP_H] / 2;
        quarter = half / 2;
        e[SP1_CLIP_Y] = (short)(saved[SP1_CLIP_Y] + (pass ? (int)half : 0));
        e[SP1_CLIP_H] = (short)half;
        e[SP1_OFS_Y]  = (short)(saved[SP1_OFS_Y] +
                                (pass ? (int)quarter : -(int)quarter));
    }
}


/* Aim the draw env at this player's half and hand the frame to the GPU. The
   tail of both passes, factored for the same reason as Sp1x2AimCamera. */
static void Sp1x2SubmitPass(short *e, int pass, const short *saved,
                            unsigned char *env)
{
    Sp1x2SetViewport(e, pass, saved);
    PutDrawEnv(env);
    DrawOTag(LinkOTPrimitives(SP1_OT_DEPTH));
}




/*
 * FLAME MATRIX CHAINS — the fix for the bending flame (2026-08-18).
 *
 * Spyro's flame has no orientation of its own: r_pete (the MODEL renderer)
 * nudges a matrix at g_SpyroFlame+0xB8 each time it draws, and the flame
 * renderer reuses it. MEASURED in-game, which is what finally settled it:
 *   - the matrix changes in BOTH of our passes           (it writes each time)
 *   - pass 2's result differs from pass 1's              (camera-dependent)
 *   - this frame's start == last frame's end             (nothing reseeds it)
 * So it is a RUNNING CHAIN: new = nudge(previous, current camera). Retail
 * draws once per frame, so the chain is smooth. We draw twice, so one chain
 * was being dragged between two cameras twice per frame and never settled —
 * the error showing up in the flame's outer segments as a bend.
 *
 * Four earlier fixes failed because they all still shared ONE chain; the bend
 * just moved between viewports. The fix is to give every (dragon, viewport)
 * pair its own continuous chain, so each sees exactly what retail's single
 * chain sees: one nudge per frame, always with the same camera.
 *
 * Scratch: BIOS3 free block at 0x8000F100, 4 x 5 words, ends well before the
 * pad state at 0x8000F200.
 */
#define SP1X2_MTX ((volatile int *)0x80078780)      /* g_SpyroFlame + 0xB8 */

static volatile int *Sp1x2FlameChain(int player, int pass)
{
    return (volatile int *)(0x8000F100 + player * 0x40 + pass * 0x20);
}

void Sp1x2FlameChainXfer(int player, int pass, int store)
{
    volatile int *chain = Sp1x2FlameChain(player, pass);
    int i;
    for (i = 0; i < 5; i++) {
        if (store) {
            chain[i] = SP1X2_MTX[i];
        } else {
            SP1X2_MTX[i] = chain[i];
        }
    }
}

/* (Sp1x2MtxSame, the flame-probe relic, deleted 2026-08-26 — unused.) */





/* Everything GamestateDraw does between the fog colour and the frame submit.
   Called once per viewport. */
/*
 * PER-VIEWPORT HUD (2026-08-22).
 *
 * NO REFERENCE IMPLEMENTATION: Spyro2x2 never did this. Spyromain's only HUD
 * reference in the whole mod is one byte written in Sp2x2LogicGameWrapper, so
 * his build shows a single HUD. This is our own design, like the player
 * collision.
 *
 * It is far less work than the old note in CLAUDE.md assumed, because the
 * mechanism is already running. EnqueueLoadingScreenSprites is open-spyro's
 * name for func_80019300, and spyro-1 shows what it really is: the HUD
 * COMPOSER, which gathers g_Hud.m_Mobys into the shaded-moby list. We already
 * call it once per pass, so both viewports build their own HUD every frame.
 * The only thing wrong was WHERE it landed.
 *
 * The HUD is 2D screen space, and DRAWENV.ofs moves 2D sprites along with
 * everything else. Pass 0 shifts ofs by -half/2 and pass 1 by +half/2, so the
 * stock HUD at screen y~20 drew at y~-36 in the top viewport — off the top
 * edge, clipped away. That is the "HUD is clipped" limitation.
 *
 * Correcting it is one delta, and it is the SAME for both passes:
 *   pass 0 wants the HUD at its stock place (top of the top strip):
 *       draws at Y + d - half/2  ==  Y      ->  d = half/2
 *   pass 1 wants it one strip lower (top of the bottom strip):
 *       draws at Y + d + half/2  ==  Y + half -> d = half/2
 * so both passes simply add half/2 along the split axis.
 *
 * g_Hud = 0x80077FA8, derived from spyro-1's hud.h (two fields carry their own
 * addresses, 0x80077FE4 and 0x80077FE8, and both agree) and confirmed against
 * open-spyro, which names that address and even notes "the icon-record base at
 * +0x44" — the m_Mobys offset this uses.
 */
#define SP1_HUD        0x80077FA8
#define SP1_HUD_MOBYS  (SP1_HUD + 0x44)    /* Moby[12] */
#define SP1_HUD_RECTS  (SP1_HUD + 0x464)   /* RECT[32]: eggs + life orbs */

/* The stock layout, from g_HudMobyTargetPos (asm/data/math.data.s):
 *   mobys 0-4   gems     x  46..174   left
 *   mobys 5-7   dragons  x 230..292   middle
 *   mobys 8-10  lives    x 394..464   right
 *   moby  11    key      x 430, y 240 (bottom)
 *   rects 0-11  eggs;  rects 12+ life orbs, placed relative to moby 10
 * z is depth (2880 for digits, 1920-3072 for icons), so apparent size goes as
 * 1/z — doubling z halves the HUD without moving where it sits.
 */
#define SP1X2_HUD_HALFSIZE 1        /* set 0 to keep the HUD full size */

/*
 * THE HUD IS PERSPECTIVE-PROJECTED, and that is why it is drawn at FULL SIZE.
 *
 * Screen position is not the stored position:
 *      screen = C + (pos - C) * z_stock / z     C = (SP1_GEOM_OFX, SP1_GEOM_OFY)
 * so z is a size control that also MOVES things. Halving the HUD by doubling z
 * pulled it a third of the way down the viewport, and compensating by doubling
 * each position's offset from C put the maths right but sent the raw
 * coordinates far off-screen (gems to x = -164, the key to y = 472) — where the
 * moby renderer culls them, and the HUD disappeared entirely.
 *
 * Both failures came from the same place, so z is left alone. Full size, right
 * position, which is the trade worth having. If half size is wanted later, the
 * thing to establish FIRST is what the renderer culls a HUD moby on; the
 * projection model above is sound and its constants can be regenerated.
 *
 * With z untouched, screen == pos, so these are plain screen offsets applied to
 * each group's stock anchor (g_HudMobyTargetPos):
 *      gems (46,10)  dragons (230,10)  lives (394,10)  key (430,240)
 */
/* Top/bottom keeps the full 512 columns, so the stock across-the-top layout is
   already right — it only wants lifting nearer the viewport edge. Slight
   overhang past the clip is intentional and asked for. */
#define SP1X2_HUD_WIDE_DY (-8)
static const short sp1x2_hud_wide_row[2] = { 0, SP1X2_HUD_WIDE_DY };

/* Side-by-side gives each player 256 columns against a 464-wide HUD, so the
   groups stack down the left corner.
 *
 * PER-MOBY, not per-group, and that is the point. The digit sits a different
 * distance from its icon in each group — +44 for gems, +34 for dragons, +42
 * for lives — so translating whole groups left stacked the numbers in a ragged
 * column. Here the icons share one column (x=20) and the digits another
 * (x=68, 28 apart), which lines the counts up. Rows are 32 apart, up from 28,
 * because at full size 28 let them touch; the block starts at y=2 so it sits
 * in the corner. Widest row reaches x=152, inside 256. */
static const short sp1x2_hud_tall[12][2] = {
    {    -8,  -10 },   /* gem digit 1    -> ( 82, -10) */
    {    -8,  -10 },   /* gem digit 2    -> (110, -10) */
    {    -8,  -10 },   /* gem digit 3    -> (138, -10) */
    {    -8,  -10 },   /* gem digit 4    -> (166, -10) */
    {   -12,  -10 },   /* gem chest      -> ( 34,   0) */
    {  -182,   22 },   /* dragon digit 1 -> ( 82,  22) */
    {  -182,   22 },   /* dragon digit 2 -> (110,  22) */
    {  -196,   22 },   /* dragon icon    -> ( 34,  32) */
    {  -354,   54 },   /* life digit 1   -> ( 82,  54) */
    {  -354,   54 },   /* life digit 2   -> (110,  54) */
    {  -360,   54 },   /* Spyro head     -> ( 34,  64) */
    {  -396, -144 },   /* key            -> ( 34,  96) */
};

static void Sp1x2HudShift(int delta, int sign)
{
    volatile unsigned char *moby = (volatile unsigned char *)SP1_HUD_MOBYS;
    volatile short *rect = (volatile short *)SP1_HUD_RECTS;
    int vertical = (SP1X2_SPLIT_MODE == SP1X2_SPLIT_VERTICAL);
    int axis = vertical ? 0 : 1;
    int ox, oy;
    int i;

    if (SP1X2_SOLO) {
        return;                     /* full screen: stock layout is correct */
    }

    for (i = 0; i < 12; i++) {
        volatile int *pos = (volatile int *)
            (moby + i * SP1_MOBY_STRIDE + SP1_MOBY_POS_OFF);

        const short *a = vertical ? sp1x2_hud_tall[i] : sp1x2_hud_wide_row;

        pos[axis] += sign * delta;      /* into this player's viewport */
        pos[0]    += sign * a[0];
        pos[1]    += sign * a[1];
    }

    /* Life orbs are placed relative to the Spyro-head icon by HudReset and only
       recomputed there, so they have to travel with the lives group. */
    /* Two flat loops rather than one with a branch per entry — same result,
       noticeably less code, and BIOS2 has none to spare. */
    ox = vertical ? -360 : 0;
    oy = vertical ?   54 : SP1X2_HUD_WIDE_DY;

    for (i = 0; i < 32; i++) {
        rect[i * 4 + axis] = (short)(rect[i * 4 + axis] + sign * delta);
    }
    for (i = 12; i < 32; i++) {          /* life orbs follow the head icon */
        rect[i * 4]     = (short)(rect[i * 4]     + sign * ox);
        rect[i * 4 + 1] = (short)(rect[i * 4 + 1] + sign * oy);
    }
}

static void Sp1x2BuildScene(int pass, int hud_delta)
{
    int tint;

    /* Published for Sp1x2TickSparkles, which must age particles once per
       FRAME even though the scene is drawn once per PLAYER. */
    SP1X2_RENDER_PASS = pass;

    /* "Queue render mobys" (spyro-1's name for func_800521C0). It must run for
     * EVERY pass — queueing it once left player 2's viewport with no mobys at
     * all — but it has a side effect that two passes break: while walking the
     * level it does `sb $zero, 0x51(moby)`, CLEARING m_WasDrawn on the mobys
     * it does not queue.
     *
     * That flag is the culler's grace: BuildActorDrawList gives a moby an
     * extra 512 units of range if it was drawn last frame. So pass 2's queue
     * wipes the grace pass 1 earned, small-radius objects drop out of range,
     * and once out they are never drawn again — self-latching invisibility,
     * which is exactly how the flight-level chests behaved.
     *
     * Our m_WasDrawn union already existed but was applied after the WHOLE
     * pass, far too late for pass 2's own culling. Applying it immediately
     * after the queue call puts the grace back before anything reads it.
     *
     * NOTE: this did NOT fix the flight-level chests (tested), so their
     * invisibility is something else. Kept anyway — restoring the grace
     * before it is read is correct regardless. */
    /* Applied for the WHOLE pass, not just around the composer: that
       function only collects pointers into the shaded list, and the
       render that dereferences them happens further down this pass. */
    Sp1x2HudShift(hud_delta, 1);

    Sp1x2RegionsLoad(pass);        /* this viewport's region visibility */
    BuildRenderEntityLists();
    if (pass != 0) {
        Sp1x2SyncMobyFlags(1);
    }
    if (g_nFlightLevelActive == 0) {
        EnqueueLoadingScreenSprites();
    }
    if (g_nDeathState != 0) {
        DrawDemoModeOverlay();
    }
    /* THE FLAME FIX (2026-08-18). Spyro's flame has no orientation of its
     * own: r_pete, the MODEL renderer, leaves an orientation matrix in
     * g_SpyroFlame+0xB8 and the flame renderer reuses it. Retail draws the
     * model once per frame, so that write happens once. We draw the scene
     * twice, so it happened twice — and the second write corrupted the jet,
     * bending its outer half.
     *
     * But it cannot simply be skipped for pass 2 either: that was tried, and
     * pass 2's flame then bent FROM THE START, because the matrix already has
     * the camera baked in and pass 2 was left holding pass 1's camera. Both
     * facts together mean r_pete does not build the matrix from scratch — it
     * COMPOUNDS onto whatever is already there. So: snapshot the matrix before
     * pass 1 writes it, restore that same base before pass 2, and let each
     * pass build its own. One write per pass, from an identical starting
     * point, correct for each camera.
     *
     * Found by bisection after nine experiments aimed (wrongly) at player 2:
     * the bend reproduced with controller 2 DISCONNECTED, which cleared him
     * entirely; single-pass rendering was smooth; suppressing the model in
     * pass 2 was smooth; suppressing just this write was smooth. */
    /* Player 1's dragon: his chain for THIS viewport goes in, r_pete nudges
       it, the flame (drawn inside ComposeFrameScene) reads it, and the
       advanced value goes back to the same chain. */
    Sp1x2FlameChainLoad(0, pass);
    ComposeFrameScene();
    Sp1x2FlameChainStore(0, pass);
    Sp1x2DrawPlayer2Spyro(pass);   /* second dragon — see Sp1x2Spyro.c */
    SetupFrameOT();
    DrawActors();
    RasterizeEmitList();

    if (g_nGenericCountdown != 0) {
        tint = g_nGenericCountdown << 3;
        DrawFullscreenTint(2, tint, tint, tint);
    }
    if (g_nGameplayBlocked != 0 || g_nLetterboxBarHeight != 0) {
        DrawCinematicLetterbox();
    }
    DrawMotionTrailRibbons();

    /* Particles age as they are drawn; undo the second pass's extra ageing.
       Port of Sp2x2RenderParticles — see Sp1x2Pad.c. */
    Sp1x2SyncParticles(pass);

    /* SetupFrameOT (the world renderer) has now rewritten the region table for
       THIS camera — keep it for this viewport's next frame. */
    Sp1x2RegionsStore(pass);

    Sp1x2HudShift(hud_delta, -1);  /* leave g_Hud exactly as HudTick left it */
}


void Sp1x2Graphics(void)
{


    unsigned char *env;
    unsigned char *cursor;
    unsigned char *limit;
    void *otbase;
    void *otslot;
    short *e;
    short saved_env[6];
    int hud_delta;
    unsigned char r, g, b;


    if (g_nGamestate != 0) {
        GamestateDraw();
        return;
    }

    /* ---- frame setup (GamestateDraw's head) ---- */
    env = g_abFrameDrawEnv0;
    if ((unsigned char *)g_pActiveFrameDrawEnv == env) {
        env += SP1_ENV_STRIDE;
    }
    otbase = *(void **)(env + SP1_ENV_OTBASE);
    otslot = *(void **)(env + SP1_ENV_OTSLOT);
    cursor = *(unsigned char **)(env + SP1_ENV_PRIMCURSOR);
    limit  = cursor + SP1_PRIM_BUFFER_SIZE;

    g_pActiveFrameDrawEnv      = env;
    g_nPrimBufferOverflowFlag  = 0;
    g_pPrimBufferWriteCursor   = cursor;
    g_pOtDepthBinArrayBase     = otbase;
    g_pOtActiveDepthSlot       = otslot;
    g_pPrimBufferLimit         = limit;
    g_pSpriteRecordBufferTop   = limit;
    g_pSpriteRecordWriteCursor = limit;

    /* HUD TEXT MOBYS — restored 2026-08-20. GamestateDraw does
     * `g_HudMobys = polyBuf + 0x1C000` every frame, and the text builders walk
     * it DOWNWARD one Moby at a time (`g_HudMobys -= 1`). We reimplemented the
     * frame head and left this line out, so during gameplay nothing reset it:
     * the pointer only ever descended, frame after frame, through the polygon
     * buffer. Menus happened to hide it because they run the stock draw, which
     * resets it.
     * Found while investigating why giving pass 2 its own ordering table
     * corrupted the frame — a drifting pointer would eventually write into
     * exactly that new region. */
    g_HudMobysPtr = limit;

    /* World fog colour becomes the background clear colour in both envs. */
    r = ((unsigned char *)&g_dwWorldFogColor)[0];
    g = ((unsigned char *)&g_dwWorldFogColor)[1];
    b = ((unsigned char *)&g_dwWorldFogColor)[2];
    g_abFrameDrawEnv0[SP1_DRAWENV_R0 + 0] = r;
    g_abFrameDrawEnv0[SP1_DRAWENV_R0 + 1] = g;
    g_abFrameDrawEnv0[SP1_DRAWENV_R0 + 2] = b;
    g_abFrameDrawEnv1[SP1_DRAWENV_R0 + 0] = r;
    g_abFrameDrawEnv1[SP1_DRAWENV_R0 + 1] = g;
    g_abFrameDrawEnv1[SP1_DRAWENV_R0 + 2] = b;

    /* The env is the game's own persistent state — remember what to put back. */
    e             = (short *)env;
    saved_env[SP1_CLIP_X] = e[SP1_CLIP_X];
    saved_env[SP1_CLIP_Y] = e[SP1_CLIP_Y];
    saved_env[SP1_CLIP_W] = e[SP1_CLIP_W];
    saved_env[SP1_CLIP_H] = e[SP1_CLIP_H];
    saved_env[SP1_OFS_X]  = e[SP1_OFS_X];
    saved_env[SP1_OFS_Y]  = e[SP1_OFS_Y];

    /* The SELECT/START button toggles that used to live here were removed
       2026-08-22: the Multiplayer pause menu has owned these settings since
       2026-08-19, and the space paid for player 2's Sparx. */

    /* Half the split axis, matching Sp1x2SetViewport's own `half`, and the
       HUD moves by half of that. Solo takes no viewport shift, so no HUD
       shift either. */
    hud_delta = SP1X2_SOLO ? 0 :
                ((SP1X2_SPLIT_MODE == SP1X2_SPLIT_VERTICAL)
                     ? saved_env[SP1_CLIP_W] : saved_env[SP1_CLIP_H]) / 4;

    /* ================= PASS 1 — player 1, top half ================= */
    Sp1x2AimCamera();
    Sp1x2BuildScene(0, hud_delta);
    Sp1x2SyncMobyFlags(0);             /* record what player 1's pass saw */

    /* Frame submit happens ONCE, here, before the first DrawOTag. Two
       viewports, one frame, one display flip. */
    DrawSync(0);
    if (g_nDeathRespawnPending != 0) {
        VSync(0);
    }
    g_nVsyncFrameEndCount = VSync(-1);
    while (g_nVsyncFrameEndCount - g_nVsyncFramePaceAnchor < 2) {
        VSync(0);
        g_nVsyncFrameEndCount = VSync(-1);
    }
    g_nVsyncFramePaceAnchor = VSync(-1);
    PutDispEnv(env + SP1_ENV_DISPENV);

    /* Top 112 lines, with ofs.y shifted up by half/2 to centre the view.
     *
     * WHY the shift: the squash compresses toward the projection centre
     * (OFY=120, mid draw area), so content lands at 64..176 while this strip
     * shows 8..120. Confirmed visually — Spyro sat exactly on the boundary
     * between the two viewports, because he IS the compression centre.
     *
     * WHY ofs.y and not SetGeomOffset — both were tried:
     *
     *   SetGeomOffset(256, 64) moves only 3D, so the 2D HUD stays put. It
     *   fixed the HUD, but every ACTOR (gnorcs, sheep, gems, dragon statues)
     *   floated into the sky while the terrain stayed correct. The world
     *   rasteriser RasterizeEmitList is hand-written assembly that drives the
     *   GTE itself, so it and the actor path disagree about where screen
     *   centre is; changing OFY moved only one of them.
     *
     *   ofs.y is applied by the GPU to every primitive uniformly, so world and
     *   actors always agree. The cost is that 2D sprites move too, pushing the
     *   gem-counter HUD (screen y ~20) to y=-36, outside the clip.
     *
     * Correct 3D with a clipped HUD beats correct HUD with floating scenery,
     * so ofs.y wins for now. The HUD needs per-player handling regardless — a
     * single full-size HUD across a split screen is wrong anyway — so this is
     * deferred, not abandoned. See SPYRO1_PORT_PLAN.md. */
    if (SP1X2_SOLO) {
        /* One dragon: leave the game's own clip and offset exactly as they
           were, draw once, and skip the second pass and its DrawSync. This is
           the whole cost saving of solo mode — the mod is CPU-bound on
           geometry, so one pass is close to retail framerate. */
        PutDrawEnv(env);
        DrawOTag(LinkOTPrimitives(SP1_OT_DEPTH));
        return;
    }

    Sp1x2SubmitPass(e, 0, saved_env, env);

    /* Flight levels used to render a single view here, to work around their
       invisible chests. That workaround is GONE: the cause was the shared
       region-visibility table (see Sp1x2RegionsLoad in Sp1x2Pad.c), and with
       one table per viewport flight levels render split-screen like any other
       level. Player 2 is still frozen in them — that is the separate steering
       fix in Sp1x2Spyro.c. */

    /* ---- wait for the GPU before touching shared state ----
       DrawOTag is asynchronous, and pass 2 rebuilds the shared primitive
       buffer and ordering table, so without this we would corrupt pass 1
       mid-draw.

       MEASURED 2026-08-20 AND KEPT ON PURPOSE. Pass 2 was given its own
       buffer and ordering table (which worked correctly) and this wait was
       removed. The result: 18 FPS by the waterfalls before, 18 FPS after —
       NO GAIN — plus a freeze when retrying a flight level. The reason is
       the same measurement that made it look promising: the mod is
       CPU-bound, so the GPU had already finished viewport 1 by the time the
       CPU wanted the buffer. This wait costs almost nothing. Do not spend
       time here again; the cost is geometry, not synchronisation. */
    DrawSync(0);

    /* ================= PASS 2 — player 2, bottom half ================= */
    /* Player 2's REAL camera — updated once per frame by
       Sp1x2UpdateCameras — is swapped in for this whole pass, so
       BuildCameraViewMatrix reads HIS euler angles. No-ops until he is
       seeded, in which case this pass shows player 1's view. */
    Sp1x2SwapCameraState();
    Sp1x2AimCamera();

    /* Rewind the primitive buffer. Safe now the GPU has finished pass 1. */
    g_nPrimBufferOverflowFlag  = 0;
    g_pPrimBufferWriteCursor   = cursor;
    g_pSpriteRecordWriteCursor = limit;
    g_HudMobysPtr              = limit;

    /* DIAGNOSTIC (flame bend, 2026-08-18). Measured: the bend appears only
       in pass 1, only while the dragons are close, and stops the moment
       PLAYER 1 leaves PLAYER 2's camera — i.e. it tracks whether P1's flame
       is DRAWN A SECOND TIME in pass 2. Hypothesis: the flame renderer
       advances trail state as it draws, so drawing it twice per frame
       consumes the trail at double rate.
       Test: blank P1's flame-active byte for pass 2 only, so his flame is
       drawn exactly once per frame. Costs seeing his fire in P2's viewport —
       diagnostic only. P2's flame is unaffected: our draw swaps his state in,
       so it reads HIS flag, not this blanked one. */
    /* DIAGNOSTIC step 3 (flame bend, 2026-08-18). Established: single pass
     * with the SAME half viewport does NOT bend, so the second pass is the
     * cause — and it reproduces with controller 2 disconnected, i.e. with
     * both passes using the SAME camera. So it is not a camera problem: it is
     * drawing the scene twice.
     *
     * Prime suspect: r_pete (Spyro's model renderer) writes the flame's
     * orientation matrix as a side effect, and it runs again in pass 2.
     * Suppress Spyro's model+shadow for pass 2 only (the flame's own draw is
     * OUTSIDE that guard in ComposeFrameScene, so the flame still renders).
     *   smooth -> the second r_pete call is what breaks the flame
     *   bends  -> something else in the second pass */
    Sp1x2BuildScene(1, hud_delta);  /* seen by EITHER player = seen */

    /* Bottom 112 lines. BOTH clip and ofs shift — clip alone would confine
       drawing to the lower half while geometry still landed up top, which is
       precisely the mistake that broke the Spyro 2 HMARGIN experiment. */
    /* Bottom 112 lines. Mirror of pass 1: the two offsets sit symmetrically
       either side of the original, at -half/2 and +half/2. Content lands on
       120..232, exactly matching this strip's clip. */
    Sp1x2SubmitPass(e, 1, saved_env, env);

    /* Put the game's state back exactly as we found it. */
    Sp1x2SwapCameraState();
    e[SP1_CLIP_X] = saved_env[SP1_CLIP_X];
    e[SP1_CLIP_Y] = saved_env[SP1_CLIP_Y];
    e[SP1_CLIP_W] = saved_env[SP1_CLIP_W];
    e[SP1_CLIP_H] = saved_env[SP1_CLIP_H];
    e[SP1_OFS_X]  = saved_env[SP1_OFS_X];
    e[SP1_OFS_Y]  = saved_env[SP1_OFS_Y];

}


/*
 * ARRIVAL CAPTURE + TRUE-START CACHE (2026-08-23). Two fallback respawn
 * points for deaths with no checkpoint stood on (the game's own slot holds
 * STALE LEVEL coordinates after a portal exit — retail repairs that inside
 * the death-reload we skip):
 *
 *   0x8000ED20  the ARRIVAL spot (x,y,z,rot) — wherever the game placed us
 *               entering this level. Always captured.
 *   0x8000ED30  the TRUE START cache (levelId, x,y,z,rot) — the game-start
 *               spawn point of this world. Captured only when the arrival
 *               MATCHES the checkpoint slot, which retail guarantees exactly
 *               on a FRESH load (it places Spyro from the slot) and never on
 *               a portal exit or balloon pad. So after returning from a
 *               level, the cache still holds this homeworld's real start —
 *               respawning there instead of on the portal lip, where a
 *               respawned player could stumble straight back in.
 *
 * Called once per level entry, from the player-2 seed.
 */
void Sp1x2CaptureSpawn(void)
{
    volatile int *cap   = (volatile int *)0x8000ED20;
    volatile int *slot  = (volatile int *)(0x80077888 + 0x50);
    volatile int *cache = (volatile int *)0x8000ED30;
    int i, near = 1;

    cap[0] = g_anSpyroWorldPos[0];
    cap[1] = g_anSpyroWorldPos[1];
    cap[2] = g_anSpyroWorldPos[2];
    cap[3] = ((volatile unsigned char *)0x80078A58)[0x0E];  /* bodyRot z */

    /* Two axes suffice (a fresh load copies the position exactly), and the
       unsigned trick folds the abs+compare: |d| <= 0x100 iff d+0x100 fits in
       0..0x200. */
    for (i = 0; i < 2; i++) {
        if ((unsigned int)(cap[i] - slot[i] + 0x100) > 0x200) {
            near = 0;
        }
    }
    if (near) {
        cache[0] = g_LevelId;
        cache[1] = slot[0];
        cache[2] = slot[1];
        cache[3] = slot[2];
        cache[4] = slot[3];        /* the slot rot sits at pos[3] (+0x5C) */
    }
}



/* Sp1x2CamGate MOVED to Sp1x2Gates.c (2026-08-27) — it is an entry gate
   like the two collision gates, and BIOS2 had no room for the elevation
   clamps. See that file. */


/* BOOT STAGE 2 (2026-08-23): copies payload chunks B and C into BIOS2B and
   BIOS2C. Lives HERE (in BIOS2, itself copied by stage 1) so the LOADER stub
   stays small — stage 1 copies BIOS2, flushes the I-cache, then calls this. */
void Sp1x2BootStage2(void)
{
    volatile unsigned int *src = (volatile unsigned int *)0x80075800;
    volatile unsigned int *d;
    int i;

    d = (volatile unsigned int *)0x8000E400;          /* chunk B */
    for (i = 0; i < 0x400 / 4; i++) {                /* 0x400 since 2026-08-27
                                                        — see spyro1x2.ld */
        d[i] = src[0x800 + i];
    }
}


