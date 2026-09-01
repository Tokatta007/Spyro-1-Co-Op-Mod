/*
 * Spyro 1 (NTSC, SCUS_942.28) base-game addresses.
 *
 * Valid for the ROM whose SCUS_942.28 hashes to
 *   84e3728ab94720d0873e2514adf4aade4935e0c5
 * which is what open-spyro targets and what we have. See CLAUDE.md.
 *
 * Names are kept EXACTLY as the upstream reverse-engineering projects use
 * them, so you can grep those repos directly:
 *   _lowerCamel   -> decomps/spyro1-reverse-engineering/symbols/symbols.txt
 *   g_abSomething -> decomps/open-spyro/config/symbol_addrs.txt
 *
 * NOTE (learned from Spyro2x2): do NOT trust address comments in C headers.
 * In Spyro2x2, Sp2.h's comments are PAL addresses while the NTSC project
 * overrides them here. THIS FILE is the single source of truth. We target
 * NTSC only, so there is exactly one address set.
 */


/* ---- the backbone: Spyro + camera ---- */

_spyro                    = 0x80078A58;   /* cf. sp2_spyro   (110 uses) */

_cameraPosition           = 0x80076DF8;   /* cf. sp2_camera  (60 uses).  */
_cameraAngle              = 0x80076E1C;   /* Spyro 1 splits the camera   */
_cameraRotationMatrix     = 0x80076dd0;   /* into separate globals, so   */
g_anWorldToCameraRotMtx   = 0x80076dd0;   /* open-spyro name. THE matrix the
     renderer transforms vertices with. BuildCameraViewMatrix writes the full
     matrix to 0x80076de4, then writes a copy here with the Y row scaled by
     0x140>>9 (= 320/512 = 0.625) for aspect correction. Halving that Y row
     again squashes everything vertically — which is exactly what a
     half-height viewport needs. */
_cameraPureRotationMatrix = 0x80076de4;
g_anCameraPureRotMtx      = 0x80076de4;   /* same address, our name */   /* one Swap becomes several.   */
g_Camera                  = 0x80076dd0;   /* spyro-1: the camera is ONE struct,
     sizeof 0x110 (static_assert, camera.h). Spans 0x80076dd0..0x80076ee0 —
     ends exactly at g_abFrameDrawEnv0. BuildCameraViewMatrix reads its euler
     angles and writes its matrix, all inside the struct. */
UpdateCameraFrame         = 0x80037bd4;   /* per-frame camera update (follow,
     springs, collision). FIVE call sites; only the GamestateUpdate one
     (0x80033b4c) is hooked — P2's camera holds during death/respawn states. */
g_LevelId                 = 0x8007596c;
g_LevelMobys              = 0x80075828;
g_UpdateMoby              = 0x80075734;   /* void(*)() — the per-level moby
     update megafunction pointer. Its GamestateUpdate call is a jalr through
     this (hooked at 0x80033aa4). The megafunction FIRST calls func_80051FEC,
     which builds a null-terminated ACTIVE MOBY pointer list at
     D_8006FCF4+0x400 from distance/visibility vs g_Spyro, then iterates that
     list — the same active-list architecture as Spyro 2's sp2_active_mobys.
     Builder skip rule (from its asm): m_WasDrawn==0 AND m_UpdateDistance==0
     excludes a moby — that is our per-player masking lever. */
g_Sparx                   = 0x80075898;   /* Moby* — player 1's dragonfly */
g_nMotionTrailRibbonCount = 0x80075684;
PauseMenu_Update          = 0x8002e12c;   /* gamestate 2 (GS_PauseMenu) update;
     ONE call site, 0x800338b8. Owns the pause/options/quit substates. */
Gamestate02_03_06_Draw    = 0x8001a40c;   /* draws pause/options/quit. ONE call
     site, 0x8001ee34, inside GamestateDraw and BEFORE its frame submit — so a
     wrapper can append its own text sprites after it. */
g_ParticlesPtr            = 0x80075824;   /* Particle* — spyro-1 loaders.c:
     `g_Particles = m_ParticleSpaceStart; g_Particles[0].m_Type = -1;
      *(int *)(&g_Particles[256]) = -1;`  so: 32-byte entries, 0xFF in
     m_Type (byte 1) terminates, 256 max. Byte 2 is m_Life ("remaining life
     in ticks"), byte 3 is m_03. Identical shape to Spyro 2's particle list,
     which is what makes Sp2x2RenderParticles port directly. */
