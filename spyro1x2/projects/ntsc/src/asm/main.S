/*
 * The body of SCUS_942.28, reassembled.
 *
 * This is a binary patcher written as assembly. The original executable is
 * pulled in verbatim with .incbin, and hooks are added by BREAKING the incbin
 * into chunks and emitting one replacement instruction in the gap:
 *
 *     .incbin "rom/SCUS_942.28", 0x800, 0x1234    # original bytes
 *     /-* 0x80011a34 *-/ jal Sp1x2Graphics         # ONE instruction replaced
 *     .incbin "rom/SCUS_942.28", 0x1a38, ...      # original bytes resume
 *
 * File offset <-> RAM address:   offset = addr - 0x80010000 + 0x800
 * Each replaced instruction is 4 bytes, so the second .incbin must start
 * 4 bytes past where the first one ended. Get a length wrong and everything
 * after it shifts, corrupting the executable — so add hooks ONE AT A TIME and
 * re-run the byte-identical check between each.
 *
 * Body is 0x65800 bytes (t_size from the PS-EXE header), loaded at 0x80010000.
 * File spans 0x800 .. 0x66000.
 *
 * HOOK LIST (15) — 13 jal redirections + 2 instruction patches:
 *   7. ram 0x8001a980 / file 0xb180 — the `jal BuildTextSpriteChain` that
 *      draws "PAUSED", redirected to Sp1x2PauseDraw. Our wrapper draws the
 *      original string, then appends the Multiplayer menu. Wrapping the whole
 *      pause draw instead was tried and measured: the hook ran but nothing
 *      appeared, because the frame is already composed by the time that
 *      function returns.
 *   8. ram 0x800338b8 / file 0x240b8 — `jal PauseMenu_Update`, redirected to
 *      Sp1x2PauseUpdate: our menu takes over input while it is open, and the
 *      stock update runs normally otherwise.
 *   6. ram 0x80056528 / file 0x46d28 — the `jal VecMagnitude` where
 *      TickActiveSoundVoices measures a 3D voice's distance, redirected to
 *      Sp1x2SoundListenerDistance = min(distance to either camera). Vanilla
 *      kill radius; per-voice two-listener attenuation. (Supersedes a
 *      temporary 4x-radius patch at 0x80056500, reverted — it made ambient
 *      loops audible across half the map.) Delay slot carries `li a1,1`
 *      (the flag argument) and is preserved.
 *
 *   5. ram 0x80033aa4 / file 0x242a4 — the `jalr $2` through g_UpdateMoby in
 *      GamestateUpdate, redirected to Sp1x2UpdateMobys (moby update once per
 *      player, nearest-player partition). Note this one replaces a jalr, not
 *      a jal — same 4 bytes; our wrapper calls through the pointer itself.
 *   1. ram 0x8001227c / file 0x2a7c — the game's only call to GamestateDraw,
 *      redirected to Sp1x2Graphics. Found by searching for the encoded
 *      instruction `jal 0x8001ed5c` = 0x0c007b57.
 *   2. ram 0x80012444 / file 0x2c44 — `jal InstallVSyncCallback` inside
 *      InitPadSystem, redirected to Sp1x2InstallVSyncCallback so we register
 *      our own pad callback (which polls twice, for two players).
 *   3. ram 0x80033ad8 / file 0x242d8 — `jal TickSpyroGameplayFrame` inside
 *      GamestateUpdate, redirected to Sp1x2TickPlayer2Spyro so Spyro's update
 *      runs once per player. NOTE there are 3 call sites for this function;
 *      the other two (0x8002e008, 0x8002f484) are gamestate-specific and must
 *      NOT be hooked.
 *
 *   4. ram 0x80033b4c / file 0x2434c — `jal UpdateCameraFrame` inside
 *      GamestateUpdate, redirected to Sp1x2UpdateCameras: the camera update
 *      runs once per player. Its FOUR other call sites (death/respawn and
 *      special gamestates) are left alone — P2's camera holds there.
 *
 * Every hook replaces an existing jal, so whatever sits in the delay slot
 * executes identically before either target — no constraint there.
 *
 * Arithmetic (every chunk plus 4 per hook must total t_size = 0x65800):
 *   chunk 1 = 0x2a7c - 0x800   = 0x227c
 *   hook 1                     = 0x4
 *   chunk 2 = 0x2c44 - 0x2a80  = 0x1c4
 *   hook 2                     = 0x4
 *   chunk 3 = 0xf634 - 0x2c48   = 0xc9ec
 *   hook 7                     = 0x4
 *   chunk 4 = 0x240b8 - 0xf638  = 0x14a80
 *   hook 8                     = 0x4
 *   chunk 5 = 0x242a4 - 0x240bc = 0x1e8
 *   hook 5                     = 0x4
 *   chunk 4 = 0x242d8 - 0x242a8 = 0x30
 *   hook 3                     = 0x4
 *   chunk 5 = 0x2434c - 0x242dc = 0x70
 *   hook 4                     = 0x4
 *   chunk 6 = 0x66000 - 0x24350 = 0x41cb0
 *   total = 0x227c+4 + 0x1c4+4 + 0x2165c+4 + 0x30+4 + 0x70+4 + 0x41cb0
 *         = 0x65800
 *
 * `.set noreorder` is essential: without it the assembler helpfully inserts
 * its own nop after the jal, which would shift the entire rest of the
 * executable by 4 bytes and corrupt everything.
 *
 * Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
 */

