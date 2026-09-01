#ifndef SP1_H_INCLUDED
#define SP1_H_INCLUDED

/*
 * Spyro 1 (NTSC, SCUS_942.28) base-game declarations.
 *
 * IMPORTANT: unlike Spyro2x2's Sp2.h, there are deliberately NO addresses in
 * the comments here. In Spyro2x2 those comments are PAL addresses while the
 * NTSC build overrides them in symbols.ld, which is a trap for anyone reading
 * the header. Here, projects/ntsc/symbols.ld is the single source of truth.
 *
 * Names match the upstream reverse-engineering projects exactly so you can
 * grep decomps/ for context. See symbols.ld for provenance.
 */


/* Main render entry point. Called once per frame from the game loop, from a
   single call site at ram 0x8001227c. This is the Spyro 1 counterpart of
   Spyro 2's Sp2Graphics(). */
extern void GamestateDraw(void);

/* Main logic entry point, called from ram 0x80012230 — 0x4c before the draw
   call, in the same game loop. Counterpart of Sp2LogicGameWrapper(). */
extern void GamestateUpdate(void);


/* Which top-level state the game is in. 0 = gameplay; 1-0xF are menus,
   cutscenes, save/load, titlescreen and credits. GamestateDraw dispatches on
   this, and only state 0 runs the full scene render. Counterpart of
   sp2_game_state. */
extern int g_nGamestate;

/* Camera world position, XYZ (3 x 32-bit, 0xc bytes total).
   Consumed by BuildCameraViewMatrix(), which GamestateDraw calls near its top
   BEFORE dispatching on g_nGamestate — so changing this just before calling
   GamestateDraw affects the frame that is about to be drawn.
   Recomputed every frame by the camera update, which runs earlier in the game
   loop (UpdateGameplayCamera, called from ram 0x8003730c). */
extern int g_anCameraPos[3];


/* ------------------------------------------------------------------------
 * The gameplay render path (GamestateDraw's g_nGamestate == 0 branch).
 *
 * Read decomps/open-spyro/src/c/GamestateDraw.c alongside this — it is the
 * decompiled original and the reason we know any of this.
 * ---------------------------------------------------------------------- */

/* Turns the camera globals into the view matrix. GamestateDraw calls this
   once, near the top. Calling it again after swapping camera state is what
   makes a second viewpoint possible. */
extern void BuildCameraViewMatrix(void);

/* The 3x3 matrix the renderer transforms vertices with (0x14 = 20 bytes:
   nine shorts + padding). Fixed point, 0x1000 = 1.0.

   BuildCameraViewMatrix writes the FULL matrix to 0x80076de4, then writes a
   copy here with row 1 (the row producing camera-space Y) scaled by
   0x140 >> 9 = 320/512 = 0.625 — the game's own aspect correction for
   512x240 on a 4:3 screen.

   Scaling that row further squashes the whole scene vertically, which is how
   we fit a 224-line view into a 112-line viewport. The game validates the
   technique by using it itself.

   As shorts: [0..2] = row 0, [3..5] = row 1 (Y), [6..8] = row 2. */
extern short g_anWorldToCameraRotMtx[10];

/* The SECOND matrix BuildCameraViewMatrix writes (g_Camera + 0x14) — the
   un-aspect-corrected copy. Per spyro-1's own annotation in r_environment.s,
   the WORLD renderer CULLS with this one (0x80025934) while DRAWING with the
   scaled copy above (0x800261B0); the skybox, shadows and a math helper read
   it too. Any scaling applied to the render matrix must be applied here as
   well, or we draw a wider view than we cull for. */
extern short g_anCameraPureRotMtx[10];

/* Scene building, in the order GamestateDraw calls them. */
extern void BuildRenderEntityLists(void);
extern void EnqueueLoadingScreenSprites(void);   /* skipped in flight levels */
extern void DrawDemoModeOverlay(void);
extern void ComposeFrameScene(void);
extern void SetupFrameOT(void);
extern void DrawActors(void);
extern void RasterizeEmitList(void);
extern void DrawFullscreenTint(int mode, int r, int g, int b);
extern void DrawCinematicLetterbox(void);
extern void DrawMotionTrailRibbons(void);

/* Sets the GTE projection offset — where 3D geometry is centred in screen
   space. Crucially this moves ONLY projected 3D geometry; 2D sprites (the HUD,
   gem counters) are placed by DRAWENV.ofs and are unaffected. That separation
   is what lets us centre each player's 3D view in its half-height strip
   without dragging the HUD off-screen with it.

   Spyro 1 calls this exactly once, at init (ram 0x80012818), with the values
   below — read straight out of the disassembly. Nothing else ever calls it, so
   anything we change here must be restored by us. */