g_HudMobys                = 0x80075710;
g_HudMobysPtr             = 0x80075710;   /* same, as a writable void* */   /* Moby* — the sprite mobys the
     text routines build into. After BuildTextSprites returns, this points at
     the letters it just made, which is how the stock menu wobbles the
     SELECTED item's letters. */
g_anCosTable              = 0x8006cc78;   /* COSINE_8 lookup, u16 entries */
g_nPauseTextTicks         = 0x800758b8;   /* drives the wobble phase */
g_pSoundTable             = 0x800761d4;   /* menu chimes at +46 and +47 */
PlaySound                 = 0x80055a78;
TickSparkles              = 0x800584c4;   /* called as TickSparkles(g_DeltaTime)
     from func_80058BA8 ("render glows and sparkles", the last call in
     ComposeFrameScene). It ADVANCES particle state by the frame delta as a
     side effect of DRAWING — so our two render passes aged every particle in
     the game twice per frame. Hooked at its one call site, 0x80058bc0. */
RotateSpyroToNeutral      = 0x8003da08;   /* spring-damper that pulls Spyro
     back toward level. Called from the flight attitude controller
     (func_8003DAE4) which runs ONCE PER SUBSTEP, i.e. g_DeltaTime times a
     frame — while steering input is applied once per FRAME. */
SpyroFlightAttitude       = 0x8003dae4;   /* func_8003DAE4, decompiled in
     spyro-1 PR #68. A spring-damper that drives Spyro's PITCH toward the floor
     slope. Called SIX times inside the rotation substep func_8004888C, which
     itself runs g_DeltaTime times per frame — so the spring pulls dt times per
     frame while steering input is applied ONCE. */
TriggerRespawnOrGameOver  = 0x8002c85c;   /* spyro-1 func_8002C85C, commented
     "Die, lose a life and respawn or game over": takes a life if there is one
     and sets GS_Respawn, else GS_GameOver. TWO call sites, both inside Spyro's
     own tick (func_80041670 at 0x80042f10, func_8004A200 at 0x8004a4d8), so
     whichever player is ticking is the one dying. */
FindFloorBelow            = 0x8004d5ec;   /* spyro-1 func_8004D5EC(pos, max),
     documented in collision.h as "looks for the floor below the specified
     position ... returns the floor position". The game's own way of putting
     something on the ground: loaders.c:709 probes from z+1024 when placing
     every level moby, and moby_helpers.c:194 then assigns
     z = floorZ + m_FloorDistance. It also writes the collision globals as a
     side effect, which is why retail follows it with a shadow update. */
ResetSpyroState           = 0x8004ac24;   /* spyro-1 func_8004AC24(keepPos).
     With 1 it backs up position, body rotation and health, MEMSETS the whole
     Spyro struct, restores those three and returns him to state 0 — the
     game's own "put him back on his feet" used by the level reload and every
     cutscene entry. */
g_SpyroLifeCount          = 0x8007582c;
g_Checkpoint              = 0x80077888;   /* CheckpointData, 0x68. +0x50
     m_StartingPosition (Vector3D), +0x5C m_StartingRotation (Z only). The
     level reload places Spyro from these on a respawn, shifting the rotation
     >>4 when a checkpoint was actually stood on (loaders.c:428). */
/* REMOVED 2026-08-28 (CHANGES.md audit): InitFairyCutsceneBody,
   CameraFrameBody and sp1x2_fairy_mute were resume points and a flag for
   gates that no longer exist (the fairy gate was reverted 2026-08-22, the
   camera gate deleted in the 2026-08-27 baseline retreat). sp1x2_fairy_mute
   was the dangerous one: it ALIASED 0x8000ED18, which is the LIVE
   SP1X2_LAST_SEQ. Nothing referenced it, but a stale symbol pointing at live
   data is how the next collision starts. The fairy-gate technique itself is
   documented in CLAUDE.md if it is ever rebuilt: InitFairyCutscene =
   0x8002c924, resume at 0x8002c92c, insn 1 (move a3,a0) is side-effect free
   in the delay slot. */