.set noreorder

.incbin "rom/SCUS_942.28", 0x800, 0x227c
/* 0x8001227c */ jal Sp1x2Graphics
.incbin "rom/SCUS_942.28", 0x2a80, 0x1c4
/* 0x80012444 */ jal Sp1x2InstallVSyncCallback
.incbin "rom/SCUS_942.28", 0x2c48, 0x4de0
/* PATCH pair: add -> addu in VecMagnitude, summing the GTE's squared
   components. The signed adds OVERFLOW-TRAP on huge vectors — the decomp-
   documented retail "Baruti crash", which our camera-spasm-on-hit distances
   trigger readily. addu is the decomp team's own recommended fix. */
/* 0x80017228 */ .word 0x00220821
/* 0x8001722c */ .word 0x00230821
.incbin "rom/SCUS_942.28", 0x7a30, 0x2ea8
/* 0x8001a0d8 */ jal Sp1x2DrawPortalSpyro   /* was: jal RasterizePairedActor.
                                       The ONE Spyro draw in the handler for
                                       GS_LevelTransition (1) and
                                       GS_EntranceAnimation (9) — the portal
                                       sequence. Unconditional, nop delay
                                       slot. We call the real renderer and
                                       then draw player 2 beside him. */
.incbin "rom/SCUS_942.28", 0xa8dc, 0x71c
/* 0x8001a7f8 */ .word 0x1060000c   /* PATCH: bne v1,v0 -> beq v1,zero, so the
                                       BIG pause box is drawn for any substate
                                       except 0 (was: only the options list).
                                       Our menu is substate 3. Side effect: the
                                       quit prompt gets the big frame too. */
.incbin "rom/SCUS_942.28", 0xaffc, 0x98
/* 0x8001a894 */ .word 0x10600017   /* PATCH: the same test for the box BORDER
                                       lines — background and border must
                                       agree or the frame is mismatched. */