extern void SetGeomOffset(int x, int y);

#define SP1_GEOM_OFX 256   /* from `li a0,256` at 0x80012814 */
#define SP1_GEOM_OFY 120   /* from `li a1,120` at 0x8001281c */

/* ---- input ----
   The per-frame pad handler, installed as the VSync callback by InitPadSystem.
   It reads ONLY port 1's buffer, so calling it again with the two buffers
   swapped makes the game process player 2's input as though it were
   player 1's. */
extern void PollPadAndDistributeInput(void);
extern void InstallVSyncCallback(void (*fn)(void));

/* Raw BIOS pad buffers. 32 bytes is comfortably inside the 34 the BIOS uses,
   and the two are 0x7B0 apart so a swap cannot overlap them. */
extern unsigned char g_abPadPort1Buffer[32];
extern unsigned char g_abPadPort2Buffer[32];
#define SP1_PAD_BUF_WORDS 8

/* Derived per-player input state, 164 bytes at 0x80077378. Verified to hold
   nothing but pad globals. Both polls write it, so it must be swapped out to
   shadow storage between them or the two players overwrite each other. */
extern unsigned char g_abPadDerivedState[0xA4];
#define SP1_PAD_STATE_WORDS 41      /* 0xA4 / 4 */

/* Invalidates the instruction cache. Essential after writing code into RAM —
   the R3000's I-cache does not observe memory writes, so freshly copied
   instructions may otherwise never be seen. Used by the boot stub. */
extern void FlushCache(void);

/* The camera — ONE struct in Spyro 1 (decomps/spyro-1/include/camera.h,
   static_assert sizeof == 0x110). */
extern unsigned char g_Camera[0x110];
#define SP1_CAMERA_SIZE 0x110

/* The game's per-frame camera update: follow, springs, collision. */
extern void UpdateCameraFrame(void);

/* Current level id (spyro-1 name). */
extern int g_LevelId;

/* POINTER to the current level's moby array (spyro-1: extern Moby*). */
extern unsigned char *g_LevelMobys;
#define SP1_MOBY_STRIDE     0x58   /* sizeof(Moby), static_assert'd */
#define SP1_MOBY_STATE_OFF  0x48   /* negative state byte = end of list */
#define SP1_MOBY_WASDRAWN   0x51   /* culling grace + enemy "am I on screen" */
#define SP1_MOBY_POS_OFF    0x0C   /* m_Position, 3 ints — the builder's lw 0xC/0x10 */
#define SP1_MOBY_UPDDIST    0x52   /* m_UpdateDistance; ==0 (with WasDrawn 0) skips */

/* The per-level moby update megafunction (overlay code, via pointer). */
extern void (*g_UpdateMoby)(void);
/* HUD text mobys. GamestateDraw resets this to polyBuf + 0x1C000 every frame
   and the text builders walk it DOWNWARD (`g_HudMobys -= 1`). Our render hook
   reimplements that frame head, so it must reset it too. */
extern void *g_HudMobysPtr;

/* Player 1's Sparx moby. */
extern void *g_Sparx;

/* The overlay's moby factory (function POINTER — overlay code moves). */
extern void *(*g_SpawnMoby)(int pClass, void *pParent);

/* Shared motion-trail ribbon count — see symbols.ld for the lifecycle. */
extern int g_nMotionTrailRibbonCount;

extern void TriggerRespawnOrGameOver(void);
extern void ResetSpyroState(int keepPosition);
extern int  FindFloorBelow(volatile int *pos, int max);
extern int  g_SpyroLifeCount;

/* Returns the height a dragon respawning at height z should stand at, so he
   lands on the floor instead of dropping onto it. Lives in LOADER rather than
   BIOS2 beside its caller, which is full — see Sp1x2Ground.c. */
extern void Sp1x2Ground(volatile int *spyro);

/* Hooked over BOTH `jal TriggerRespawnOrGameOver` sites, so one player dying
   no longer restarts the level for everybody. */
extern void Sp1x2Die(void);

extern void Sp1x2SparxHeal(void);
extern void Sp1x2P2SparxKeep(void);
extern void Sp1x2CaptureSpawn(void);
extern void Sp1x2BootStage2(void);
#define SP1X2_P2_SPARX (*(void * volatile *)0x8000ED10)
/* Last-seen g_Sparx — rebuild detector + dragonfly lifecycle. */
#define SP1X2_SPARX1_SEEN (*(void * volatile *)0x8000ED14)