CollisionProbeBody        = 0x8004ae40;   /* resume point for the second
     gate: func_8004AE38, the SEGMENT probe (a0 = start vec, a1 = end vec)
     used by Spyro's tick — same entry shape, same s-reg save area. */
CollisionQueryBody        = 0x8004be54;   /* resume point for the query
     logger: entry insns 0-1 of func_8004BE4C (lui/addiu $at = the s-reg save
     area D_80077DD8) are replayed inside the gate — the delay slot runs the
     addiu against a stale $at, and the gate's own lui overwrites it. */
D_80077DD8_regsave        = 0x80077dd8;
g_SpawnMoby               = 0x800758cc;   /* Moby *(*)(int class, Moby *parent)
     — POINTER to the overlay's moby factory (spyro-1 overlay_pointers.h).
     Retail spawns Sparx with g_SpawnMoby(120, 0) in LoadLevelScene, skipping
     flight levels. Same class 120 = 0x78 as Spyro 2's create call. */
HudPrint                  = 0x800542e4;   /* HudPrint(idx, len, value, mode)
     — writes a number into the HUD's digit mobys. idx 8 / len 2 is the LIFE
     group (hud.c:138, HudReset's own refresh). mode 1 is the plain print;
     mode 0 is the one the roll animation uses. Address decoded from the jal
     at 0x800311D0 and confirmed against the ROM. */
HudTick                   = 0x80054988;   /* PauseMenu_Update's first call —
     keeps the HUD icons animating while paused. */
SpecularUpdate            = 0x80058cc0;   /* called as SpecularUpdate(3) right
     after; this is what makes "PAUSED" and the HUD shimmer. Skipping the
     stock update froze both. */
g_nPauseMenuIdleFrames    = 0x8007568c;
BuildTextSpriteChain      = 0x80017fe4;   /* (text, &pos, size, col) — the
     call the pause screen uses for the word "PAUSED", so it is known to render
     in gamestate 2. Four args, unlike BuildTextSprites. */
BuildTextSprites          = 0x800181ac;   /* (text, &pos, &spacing, size, col)
     — the game's own menu text call, e.g. ("SOUND EFFECTS", pos, sp, 16, 11) */
g_nPauseMenuSubstate      = 0x800757c8;   /* 0 main, 1 options, 2 quit. Any
     other value draws just the frame + "PAUSED" — a clean canvas for us. */
g_nPauseMenuCursor        = 0x80075720;   /* Motion-trail ribbons (the flame
     breath's trail among them) are a SHARED append-only list: entries live in
     tables at 0x800772c8 and 0x80078658, both indexed from 0, and
     DrawMotionTrailRibbons draws 0..count-1 then ZEROES the count — it
     CONSUMES the list. With two ticks appending to one list, pass 1 drew BOTH
     dragons' ribbons (bend) and pass 2 got none. */
VecMagnitude              = 0x800171fc;   /* |vec| — hooked at ONE call site
     (0x80056528, the 3D-voice distance in TickActiveSoundVoices) so sound
     range/falloff uses min(distance to P1 cam, distance to P2 cam). */   /* Moby* — POINTER to the level's
     moby array. Stride sizeof(Moby)=0x58 (static_assert, moby.h); list ends
     at the first moby whose state byte (+0x48) is negative. m_WasDrawn is
     +0x51 — verified two ways: counting the nine trailing u_chars back from
     the struct end, AND the culling-grace `lb 0x51` in moby_lists.s. */   /* spyro-1 name; open-spyro calls it
     g_nActiveLevelId. Used to detect level changes and reseed player 2. */


/* ---- frame buffers ---- */
/* Spyro 2 lumps DRAWENV+DISPENV into one 116-byte blob (sp2_env_1/_2).
   Spyro 1 names them separately, which is clearer — and makes it much harder
   to repeat the Spyro 2 mistake of moving the draw env while leaving the
   display env behind (see the HMARGIN note in CLAUDE.md). */

g_abFrameDrawEnv0     = 0x80076ee0;   /* 0x5c = 92 bytes, DRAWENV */
g_abFrameDispEnv0     = 0x80076f3c;   /* 0x14 = 20 bytes, DISPENV */
g_abFrameDrawEnv1     = 0x80076f64;
g_abFrameDispEnv1     = 0x80076fc0;
g_pActiveFrameDrawEnv = 0x80075888;   /* cf. sp2_current_env */