.incbin "rom/SCUS_942.28", 0xb098, 0xe8
/* 0x8001a980 */ jal Sp1x2PauseDraw     /* replaces the "PAUSED" text call */
.incbin "rom/SCUS_942.28", 0xb184, 0xa6c
/* 0x8001b3f0 */ jal Sp1x2MainMenuItem   /* "CONTINUE" */
.incbin "rom/SCUS_942.28", 0xbbf4, 0x4c
/* 0x8001b440 */ jal Sp1x2MainMenuItem   /* "OPTIONS" */
.incbin "rom/SCUS_942.28", 0xbc44, 0x4c
/* 0x8001b490 */ jal Sp1x2MainMenuItem   /* "INVENTORY" */
.incbin "rom/SCUS_942.28", 0xbc94, 0x5c
/* 0x8001b4f0 */ jal Sp1x2MainMenuItem   /* "QUIT" (flight) — row-4 variant.
   FIRST PLACEMENT WAS OFF BY FOUR: the hook words landed in the two jals'
   DELAY SLOTS, putting a jal inside a jal's delay slot — undefined on MIPS,
   and the PC wandered into the LOADER tail = the level-pause soft-lock,
   diagnosed by register reads. The byte-count gate CANNOT catch a +4
   placement error; always disassemble the built ROM at each new hook. */
.incbin "rom/SCUS_942.28", 0xbcf4, 0x80
/* 0x8001b574 */ jal Sp1x2MainMenuItem   /* "EXIT LEVEL" (levels) */
.incbin "rom/SCUS_942.28", 0xbd78, 0x48
/* 0x8001b5c0 */ jal Sp1x2MainMenuItem   /* "QUIT GAME" */
.incbin "rom/SCUS_942.28", 0xbdc4, 0x182f4
/* 0x800338b8 */ jal Sp1x2PauseUpdate
.incbin "rom/SCUS_942.28", 0x240bc, 0x1e8
/* 0x80033aa4 */ jal Sp1x2UpdateMobys      /* replaces `jalr $2` (g_UpdateMoby) */
.incbin "rom/SCUS_942.28", 0x242a8, 0x30
/* 0x80033ad8 */ jal Sp1x2TickPlayer2Spyro
.incbin "rom/SCUS_942.28", 0x242dc, 0x70
/* 0x80033b4c */ jal Sp1x2UpdateCameras
/* BASELINE 2026-08-27: the Sp1x2CamGate entry patch on UpdateCameraFrame
   (0x80037bd4) is REMOVED. Every camera-state intervention we ever shipped
   lived in it — the spherical clamps, the shake-trio zeroing, the focus
   repair — and each was aimed by reasoning at a bug we had never once
   observed WITHOUT our own interference. One of them (the elevation clamps)
   was proven to generate the very spasm it was meant to contain. The camera
   now runs exactly as retail does. Re-add nothing here without a measurement
   that names the field and shows the game itself writing an impossible
   value. Chunk merged: 0x24350 + 0x4084 + 4 + 0xb338 = 0xF3C0. */
.incbin "rom/SCUS_942.28", 0x24350, 0xf3c0
/* 0x80042f10 */ jal Sp1x2Die   /* was: jal TriggerRespawnOrGameOver, from
                                   inside UpdateSpyroStateBehavior */
.incbin "rom/SCUS_942.28", 0x33714, 0x75c4
/* 0x8004a4d8 */ jal Sp1x2Die   /* the other site, inside the Spyro tick
                                   we already wrap. One player dying now
                                   respawns only him. */
.incbin "rom/SCUS_942.28", 0x3acdc, 0x95c
/* 0x8004ae38 */ j Sp1x2ProbeGate    /* ENTRY PATCH: refuse a segment probe
                                        whose END vector is scrambled. See
                                        src/c/Sp1x2Gates.c. */
.incbin "rom/SCUS_942.28", 0x3b63c, 0x1010
/* 0x8004be4c */ j Sp1x2QueryGate    /* ENTRY PATCH: same for the sphere
                                        query's centre. Restored 2026-08-27
                                        after their removal brought the
                                        ram-hit freeze back. */
.incbin "rom/SCUS_942.28", 0x3c650, 0xa6d8
/* 0x80056528 */ jal Sp1x2SoundListenerDistance
.incbin "rom/SCUS_942.28", 0x46d2c, 0x2694
/* 0x80058bc0 */ jal Sp1x2TickSparkles   /* age particles once per FRAME */
.incbin "rom/SCUS_942.28", 0x493c4, 0x1cc3c