/* Implemented by us (Sp1x2Spyro.c): the moby update, once per player. */
extern void Sp1x2UpdateMobys(void);

/* ---- co-op view settings: runtime, so the pause menu can write them ---- */
#define SP1X2_SPLIT_MODE     (*(volatile int *)0x8000F180)
#define SP1X2_SPLIT_VERTICAL 1
#define SP1X2_WIDESCREEN     (*(volatile int *)0x8000F188)
#define SP1X2_VIEW_FIT       (*(volatile int *)0x8000F190)

/* How many dragons are in play. Written by the Multiplayer pause menu.
   1 = solo (full screen, no second dragon at all); ANY other value, including
   the 0 that uninitialised BIOS RAM holds at boot, means 2 — so a fresh boot
   falls back to the proven co-op layout rather than to an untested one.
   Room for 3 and 4 later: the render already takes quadrant cases. */
#define SP1X2_PLAYERS        (*(volatile int *)0x8000F198)
#define SP1X2_SOLO           (SP1X2_PLAYERS == 1)
#define SP1X2_FIT_BALANCED   1
#define SP1X2_FIT_CROPPED    2

/* The game's own menu machinery (see symbols.ld). */
extern void PauseMenu_Update(void);
extern void Gamestate02_03_06_Draw(void);
extern void BuildTextSprites(const char *text, void *pos, void *spacing,
                             int size, int colour);
extern void BuildTextSpriteChain(const char *text, void *pos, int size,
                                 int colour);
extern void TickSparkles(int delta);
extern void Sp1x2TickSparkles(int delta);
extern void Sp1x2RegionsLoad(int pass);
extern void Sp1x2RegionsStore(int pass);
extern void Sp1x2SyncMobyFlags(int player);
extern void Sp1x2SyncParticles(int pass);
/* Which render pass is building right now (0 = player 1, 1 = player 2). */
#define SP1X2_RENDER_PASS (*(volatile int *)0x8000F1D8)  /* was 0x8000F1C8,
    which is the Z word of the teleport-detect position at 0x8000F1C0 */
extern void HudPrint(int idx, int len, int value, int mode);
extern void HudTick(void);
extern int  PlaySound(int id, void *moby, int flags, void *out);
extern void SpecularUpdate(int mode);
extern int  g_nPauseMenuIdleFrames;
extern int  g_nPauseMenuSubstate;
extern int  g_nPauseMenuCursor;

extern void Sp1x2PauseUpdate(void);
extern void Sp1x2PauseDraw(const char *text, void *pos, int size, int colour);
extern void Sp1x2MainMenuItem(const char *text, void *pos, void *spacing,
                              int size, int colour);

extern int VecMagnitude(int *vec, int flag);
extern int Sp1x2SoundListenerDistance(int *diff, int flag);

/* Implemented by us (Sp1x2Spyro.c): draws player 2's Spyro in the current
   viewport by swapping his state in around the game's own draw calls. */
extern void Sp1x2DrawPlayer2Spyro(int pass);

/* Hooked over the ONE `jal RasterizePairedActor` in the portal / level-entrance
   sequence draw handler (0x8001a0d8). Draws both dragons side by side. */
extern void Sp1x2DrawPortalSpyro(void);

/* Fills a 3-vector with where the second dragon belongs relative to the LIVE
   one — out along that dragon's own lateral axis. Defined in Sp1x2Pad.c and
   shared by the portal-sequence draw and the level seeding, so the two cannot
   place him on opposite sides. */
extern void Sp1x2FormationOffset(int *out);

/* How far apart the dragons stand, in world units.
   ONE constant for two jobs that must agree:
     - where player 2 is drawn during the portal / entrance sequence
     - where he is actually placed when he is seeded into a level
   If these differ, the dragons visibly jump apart (or together) at the moment
   the sequence ends and the split screen returns.
   Must stay above SP1X2_BODY_RADIUS (0x1A0) or the player-separation push will
   shove them further apart the instant gameplay resumes, reintroducing the
   same jump. */
#define SP1X2_P2_START_OFFSET 0x280

/* Per-(dragon, viewport) flame orientation chains — see Sp1x2Graphics.c. */
/* One walker, direction flag — the MaskWalk space trick again (2026-08-26).
   The old Load/Store pair were twin 80-byte loops. */