/* ---- game state + input ---- */

/* Two names for one address is intentional: C0mposer and open-spyro named
   these independently. We now lean on open-spyro's decompiled
   GamestateDraw.c, so prefer the g_* names in new code. */
_gameState             = 0x800757D8;   /* cf. sp2_game_state */
g_nGamestate           = 0x800757D8;   /* open-spyro name, same address.
                                          0 = gameplay; 1-0xF are menus,
                                          cutscenes, titlescreen, credits. */
g_anCameraPos          = 0x80076DF8;   /* open-spyro name for _cameraPosition */

_currentButtonsHeld    = 0x80077380;   /* cf. sp2_pad        */
_currentButtonsPressed = 0x80077378;
_secondController      = 0x80078E50;   /* Spyro 1 already reads port 2.
                                          Unclear what uses it — worth
                                          investigating for player 2 input. */


/* ---- PSX library (same routines as Spyro 2, different addresses) ---- */

VSync              = 0x8005dbc4;
DrawSync           = 0x8005f764;
DrawOTag           = 0x8005fd64;
PutDrawEnv         = 0x8005fdd8;
PutDispEnv         = 0x80060030;
InitDefaultDrawEnv = 0x8005ea94;   /* cf. SetDefDrawEnv */
InitDefaultDispEnv = 0x8005eb4c;


/* ---- candidate hook points ---- */

GamestateDraw        = 0x8001ed5c;   /* cf. Sp2Graphics — main render entry */
GamestateUpdate      = 0x8003385c;   /* cf. Sp2LogicGameWrapper */
UpdateGameplayCamera = 0x80034480;   /* cf. Sp2LogicCamera */


/* ---- the gameplay render path ----
   Everything GamestateDraw's g_nGamestate == 0 branch touches. We reimplement
   that branch in Sp1x2Graphics because it ends with a whole-frame submit, so
   it cannot simply be called twice. Read
   decomps/open-spyro/src/c/GamestateDraw.c alongside this. */

BuildCameraViewMatrix       = 0x80033c50;   /* camera globals -> view matrix.
                                               Call again after swapping camera
                                               state to render a 2nd viewpoint. */
BuildRenderEntityLists      = 0x800521c0;
EnqueueLoadingScreenSprites = 0x80019300;
DrawDemoModeOverlay         = 0x80018908;
ComposeFrameScene           = 0x80019698;
SetupFrameOT                = 0x8002b9cc;
DrawActors                  = 0x80050bd0;
RasterizeEmitList           = 0x800573c8;
DrawFullscreenTint          = 0x800190d4;
DrawCinematicLetterbox      = 0x80018f30;
DrawMotionTrailRibbons      = 0x800189f0;
LinkOTPrimitives            = 0x80016784;

/* GTE projection offset — where 3D geometry is centred in screen space.
   Moves ONLY projected 3D; 2D sprites (HUD) are positioned by DRAWENV.ofs
   instead, which is why this is the right knob for split-screen.
   Spyro 1 calls it ONCE at init (ram 0x80012818) with (256, 120), and never
   again — so if we change it we must restore it ourselves. */
SetGeomOffset               = 0x80062618;
FlushCache                  = 0x800626e8;   /* used by the boot stub */

/* ---- Spyro himself ----
   ComposeFrameScene draws Spyro with:
       if (g_nSpyroDrawSuppressed == 0) { RasterizePairedActor();
                                          DrawSpyroDropShadow(); }
   Calling those again with player 2's state swapped in draws a SECOND Spyro
   in the same pass — which is exactly how Sp2x2RenderEntities does it. */
RasterizePairedActor   = 0x80023ac4;
DrawSpyroDropShadow    = 0x80059a48;
/* The FLAME renderer. spyro-1's asm/renderers/r_flame.s defines this
   function; open-spyro misnames it DrawSpyroHornStrikeTrails. Guarded by the
   flame-active byte at 0x80078760 (first byte of the flame block we swap) —
   the exact analog of Spyromain's `if (sp2_spyro_flames[152]) Sp2RenderFlames()`.
   One call site, in ComposeFrameScene, which draws the LIVE Spyro only. */
DrawSpyroFlames        = 0x80058d64;
g_nSpyroDrawSuppressed = 0x80075814;

/* Spyro's world position, XYZ — the first field of g_Spyro (0x2a4 bytes),
   all of which the swap table copies. An earlier note here claimed 0x80078bbc
   inside it was a foreign global "g_nLevelReadyFlag" to be swapped around;
   the decompilation shows it is g_Spyro + 0x164 = m_health, which is
   per-player and must travel with him. Nothing acted on the claim. */
g_anSpyroWorldPos      = 0x80078a58;

/* Spyro's per-frame update, called from GamestateUpdate. Running this a
   second time with player 2's Spyro state AND input swapped in is what makes
   the second dragon move. Three call sites exist; only 0x80033ad8 (inside
   GamestateUpdate) is the gameplay one. */
TickSpyroGameplayFrame = 0x8004a200;

/* Interrupt masking. ESSENTIAL around the player-2 state swap: our pad
   callback runs in the VSync INTERRUPT and can fire mid-swap, polling player 1
   into player 2's slot and scrambling both. Spyromain wraps the same swaps in
   these in Sp2x2LogicController. */
EnterCriticalSection   = 0x8005dba4;
ExitCriticalSection    = 0x8005dbb4;

/* ---- input ----
   InitPadSystem (0x800123c8) hands the BIOS these two buffers and polls BOTH
   every frame. Verified by scanning every instruction that references them:
   port 1 has 3 refs (init, 0x8003354c, and 0x80053f00 inside
   PollPadAndDistributeInput); port 2 has exactly ONE — the init. Nothing in
   the game ever reads port 2. So swapping the buffers and calling
   PollPadAndDistributeInput again runs player 2's input through the game's
   whole pipeline. This is Spyromain's trick from Sp2x2VSyncCallback. */
PollPadAndDistributeInput   = 0x80053c68;
InstallVSyncCallback        = 0x8005de58;
g_abPadPort1Buffer          = 0x800786A0;
g_abPadPort2Buffer          = 0x80078E50;   /* = C0mposer's _secondController */

/* The DERIVED per-player input state PollPadAndDistributeInput produces.
   0x80077378..0x8007741c = 164 bytes. Verified to contain ONLY pad symbols:
   g_dwPadPressed, g_dwPadReleased, _currentButtonsHeld (0x80077380),
   g_nPadType, g_nPadStickActiveFlag, g_nPadAllInputIdleFlag,
   g_nPadDirectionalIdleFlag, g_dwPadAnalogRawSnapshot, and
   g_anPadSubstepRing (+96). Both polls write here, which is why running the
   handler twice makes the players fight over one set of variables. */
g_abPadDerivedState         = 0x80077378;

/* Per-frame buffer cursors. GamestateDraw republishes these from the tail of
   whichever frame env is active: +0x70 prim cursor, +0x74 OT base,
   +0x78 active depth slot. The envs are 0x84 apart. */
g_nPrimBufferOverflowFlag  = 0x800758b0;
g_pPrimBufferWriteCursor   = 0x800757b0;
g_pPrimBufferLimit         = 0x80075780;   /* spyro-1 buffers.h calls this
     D_80075780 "something skybox related"; GamestateDraw sets it to
     polyBuf + 0x1C000. */
g_pPolyBufferEnd           = 0x800756fc;   /* spyro-1: D_800756FC, "End of poly
     buffer". GamestateDraw sets it to the SAME value — two separate globals
     for the same limit, and repointing only one of them is what corrupted the
     first attempt at giving pass 2 its own scratch. */
g_pOtDepthBinArrayBase     = 0x80075820;
g_pOtActiveDepthSlot       = 0x8007581c;
g_pSpriteRecordBufferTop   = 0x800756fc;
g_pSpriteRecordWriteCursor = 0x80075710;

/* Frame-state flags the gameplay branch reads. */
g_dwWorldFogColor          = 0x80078a50;
g_nFlightLevelActive       = 0x80075690;
g_nDeathState              = 0x80075714;
g_nGenericCountdown        = 0x80075918;
g_nGameplayBlocked         = 0x8007570c;
g_nLetterboxBarHeight      = 0x800756c0;
g_nDeathRespawnPending     = 0x80075784;
g_nVsyncFrameEndCount      = 0x80075954;
g_nVsyncFramePaceAnchor    = 0x80075950;