extern void Sp1x2FlameChainXfer(int player, int pass, int store);
extern void Sp1x2PadCallback(void);
extern void Sp1x2PadRelease(void);
extern void Sp1x2MaskWalk(unsigned char *base, int n, int owner, int unmask);
#define SP1X2_PAD_HOLD (*(volatile int *)0x8000ED54)  /* see Sp1x2Pad.c */
#define Sp1x2FlameChainLoad(p, s)  Sp1x2FlameChainXfer((p), (s), 0)
#define Sp1x2FlameChainStore(p, s) Sp1x2FlameChainXfer((p), (s), 1)

extern void Sp1x2SwapCameraState(void);   /* no-ops until P2 is seeded */
extern void Sp1x2UpdateCameras(void);

/* ---- Spyro ----
   ComposeFrameScene draws him via these two, guarded by g_nSpyroDrawSuppressed.
   Calling them again with player 2's state swapped in draws a second Spyro. */
extern void RasterizePairedActor(void);
extern void DrawSpyroDropShadow(void);
extern void DrawSpyroFlames(void);   /* r_flame.s; see symbols.ld note */
extern int  g_nSpyroDrawSuppressed;

/* Spyro's per-frame update. Called twice — once per player — by our hook. */
extern void TickSpyroGameplayFrame(void);

/* Swaps the live derived pad state with player 2's shadow (Sp1x2Pad.c), so
   the game's Spyro logic reads player 2's input. */
extern void Sp1x2SwapPadState(void);

/* Interrupt masking. The pad callback runs in the VSync interrupt, so any
   swap of state it also touches MUST be protected or the two players'
   input gets interleaved at random. */
extern void EnterCriticalSection(void);
extern void ExitCriticalSection(void);

/* Spyro's world position, XYZ — the first thing in g_Spyro.
   NOTE (2026-08-31): a block of declarations here described an OLDER swap
   design that copied a 408-byte "core state" with a 4-byte hole at +0x164,
   believing that offset held a foreign global named g_nLevelReadyFlag. It
   does not: the decompilation shows +0x164 is `m_health`, which is exactly
   per-player and must be swapped. The live table swaps all 676 bytes of
   g_Spyro and always did, so nothing was ever wrong in the code — but the
   declarations asserted the opposite and were unused, which is how a wrong
   idea survives. Deleted. */
extern int g_anSpyroWorldPos[3];

/* Frame submit. */
extern void DrawSync(int mode);
extern int VSync(int mode);
extern void *PutDispEnv(void *env);
extern void PutDrawEnv(void *env);
extern void *LinkOTPrimitives(int depth_max);
extern void DrawOTag(void *ot);

/* The two frame environments, 0x84 apart. Layout:
     +0x00 DRAWENV (0x5c)   — of which +0x18 isbg, +0x19/1a/1b = r0/g0/b0
     +0x5c DISPENV (0x14)
     +0x70 prim-buffer write cursor
     +0x74 OT depth-bin array base
     +0x78 active depth slot                                              */
extern unsigned char g_abFrameDrawEnv0[0x5c];
extern unsigned char g_abFrameDrawEnv1[0x5c];
extern void *g_pActiveFrameDrawEnv;

#define SP1_ENV_STRIDE      0x84
#define SP1_ENV_DISPENV     0x5c
#define SP1_ENV_PRIMCURSOR  0x70
#define SP1_ENV_OTBASE      0x74
#define SP1_ENV_OTSLOT      0x78
#define SP1_DRAWENV_ISBG    0x18
#define SP1_DRAWENV_R0      0x19

/* Per-frame buffer cursors, republished from the active env each frame. */
extern int g_nPrimBufferOverflowFlag;
extern void *g_pPrimBufferWriteCursor;
extern void *g_pPrimBufferLimit;
extern void *g_pOtDepthBinArrayBase;
extern void *g_pOtActiveDepthSlot;
extern void *g_pSpriteRecordBufferTop;
extern void *g_pSpriteRecordWriteCursor;

/* Size of the primitive buffer; the sprite-record buffer starts where it
   ends and grows down. From GamestateDraw: limit = cursor + 0x1C000. */
#define SP1_PRIM_BUFFER_SIZE 0x1C000

/* Depth passed to LinkOTPrimitives at frame submit. */
#define SP1_OT_DEPTH 0x800

/* Flags the gameplay branch reads. */
extern unsigned int g_dwWorldFogColor;
extern int g_nFlightLevelActive;
extern int g_nDeathState;
extern int g_nGenericCountdown;
extern int g_nGameplayBlocked;
extern int g_nLetterboxBarHeight;
extern int g_nDeathRespawnPending;
extern int g_nVsyncFrameEndCount;
extern int g_nVsyncFramePaceAnchor;


#endif /* SP1_H_INCLUDED */
