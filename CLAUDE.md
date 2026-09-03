# Spyro Co-op Mod Project

## VERSION CONTROL — created 2026-08-30, and it did not exist before

**There was no git repository for the entire project until now.** That matters
because this file repeatedly says "resurrect from git history" when retiring
code — **that advice was worthless, and anything deleted before 2026-08-30 is
genuinely gone** (the collision gates were rewritten from these notes once
because of exactly this). From this commit on, the advice is real.
`git init` at the project root, first commit = the working state described
below. `.gitignore` excludes ROMs, discs, extracted assets, `reference/`,
`tools/` (a 45 MB emulator installer) and the `Spyro2x2` reference clone,
which has its own upstream. **Commit before any experiment**, especially
before touching the linker script, the memory map, or attempting the decomp
pivot.

## Commit convention (user preference, 2026-08-30)

**Do NOT add a `Co-Authored-By: Claude` trailer to commits.** The user finds
it odd to credit a tool on every commit, and CLAUDE.md already makes the
working method obvious to anyone reading the repository. Commits are authored
as `Tokatta007 <Tokatta007@users.noreply.github.com>` — the local repo config
is set, so this happens automatically. The real name and personal email must
not appear in the history.

## Project layout (renamed and restructured 2026-08-30)

The mod folder is now **`mod/`**, not `spyro1x2/` — the old name only ever
referenced Spyromain's mod and meant nothing on its own. Build outputs are
`spyro1-coop.bin/.cue`, the linker script is `coop.ld`. **The `Sp1x2` prefix
on function names was deliberately NOT renamed**: 71 symbols across every
source file plus the assembly hook list, for zero user-visible benefit and a
real chance of breaking a hook. It is internal.
```
mod/        the mod itself
docs/       research; docs/history/ for superseded investigations
README.md   for anyone cloning: requirements, build, how it works
```
Roms/, reference/, Spyro2x2/ and tools/ stay untracked.

## Where things are written down — THREE files now

- **`CHANGES.md`** — what the mod changes (hooks, memory map, swapped state),
  with confidence markers. Enforced by the build: a hook without a row fails.
- **`BUGS.md`** — the open-work list: active / parked / accepted. Move items
  out of it when they are fixed.
- **`CLAUDE.md`** (this file) — the investigation log: how things were found,
  what was tried and failed, and the rules those failures taught.

## FLIGHT HUD FIXED 2026-09-01 (user: "pretty much perfect") — AND THREE LESSONS

The flight levels' collectible icons and timer now sit inside each viewport:
across the top horizontally, a column down the top-left vertically, timer
top-right. **The first thing the port made possible that v0.1 could not do.**

**NOT FIXED WHERE IT IS DRAWN.** The elements are written by
`func_level_X_8007CFB4` — which is FIVE hand-written assembly functions
(5,043-5,785 lines, similar but not identical) — so editing them would have
been five archaeology jobs. They are ordinary level mobys carrying SCREEN-space
positions, so our render pass shifts them instead, exactly as it already does
the main HUD. Identification is by coordinate range, which is a heuristic, kept
conservative so the failure mode is "the HUD does not move", never "a world
object teleports".

**LESSON 1 — THE SAME BUG THREE TIMES: A RESTORE THAT IS NOT AN INVERSE.**
Going in was `(x >> 1) + delta`; coming out was written `(x << 1) - delta`,
which is a DIFFERENT FUNCTION, so every frame corrupted positions instead of
restoring them and the HUD drifted away permanently. The user's own
observation found it: "the timer disappears if you go vertical and then back
to horizontal" — state destroyed, not state mispositioned. **The cure was to
stop inverting at all: record each moby with the position it had and replay
those values.** Correct by construction whatever the layout does — and it had
to be, because the final column layout HAS no inverse (every icon shares one
X). Prefer save-and-replay over invert-the-maths for anything reversible.

**LESSON 2 — READ THE MAPPING, DO NOT INFER IT FROM SCREENSHOTS.** Two rounds
were spent guessing coordinates. `Sp1x2SetViewport` states it outright: each
pass offsets DRAWENV by a quarter of the split axis, one negative and one
positive, and clips to its own half — so BOTH passes show the same source
window, Y in [56,168] horizontally and X in [128,384] vertically. Every layout
constant after that was arithmetic rather than trial and error.

**LESSON 3 — CHECK THE GEOMETRY BEFORE PROMISING A LAYOUT.** Eight ~36px icons
need 288px; a vertical viewport is 256. They CANNOT go across it at any scale,
which is why the user's own suggestion — a column, as the main HUD already
does — was the answer. Uniform scaling was already at 86% of its theoretical
maximum and could never have looked right.

**AND ONE ORDINARY SLIP:** the column index used the save-table index, which
counts the timer's glyphs too, so the icons began four slots down and ran off
the bottom. The screenshot's arithmetic (y = 120, 144, 168...) named it exactly.

## THE PORT IS DONE AND WORKING — 2026-09-01 (user: "seems to be working just fine")

The whole mod now builds INSIDE the decompilation: 3,630 lines, all 24 hooks,
the intro cutscene playing, the game running. Full detail in
`docs/decomp/README.md`; what follows is what the process taught.

**THE CEILING NOBODY EXPECTED: THE EXECUTABLE CANNOT GROW.** Not "our hooks
are wrong" — a build of VANILLA Spyro carrying 12 KB of PADDING and no mod
code at all breaks the intro cutscene identically. The game fills RAM from
both ends and expands level and cutscene data into the gap between them;
growing the executable shrinks that gap. Nothing hardcodes a low address, so
no amount of relocation helps. **So the mod stays in BIOS scratch, and `.text`
is padded back to retail's length so every later section keeps its address.**
I had claimed that fixing overlays.ld's absolute pins "unblocks growing the
executable". It does not. There were TWO constraints and I had found one.

**THE PROCESS LESSON, and it is the expensive one.** The intro bug took
roughly eight rounds. Every one of the first six was me reasoning about which
of OUR hooks was at fault, and the answer was none of them. What finally
worked was a CONTROL: build vanilla with the same layout shift and nothing
else. That took ten minutes and settled it outright. **When a port breaks
something, first vary the ENVIRONMENT with the payload removed — do not
bisect the payload.** The user had already forced the same correction once
(the 2026-08-27 baseline retreat) and this file already carried the rule
"establish the baseline before bisecting". It was not applied. Again.

**WHAT MEASUREMENT DID AND DID NOT BUY.** Six rounds of user memory reads
(gamestate 14, a valid layout pointer, a climbing tick, 16 actors with 8 alive
and 5 queued for drawing) were all sound and all eliminated real hypotheses —
but they were aimed inside the mod, so they could not find a cause that lay
outside it. Good measurements of the wrong subsystem still cost a day.

**WHAT THE PORT ACTUALLY BOUGHT**, now that space is not on the list:
real symbols instead of 99 raw addresses, direct calls instead of 24 patched
instructions, the entry-patch technique retired entirely, and — the only route
to the flight-HUD bug — the ability to rebuild the level overlays.

**A REAL BUG THE PORT SURFACED IN v0.1:** nothing ever initialised the mod's
BIOS-scratch state. Every flag and setting sat at a fixed address and the mod
relied on that memory happening to be zero at boot. It usually is. That is
luck, not design; the loader now clears it. (Measured innocent of the intro
bug — the flag read 0 — but a genuine latent fault, and it applies to v0.1
too.)

## THE DECOMP BUILDS, AND IT MATCHES — 2026-08-31, ON THIS MACHINE

`reference/spyro-1` was built from source and **all 38 targets match**: PSX.EXE
plus every one of the 37 level overlays, verified by the project's own
`sha256sum.txt`. Decisively:
    build/PSX.EXE  84e3728ab94720d0873e2514adf4aade4935e0c5
    SCUS_942.28    84e3728ab94720d0873e2514adf4aade4935e0c5   (our retail ROM)
**Identical.** So the decompilation reproduces OUR game exactly, and it is a
sound base to build the mod on. This was the MATCHING build (GCC 2.7.2 +
maspsx), NOT MODERN_COMPILER — so the strong gate, not the weak one.

**HOW TO REPRODUCE IT. Four steps, and three of them are not in their README:**
  1. **PSYQ headers** — the repo ships `psyq/` EMPTY. Get
     `http://psx.arthus.net/sdk/Psy-Q/psyq-4.7-converted-full.7z` (1.5 MB, the
     archive every PS1 decomp points at) and copy its `include/` and `lib/`
     into `reference/spyro-1/psyq/`. Needs `brew install sevenzip` (`7zz x`).
     PSYQ headers are the SDK's compile-time `.h` files and have NOTHING to do
     with `Roms/BIOS/SCPH1001.BIN`, which is the console's runtime firmware —
     an easy and natural confusion.
  2. **`git submodule update --init`** — `tools/maspsx` was empty in our clone,
     and the matching build cannot run without it.
  3. **`docker build --platform linux/amd64`** — NOT the bare command in their
     `docker_env.sh`. The matching compiler `tools/gcc2.7.2/cc1` is a 32-bit
     x86 Linux binary; on Apple Silicon an arm64 image cannot run it. Under an
     amd64 image it works (verified: it rejects `--version`, which postdates
     it, but compiles).
  4. `docker run --rm --platform linux/amd64 -v "$(pwd)":/s1 s1_dev_env \
      bash -c 'make -j4 all'` — about 20 seconds of image build, then quick.

**WHAT THE BUILD DOES AND DOES NOT PRODUCE:** `build/PSX.EXE` and
`build/wad/*.ovl`. It does NOT build a disc — our own mkpsxiso pipeline still
does that, and slots straight in.
**WHY MATCHING RATHER THAN MODERN_COMPILER, corrected 2026-08-31.** The
original plan was to chase MODERN_COMPILER for its smaller code. That was
aiming at the wrong thing. What the mod actually needs is to LINK BY SYMBOL
AND LET THE BINARY GROW, which the matching build already permits — matching
is a check that the base is faithful, and our additions simply make it bigger
and stop matching, which is expected. MODERN_COMPILER additionally gives up
the 38-file gate and lets every upstream sync shift code, on a binary that is
still ~60% hand assembly. Note their MODERN_COMPILER path uses `-Os -G 0`,
the same small-data setting our own mod already builds with, so it remains
available later at no conversion cost.
**AND THE USER WAS RIGHT ABOUT UPSTREAM CHURN, where I was not.** I warned
that the decomp's progress would shift addresses under us. It does not: in a
matching build a newly decompiled function compiles to identical bytes BY
DEFINITION, so the output never moves. Their README's warning is about SOURCE
churn — renames and restructuring — which is ordinary merge work.
**HARDWARE, since it came up:** none of this affects console compatibility.
The PS1 has 2 MB of RAM whether emulated or real. Building from source does
not grant more; it lets our code sit in the normal layout instead of squatting
in BIOS scratch — which is in fact SAFER on hardware, because the scratch
region was only ever verified against SCPH1001 and other BIOS revisions lay it
out differently. The real console barrier is SPEED: we draw the scene twice,
the 300% overclock is an emulator feature, and the four-viewport probe
measured exactly half framerate. **Four-player split is an OpenPete goal, not
a PS1 one.**

## A1 CLOSED 2026-08-30 — ENEMIES NEAR PLAYER 2 BEHAVE NORMALLY

User, after a full session: the rams now "attack, then return to position and
wait for spyro to get close", which is vanilla. Previously they got stuck
oscillating between chasing a dragon and walking home. **No fix was aimed at
this** — it was cured by something already shipped, and BUGS.md had named the
likely candidate in advance: **the g_PadBackup swap-offset typo (0x16D8 ->
0x26D8, fixed 2026-08-28), which was zeroing g_Models[216..257] — 42 enemy
model pointers — during every one of player 2's passes.** Not proven for the
ram's specific class, but it is the only change that plausibly touches enemy
behaviour and it predates the improvement.
**THE PROCESS POINT, and it is the good kind:** the entry said "re-test this
first, it may have been cured outright, and it has never been re-tested
against this symptom." That note cost one line to write and saved an
instrumentation round on the project's oldest open bug. **When a fix might
incidentally close another item, SAY SO IN THAT ITEM — a stale bug entry is
as expensive as a missing one.**
Left open by agreement: if it reappears on a DIFFERENT enemy, it is a real
bug again. The rams were the hardest case, which is why they were the test.

## RESPAWN GROUNDING CONFIRMED 2026-08-30 (user: "respawn works... complete")

A respawned dragon now stands where he lands, including on a dragon platform.
**TWO ATTEMPTS, and the first failed on TWO CONSTANTS — both of which the
game's own source states outright, and neither of which I read before
building.**
  1. **Spyro's origin sits 356 UNITS ABOVE THE FLOOR, not on it.**
     checkpoint.c:21 adds exactly that when saving a checkpoint, commented
     "move the starting position up a bit to accommodate for Spyro's
     hitsphere"; pete.c calls him grounded while
     m_Position.z - m_surfaceBelowSpyro is within 512. Placing him AT floor
     level buried him and the collision ejected him — the user saw a bounce
     up and off the homeworld pad.
  2. **The search reach must be 0x10000, Spyro's own figure (pete.c:1487).**
     I used 4096, which is retail's reach for MOBYS. A spawn point captured
     while he was still descending from a level's fly-in sits far beyond
     that, so the probe found nothing and levels were left untouched.
**THE LESSON, and it is a sharp one: a helper's DOC COMMENT describes the
helper, never the CALLER'S CONTRACT.** collision.h says func_8004D5EC "looks
for the floor below the specified position", which is true and was not
enough. Everything I got wrong was in how the game calls it FOR SPYRO
SPECIFICALLY — a different reach and a standing offset — and both were one
grep away in pete.c and checkpoint.c. **Before using a game function, read a
call site that uses it on the SAME ACTOR you are aiming at.** Reading
moby_helpers.c and copying the moby idiom is precisely what produced two
wrong constants.
Diagnostic value worth noting: the two symptoms were ASYMMETRIC (homeworld
bounced, levels unchanged) and each constant explained exactly one of them.
An asymmetric pair of symptoms usually means TWO faults, not one.
STILL OPEN AT SOURCE, logged in CHANGES.md 5g: the arrival capture is taken
on the first gameplay frame, which can be before the fly-in has landed him,
so the stored height is sometimes airborne. Grounding fixes the symptom from
whatever source; gating the capture on m_airTime == 0 would fix the cause.
**SPACE IS NOW THE BINDING CONSTRAINT: LOADER 8 free, BIOS2 16, BIOS2B 4.**
Sp1x2Ground.c exists as its own file ONLY because code is placed per object
file and LOADER had the room BIOS2 did not. The next feature — the per-player
colour menu — needs space FOUND, not shaved.

## DEADLOCK FIX CONFIRMED 2026-08-30 (user: no freeze, controls fine)

The deferred pad poll works. Interrupts stay enabled, the hard freeze is gone,
and input feels normal for both players. **A2 is closed.**

## CAMERA SPASM — one more theory RULED OUT, and the best remaining lead

RULED OUT: "a death or game over leaves the state crossed, and clearing
SP1X2_P2_READY disables the swap that would uncross it". It explained one
session and contradicted the next, and one address read finished it:
**0x8000ED00 reads 1 throughout**, so the swap was never disabled. Cost: one
memory read instead of three builds. Keep doing that.
**CORRECTION 2026-09-02: Sp1x2FixFocus WAS RETIRED on 2026-08-30 and the
counters at 0x8000ED70-ED7C ARE DEAD.** This section still read as though the
repair were live, and its instruction "do not retire it again without a
measurement showing the counter stays zero" sent a later session to read four
addresses that nothing writes. Zero there means THE CODE IS ABSENT, not that
the focus is healthy. **A retirement must be recorded in every section that
tells someone to read the instrument** — the retirement note in
Sp1x2Gates.c was correct and complete, and was never going to be found by
someone following this file.

MEASURED 2026-09-02, and it revises the lead below: g_Spyro + 0x21C is NULL
for BOTH players, not just player 2 (live 0x80078C74 = 0, P2 shadow
0x8000EA1C = 0), in the Peace Keepers homeworld. Every writer of that field in
the whole game is in a level overlay (32/33/34/50/61) and this level is not
one of them, so nothing ever initialises it for anyone. The unchecked pair at
0x80040214 (lw from +0x21C) / 0x80040234 (sw into m_Focus) is confirmed in the
instruction stream, and none of the 17 stores to m_Focus anywhere writes a
literal zero. So the null can only arrive through that one instruction.
STILL UNKNOWN, and the next test: whether that branch ever executes in a level
where the field is null. A breakpoint at 0x80040234 settles it either way.

**THE BEST REMAINING LEAD, and it is grounded in measurement rather than
reasoning.** We know from the focus probe that during a spasm
`g_Camera.m_Focus` is NULL and the camera is dutifully following address zero.
We also know WHERE that null comes from: `func_8003FE40` (inside Spyro's tick)
sets camera mode 0x8000000A together with
    m_Focus = <the pointer at g_Spyro + 0x21C>
and that field is **NULL for player 2**, because he never entered the sequence
that fills it. Repairing the focus either side of the camera update did NOT
help — consistent with the null being installed DURING the update and consumed
before we get to look again.
So the fix to try is not another repair but **stopping the null from being
installed**: give player 2 a valid pointer at `g_Spyro + 0x21C` when he is
seeded (the natural value being `&g_Spyro.m_Position`, which is what the
normal camera mode uses), or refuse mode 0x8000000A for a player whose actor
pointer is null. UNTESTED, and it is attempt ~12 on this bug, so measure that
the field really is null for player 2 before building anything.

## A NEW CRASH CLASS, 2026-08-30: INTERRUPT DEADLOCK, NOT MEMORY CORRUPTION

User CP0 dump at a freeze: **Cause 0x20 (ExcCode 8 = SYSCALL), BadVAddr 0,
EPC 0x8005DBA8**, and — decisively — **the music STOPPED**. Every previous
freeze kept playing (the CD streams in hardware), so this is a different
failure entirely.
Disassembling the EPC identifies it exactly:
    8005dba4  li a0,1 / syscall   = EnterCriticalSection
    8005dbb4  li a0,2 / syscall   = ExitCriticalSection
Music stopping means INTERRUPTS ARE OFF, which is precisely what a critical
section does. So this is a deadlock, not a bad pointer.
**THE STRUCTURAL CAUSE — ours.** We wrap ENTIRE GAME FUNCTIONS in critical
sections: TickSpyroGameplayFrame in Sp1x2TickPlayer2Spyro (Sp1x2Spyro.c
~745-822) and UpdateCameraFrame in Sp1x2UpdateCameras (~848-872). Spyro's
tick can trigger damage, sounds, deaths and level loads; the camera update
runs collision raycasts. **Anything in there that waits on an interrupt — CD,
SPU, VSync, DrawSync — waits forever, because we disabled interrupts.** That
matches the trigger: a ram hit (damage -> sound/state change) right after
unpausing.
WHY THE SECTIONS ARE WIDE: our pad callback runs in the VSync interrupt and
does its own pad swap (Sp1x2PadCallback). If it fires while player 2's state
is swapped in, it polls player 1's input into player 2's slot. The wide
section is deliberate, not careless — see the comment at the tick.
**FIXING IT NEEDS DESIGN, NOT A PATCH.** The obvious move (narrow the
sections to the swaps only) reopens the callback race. A flag telling the
callback to skip while the main loop holds swapped state would keep interrupts
enabled, but the callback fires once per frame and the tick occupies much of
the frame, so naive skipping would drop input often. A DEFERRED poll (callback
records that it wants to poll, main loop performs it after the swaps) is
probably the right shape. Do not rush this at 16 bytes free.
NOTE the collision guards are NOT implicated — this crash never touched bad
memory (BadVAddr 0). Read 0x8000ED60 / 0x8000ED64 at the next freeze to
confirm which class it is before assuming.

## PER-PLAYER SPYRO COLOUR — THE EASY LEVER EXISTS (2026-08-30)

**`g_Spyro.m_colorFilter` at +0x28 (0x80078A80)** — four bytes: red, green,
blue, interpolation (0-255). spyro.h documents it as "interpolated with vertex
colours, used by Fairy Kiss effect". **USER-VERIFIED WORKING** by poking
0x800000FF (red at half strength) live in PCSX-Redux. Credit to the Mod the
Dragon Discord for the pointer — they flagged the Spyro 1 Archipelago mod
using "a filter hex at 0x78a80", which is exactly this field.
**IT IS ESSENTIALLY FREE FOR US: the field is INSIDE g_Spyro, which we already
swap wholesale per player.** Per-player colour needs no new memory — just
different values in each player's state, written during his own draw (the
game clears the field on some state changes, pete.c:1859/1901, so it must be
re-applied each frame rather than set once).
**LIMITATION the user immediately hit:** it is a UNIFORM tint — everything
blends toward one colour, so eyes, horns and belly wash out with the body.
That is what a flash effect is meant to do.
CHEAP MITIGATION, untested: a LOW interpolation (0x30-0x40 rather than 0x80)
should read as a wash rather than a replacement, keeping the internal colour
relationships visible while still making the dragons distinguishable.
PROPER SELECTIVE RECOLOUR — the model uses a PALETTE, which is promising:
`AnimationHeader` carries **`m_NumColors`** and **`m_Colors`**, and each
`AnimationFrame` has an **`m_VertexColorOffset`** that indexes into it. So
recolouring only the purple entries is a palette edit, not a per-vertex walk —
and swapping the POINTER per player would be O(1) per draw rather than a
per-frame transform.
MEASURE FIRST, in PCSX-Redux: read `g_Models[0]` (SPYRO_MODEL), walk to an
AnimationHeader, read `m_NumColors` and dump `m_Colors`. That answers the two
open questions: how big the palette is, and whether the body purple is a small
identifiable set of entries. Note `m_IsSpyroAnimation` implies his model uses a
variant format, so do not assume the generic layout holds.

## PER-PLAYER SPYRO COLOUR — earlier feasibility notes, 2026-08-30

User asked whether the two dragons can be different colours, as a Multiplayer
menu option. Findings:
- **Spyro's model is `g_Models[0]`** (moby.h: `#define SPYRO_MODEL
  (g_Models[0])`). It is a `Model` -> `AnimationHeader[]`, and each
  AnimationHeader carries **`m_Colors`** and **`m_LpColors`** — pointers to
  per-vertex colour data. That is where his purple lives.
- The per-moby colour fields (`m_SpecularMetalColor[3]`,
  `m_SpecularMetalType`) do NOT apply: **Spyro is not a moby.** Those are what
  we already use to shade the pause-menu text.
- r_pete sets no primitive colour of its own (no setRGB in its footprint), so
  there is no cheap global tint to hijack.
THE APPROACH THAT FITS OUR ARCHITECTURE: we already swap player 2's state
around his draw in Sp1x2DrawPlayer2Spyro. Swapping the `m_Colors` pointer
there too would give him a different palette for free — exactly the shape of
the flame-chain and region-table fixes.
THE BLOCKER IS RAM, NOT DIFFICULTY. A second palette has to live somewhere,
and its size is unknown (per-vertex, per-animation — plausibly kilobytes).
BIOS2 is at 16 bytes free and BIOS3 is nearly fully allocated. **Measure the
colour data's size first** (read `g_Models[0]`, walk to an AnimationHeader,
compare `m_Colors` against neighbouring pointers) before designing anything.
A cheaper variant worth considering: transform the palette IN PLACE before
player 2's draw and restore after, like the flame chains — no second copy, but
it costs a walk over the vertex colours twice a frame, which may be too slow.

## SAVE-FAIRY ON RESPAWN — SOLVED 2026-08-30 (user: "works perfectly")

She now appears on the platform but stays silent while you stand there, and
talks normally once you walk away and come back — which is what retail does.
THREE ATTEMPTS, and ALL THREE FAILURES WERE ORDERING OR SCOPE, NEVER THE IDEA:
  1. muted for a fixed 120 frames -> the fairy simply waited it out, and it
     was called from player 1's moby pass only, so player 2 was unaffected;
  2. armed with g_anSpyroWorldPos BEFORE Sp1x2Die moves the dragon, so it
     stored his DEATH position and disarmed instantly;
  3. evaluated in BOTH players' passes, so the far dragon disarmed it.
Each was found by measurement, not by re-reading the logic — and the third
was only findable because the user read a block showing "active = 0 with a
valid position beside it", which says "armed then disarmed" and nothing else.
**Standing rule this earns: for anything per-player, state WHICH player it
belongs to and check that before acting. Three of this project's bugs now
(the teleport detector, the swap guards, this) were code that was correct for
one dragon and silently wrong for the other.**

## SAVE-FAIRY ON RESPAWN — how it was attacked, 2026-08-30

The 2026-08-22 note said "any real fix must start by reading the overlay fairy
moby's code". Done, and the answer was three instructions. The level overlay
(level_10 at 0x800809D4) triggers the fairy only when the player is within
0x200 of the pad AND
    lw   $v0, g_Spyro + 0x80      ; m_idleTimer
    blez $v0, skip                ; not idling -> no trigger
so the prompt needs Spyro SETTLED. Holding m_idleTimer at zero for 120 frames
after an individual respawn fails that guard.
**WHY THIS SUCCEEDS WHERE THE OLD ATTEMPT FAILED, and the general rule:** the
old gate patched InitFairyCutscene's ENTRY. But the overlay writes moby state
6 and freezes the player BEFORE making that call, so muting the call left
player 2 frozen on the pedestal. **Suppress a sequence at its own TRIGGER
CONDITION, never at the function it eventually calls — by then the caller has
already begun setting up, and refusing mid-setup strands it.** This is the
same lesson as the two death-sequence crashes.
THIRD REVISION — MEASURED, and the real one. User read gamestate 0x0B = 11 =
**GS_Fairy** (so the target was right all along) and the mute block as
**active = 0 with a perfectly good respawn position still stored beside it**.
So the arming worked and something disarmed it. That something was
Sp1x2FairyMute itself: it runs ONCE PER MOBY PASS, ONCE PER PLAYER, and the
OTHER dragon is nowhere near the respawn point, so his pass tripped the
distance test and switched the mute off before it could suppress anything.
**That is also why sending the other dragon far away made the bug worse, not
better — a result that looked like evidence against proximity and was really
evidence for it.**
FIX: the mute records WHOSE respawn it is and is evaluated only in that
player's pass. Owner packed into the active flag (0 = off, else player+1) to
save a slot. Sp1x2Die learns whose death it is from SP1X2_TICKING
(0x8000ED50), set around each TickSpyroGameplayFrame call — the live state
alone cannot say, since it is swapped.
PAID FOR with shifts instead of signed divides in Sp1x2SquashView: a signed
/2 and /4 each emit a round-toward-zero correction, and shifting differs by at
most 1 in 4096 (the game's own aspect correction is a shift for the same
reason). BALANCED's 3/4 keeps its divide.
SECOND REVISION, also mine: the mute was armed with
g_anSpyroWorldPos at a point in Sp1x2Die BEFORE that function moves the
dragon, so it recorded his DEATH position. He then respawned far from it, the
distance test tripped on the very first frame, and the mute disarmed itself
before suppressing anything. Now armed from `checkpoint_pos` — whichever
source won (real checkpoint / cached true start / captured arrival) — at the
point the position is actually written.
LESSON, cheap to state and easy to miss: **when capturing "where something
is", check where you are in the function relative to the code that MOVES it.**
Both of this fix's failures were ordering, not logic — the first was calling
it from only one moby pass, the second was reading a position one step too
early.
REVISED ONCE ALREADY on user testing: the 120-frame timer version was wrong twice
over — the fairy simply waited it out, and it was called only from PLAYER 1's
moby pass so player 2 still got the instant prompt. Now DISTANCE-based: quiet
while within 0x800 horizontally of the respawn spot, re-arms on leaving,
called from both passes. That matches retail's own rule, which is positional
— the fairy moby holds a "recently talked" state (byte +0x48 == 2) that
re-arms only when a counter at +0x49 reaches 0x10.
A negative-idle-timer trick was considered and REJECTED: func_8003EA68 (the
state setter) does `sw $zero, g_Spyro + 0x80`, so ANY state change — landing,
moving — wipes it. Confirmed by reading the asm, not assumed.
**BIOS2 IS NOW AT 8 BYTES FREE.** The next addition needs space found first;
the audit's remaining candidates are the shift-for-divide in Sp1x2SquashView
(~52, changes rounding by <=1 LSB, needs a visual check of the view fits) and
the focus-repair counters (~56, once the camera work is truly done).
The user supplied the insight that made this findable: in retail the fairy
goes quiet after you talk to her and only returns when you walk away and come
back, so a condition governing "should she appear" had to exist.

## CONFIRMED WORKING 2026-08-30 (user play-test)

Double death now costs TWO lives; the life HUD appears on death showing the
correct number; the handover fix holds (player 2 can talk to the balloonist
without dragging player 1 to him). User: "very clean and polished".
**THE FODDER/SPARX WORRY IS NOT A BUG:** killing a sheep produces a butterfly
for EITHER player, so the health-restore path works under the moby partition.
The earlier "no sparx and 0 health after exiting a level" report stays
unexplained but unreproduced — and note that "no sparx" and "0 health" are the
SAME state, since sparx IS the health display, so only the health half was
ever a real question.

## DEATH FEEDBACK DONE 2026-08-29 (user-confirmed)

A death now pops the life counter open, holds it, and slides it shut — retail's
own animation, because **the game already opens that group whenever the HUD's
copy of the count differs from the real one** (hud.c:253). We had accidentally
disabled it: our death code must assign the two equal (or the display rolls up
to 99, since the roll animation ONLY INCREMENTS, hud.c:284), and equal counts
mean the group never opens. Nudging the state machine to HDS_Opening was all
it took.
SECOND HALF, and the non-obvious part: the counter then showed the OLD number.
**The digit sprites are only redrawn by the roll animation**, which likewise
runs solely while the counts differ — so they kept whatever was last drawn.
Fixed with HudPrint(8, 2, g_SpyroLifeCount, 1), which is HudReset's own
refresh call (hud.c:138). **HudPrint = 0x800542E4**, decoded from the jal at
0x800311D0 and confirmed against the ROM.
GENERAL LESSON: when reusing a game's own UI machinery, the STATE and the
DRAWN CONTENT are usually refreshed by different code paths. Setting the state
made it appear; only the second call made it correct.

## PLAY-TEST 2026-08-29 — results, and what the g_PadBackup fix really was

Confirmed working: all view fits and both splits, flight-level chests, P1's
death respawning correctly, P1 talking to the balloonist leaving P2 alone.
Still open and unchanged: the ram camera spasm, flight Y-axis, widescreen edge
pop-in, the pedestal fairy dialogue on respawn.
**MY REGRESSION, FIXED SAME DAY:** the options hint read "SQUARE" instead of
"SQUARE  MULTIPLAYER" — I trimmed that string during a space crunch. Restored
now the .rodata move has paid for it.
**WHAT THE g_PadBackup TYPO ACTUALLY DID — bigger than "the pad backup was not
swapped".** The wrong offset 0x16D8 resolved to 0x800766D8, which lands inside
**g_Models (0x80076378-0x80076B78, one model pointer per moby class)**,
covering **g_Models[216..257]**. Because a swap EXCHANGES, and player 2's
shadow starts blank, those 42 model pointers were replaced with **ZEROS**
during every one of his ticks, camera updates and moby passes, then restored
after. Any moby of those classes updating in his pass dereferenced a null
model pointer. Sparx (class 120) is outside the range, so it is not the sparx
bug — but this is a strong candidate for assorted long-standing "enemies
behave oddly near player 2" reports, and it is now fixed.
**NEW CLUSTER TO INVESTIGATE — three symptoms, all asymmetric (player 2
triggers, player 1 suffers):**
  1. P2 talks to the balloonist -> P1 WARPS to P2 when the conversation ends.
     P1 talking to the balloonist leaves P2 where he is. Correct direction
     works, reverse does not.
  2. P2 dies in water -> respawns next to P1 instead of at the spawn point.
     P1 dying in water respawns correctly.
  3. After a dragon rescue, either player's death respawns at the platform and
     auto-triggers the fairy (this one is the known, accepted quirk).
The shared shape is the HANDOVER path: a sequence player 2 triggers leaves his
spyro+camera live, and the resume is supposed to swap identities back so each
keeps his own spot. Symptoms 1 and 2 are both "the swap-back put P1 where P2
was", i.e. the resume is restoring the wrong side. Start at
Sp1x2HandoverResume and the SP1X2_P2_HANDOVER flag.
**HANDOVER CLUSTER — ROOT FOUND AND FIXED 2026-08-29, BY MEASUREMENT.**
The instrument caught it on the balloonist: reseed reason **1 = THE TELEPORT
DETECTOR**, gamestate 0, one reseed, while the death handler showed branch 2
(individual respawn) and zero fall-throughs.
MECHANISM: **the handover swap-back IS a teleport as far as the detector is
concerned.** Its last sample is player 2's position; after the swap the live
dragon is player 1, potentially a level away. Sp1x2HandoverResume runs TWICE
per frame (from the moby hook and the tick hook), so the very next call — same
frame — sees a >0x4000 jump, calls it a level restart, clears P2_READY and
`return`s, which SKIPS THE SWAP-BACK'S OWN PATH and leaves player 2's state
live. That reads on screen as "player 1 warped to player 2".
WHY THE EXISTING EXEMPTION DID NOT SAVE US: the seq != 8/11/12 test is sound
(GS_Balloonist really is 12, confirmed against common.h's enum) but
SP1X2_LAST_SEQ is **CONSUMED by the first jump seen during the sequence**. Any
earlier reposition eats it, and the swap-back's own jump then arrives with
nothing left to exempt it. A one-shot token cannot guard an event that may
fire more than once.
FIRST FIX FAILED (reseeds went 1 -> 2, same reason 1). **The detector fires
and `return`s BEFORE the swap-back runs**, so a re-sample placed after the
swap can never execute. Ordering, not arithmetic.
REAL FIX: **skip teleport detection entirely while SP1X2_P2_HANDOVER is set.**
During a handover the LIVE dragon is player 2 while the sample tracks player
1, so every comparison in that window is a false teleport by construction. A
genuine level change during a sequence is still caught by the
SP1X2_P2_LAST_LEVEL test further down, and the swap-back re-samples so the
next frame compares like with like. This also retires the sequence-number
exemption for this path.
Diagnostic added: 0x8000ED58 now records SP1X2_P2_HANDOVER at the moment of a
reseed, so a repeat failure says immediately whether the gate was even active.
Superseded first attempt: re-sample the detector's `last[]` immediately
after the swap-back, so there is no jump to explain away and the fragile
sequence-number dependency is gone for this path. Same fix Sp1x2Die already
applies to its own respawn teleport — **the general rule is now explicit:
ANY code that moves or exchanges a live dragon must update the teleport
detector's sample at 0x8000F1C0 in the same breath.**
Expected to fix BOTH reported symptoms (balloonist warp AND player 2's water
death respawning beside player 1), since both are the same swap-back path.
Superseded investigation notes follow.
**HANDOVER CLUSTER — investigation so far (2026-08-29):**
RULED OUT: "the water death uses a different code path" (Spyromain's warning
about Spyro 2 having several distinct kill routes). It does NOT here — the
death trigger func_8002C85C (init.c:206, "die, lose a life and respawn or
game over") has EXACTLY TWO callers in the whole executable, 0x80042F10 in
func_80041670 and 0x8004A4D8 in func_8004A200, and **both are already hooked
to Sp1x2Die**. Verified by encoding the jal and scanning all 0x65800 body
bytes. So player 2's water death DOES reach our code.
WHAT THE SYMPTOM MEANS: "respawned right next to P1" is the signature of a
RESEED (which places him beside player 1 via Sp1x2FormationOffset), not of
Sp1x2Die's own placement, which uses the checkpoint / true-start / arrival
capture. So the question is not "where did Sp1x2Die put him" but **"what
cleared SP1X2_P2_READY".** The candidates are the tick's and moby pass's
gamestate 4/5 branches — which only fire if the gamestate DID become
GS_Respawn, i.e. if Sp1x2Die took its fall-through to the stock trigger.
Its fall-through condition is
    SP1X2_SOLO || P2 not ready || *other_health < 0 || g_SpyroLifeCount == 0
and `other_health` reads the SHADOW's health, which is the correct player in
both directions (whoever dies is live, the other is in the shadow).
NEXT STEP, and do NOT skip it: a two-counter instrument recording (a) which
branch Sp1x2Die took and (b) whether a reseed followed, then reproduce a P2
water death. Guessing between "fall-through fired" and "something else
cleared the flag" is exactly the loop that cost ten builds on the camera.
**SPARX/HEALTH AFTER EXITING A LEVEL — needs a discriminating test before any
fix.** Reported: leaving a level left BOTH players with no sparx and zero
health, and killing fodder did not restore it. Note "no sparx" and "0 health"
are the SAME state, not two bugs — sparx IS the health display, so 0 health
correctly shows no dragonfly. The real defect is the second half: **fodder
kills not restoring health.** In retail, killing fodder releases a butterfly
that Sparx eats. Suspect the moby partition: if the butterfly is assigned to
the other player, or masked out of the pass where Sparx lives, it can never be
collected. UNKNOWN whether this is new — do not assume it came from today's
changes without checking.

## SPACE RECOVERED 2026-08-28/29: BIOS2 went from 4 FREE to 336

The audit corrected a measurement error of mine worth knowing: **summing
symbol sizes UNDERSTATES usage** because it misses inter-object alignment
padding. BIOS2 was reported as "128 free" when the true figure — the last
non-zero byte in the payload — was **4**. Measure free space by scanning the
built payload for trailing zeros, not with `nm`.
What worked, all verified by rebuilding and re-checking all 20 hooks:
  +240  move the three .rodata blocks from BIOS2 to BIOS2B (linker only; -G0
        means data is addressed absolutely, so functions are byte-identical)
  +152  -ffunction-sections/-fdata-sections (packs without object padding)
  + 64  unsigned halving in Sp1x2SetViewport — a SIGNED /2 emits a
        round-toward-zero correction at every site; screen dimensions are
        never negative, so it was dead weight
  + 28  one Sp1x2SwapAll() for the four longhand swap triples
**NEAR-MISS WORTH REMEMBERING: my scripted replace rewrote the triple INSIDE
the helper I had just written, so Sp1x2SwapAll called itself.** It looked like
a 752-byte saving — because the three real swap calls had vanished from the
binary — and would have stack-overflowed on the first swap. **The
byte-identical gate CANNOT catch this: it only checks the GAME's body, never
our own code.** Caught by reading every call site after the edit. Do that.
MEASURED AND REJECTED (all cost bytes or gain nothing — -Os already handles
them): env save/restore as loops (+20 worse), fog colour as a loop (0),
merging Sp1x2HudShift's two rect loops (0), merging Sp1x2Die's two loops (0),
index-parameterising Sp1x2SetViewport's branches (worse than the unsigned fix
alone). No dead code exists in BIOS2 — all 27 symbols are reachable.
STILL AVAILABLE if needed: ~52 bytes from shift-instead-of-divide in
Sp1x2SquashView (a signed /4 rounds differently to >>2 — a real if tiny
behaviour change, needs a visual check of the view-fit modes); ~44 from a
redundant Sp1x2HandoverResume() call at the top of the tick (verified
unreachable-by-branch, but gate it on a play-test); ~56 from the
Sp1x2FixFocus counters once the camera work is done. **1024 payload bytes are
already free on disc** (0x2400-0x2800 is pure sector padding), so BIOS2B could
grow with NO header change — but the RAM above it (0x8000E800) holds P2's
Spyro shadow, so that needs a relocation first.

## `make clean` WAS BROKEN — fixed 2026-08-28

`env.mk` has always defined `RM = $(PYTHON) ../../scripts/rm.py`, but that
file did not exist, so **every `make clean` failed**. It errors visibly on the
command line, but is easy to miss when output is redirected — and the symptom
it produced looked like something else entirely: a link failure on stale
objects after editing `Sp1.h` (the Makefile does not track headers, so a clean
is the documented cure, and the cure was silently unavailable). `scripts/rm.py`
is now restored, matching `cp.py`/`mkdir.py` in style. **If a build ever fails
on a symbol that no longer exists, run `make clean` and confirm it actually
prints the rm command.**

## High-RAM survey, 2026-08-28 — RESULT: 0x8000B8B0 IS NOT FREE

Surveyed across main menu, three homeworld visits, two levels and the memory
card screen. **The method validated itself**: the known-live control
(0x8000E000, kernel tables) showed busy data, and the known-clobbered control
(0x8000EC30) changed on every single capture. So the readings are trustworthy.
**THE TARGET IS OCCUPIED.** 0x8000B8B0 holds the ASCII string
`cdrom:\SCUS_942.28;1` — the BIOS's copy of the boot path — and
0x8000B938-0x8000B94B / 0x8000B980+ hold BIOS kernel state (stack pointers
0x801FFF00 / 0x801FFE00, a BIOS ROM address 0xBFC06C5C). The zero-filled
stretches between them are only ~116 and ~52 bytes and are sandwiched between
live data. Not worth the risk; **do not put code at 0x8000B870-0x8000B9A4.**
**0x8000B9A0-0x8000BFFF MEASURED TOO, AND IT IS THE BIOS MEMORY-CARD
DIRECTORY CACHE. NOT FREE — AND IT WOULD HAVE BEEN A NIGHTMARE BUG.** The
save-file name `BASCUS-94228SPYRO` sits in plain ASCII at 0x8000BA92, and the
whole range carries a regular 0x20-byte repeating structure (`a0000000 ...
ffff0000` per slot, 0xFFFFFFFF fill for empty ones) — PS1 memory-card
directory entries. There is also an indexed device table at 0x8000B9B8
(`000000f1 010000f1 020000f1 030000f1 040000f1`). The zeros from ~0x8000BEE0
are simply UNUSED DIRECTORY SLOTS, not free memory: they fill when there are
more saves or a second card.
Code placed there would have run perfectly until the player touched a memory
card, then been silently overwritten — a corruption bug that only reproduces
on save/load, which we would have chased for days. The survey earned its cost
by finding this BEFORE we built on it.
**CONCLUSION: BIOS SCRATCH IS NOW FULLY MAPPED AND THERE IS NO MORE FREE
SPACE THERE.** 0x8000B070-0x8000B870 is ours (LOADER), 0x8000B870-0x8000B9A0
is BIOS boot state (the `cdrom:\SCUS_942.28;1` path + stack pointers),
0x8000B9A0-0x8000BFFF is the memory-card cache, 0x8000C000-0x8000E000 is ours
(BIOS2), 0x8000E000-0x8000E400 is the kernel event/thread tables,
0x8000E400-0x8000E800 is ours (BIOS2B), and 0x8000EC30+ is clobbered on level
loads. Stop looking for free BIOS RAM; the remaining options for space are
(a) the crash demo's ~176 bytes in the executable, (b) trimming our own code,
(c) the level-dependent middle gap, which is a bet on the worst-case level and
is NOT recommended after BIOS2C.
INCIDENTAL CONFIRMATION from the same captures: our menu strings
("MULTIPLAYER", "WIDESCREEN", "SQUARE", "BALANCED"...) are plainly visible at
0x8000DF80-0x8000DFF4, i.e. BIOS2's rodata runs right up to the 0x8000E000
ceiling. That independently confirms both the region's contents and the
ceiling being exactly where the linker says.
ALSO ESTABLISHED (statically, from the decomp): the game fills RAM from both
ends — executable 0x80010000, .bss to ~0x8007AA38, then the LEVEL OVERLAY,
then level model/scene data growing UP; while from 0x80200000 downward sit
the stack, shared animations, particles, the two ordering tables and two
0x1C000 poly buffers (0x3E008 total below m_SharedAnimations, which is itself
0x80200000 - PETE.WAD size - stacksize). The gap between the two is where
level data expands, so its size is LEVEL-DEPENDENT — using it would mean
betting on the worst-case level, which is exactly the BIOS2C mistake.
THE CRASH DEMO, measured: only ~184 bytes of it live in our RAM (the demo is
a separate executable loaded from disc). Its 4.6 MB on disc is irrelevant —
disc space is not our constraint. Removing it is worth ~176 usable bytes,
needs a `jr ra` stub left at each entry because the TITLE SCREEN OVERLAY
calls it (L1+Triangle) and we do not patch overlays.

## Where things are written down

- **`CHANGES.md` is the canonical record of what the mod changes** — every
  hook, every memory allocation, every swapped per-player region, with a
  confidence marker. **Any finalised change must be added there in the same
  pass that makes it.** If it is not in CHANGES.md it is not finished.
  Writing it on 2026-08-28 immediately found a real bug (a wrong offset that
  meant g_PadBackup was never swapped) — that is what the file is for.
- **This file (CLAUDE.md)** is the working log: how things were found, what
  was tried and failed, and the rules those failures taught. Read it for
  WHY; read CHANGES.md for WHAT.

## Who you're working with

The user is **new to coding**. Please:

- Explain commands before running them, in plain language.
- Walk through things step by step. One stage at a time, not a wall of steps.
- Don't assume familiarity with git, make, compilers, or build systems.
- When something fails, explain what the error means before proposing a fix.
- Never dump a long script with no explanation.
- Asking "basic" questions is expected and fine. Answer them properly.
- **Always say which emulator to open a build in, and why.** There are two and
  they are not interchangeable: **DuckStation** for play-testing and "does it
  look right", **PCSX-Redux** for anything needing the memory viewer,
  debugger or exact BIOS behaviour. Never leave it implied.

They have a little terminal experience and are on **macOS**. Use macOS commands
(`shasum`, not `sha1sum`; Homebrew for installs).

## What this project is

A PS1 (PSX) ROM hacking project. The goal is a split-screen co-op mod for
**Spyro the Dragon (Spyro 1)**, using Spyromain's **Spyro2x2** (MIT licensed)
as the reference implementation to study and port from.

Agreed plan, in order:

1. ~~Build Spyro2x2 from source, unmodified, and verify it.~~ **DONE**
2. ~~Study his code; make small modifications to it.~~ **DONE**
3. Port the co-op architecture to Spyro 1. **<- current stage**
4. Spyro 3 last, if ever (see anti-tamper warning below).

**This is NOT the Gex 64 decompilation project.** Different console, different
CPU, different toolchain, different goals. Do not apply N64 decomp conventions,
IDO/decomp.me matching rules, or Gex-specific guidance here.

## Key difference from a decomp project

The goal is **working new code**, not byte-for-byte matching with an original.
We inject and hook code; we do not reproduce original instructions. Readability
wins over instruction count.

## Current state

Update this section as things progress.

- [x] DuckStation installed and working
- [x] Original Spyro 2 (Ripto's Rage NTSC) obtained
- [x] Spyro2x2 confirmed working via the official xdelta patch
- [x] Project folder created, Spyro2x2 repo cloned
- [x] Toolchain installed (mipsel GCC 14.2.0, Python 3.9, mkpsxiso 2.30)
- [x] `env.mk` configured with toolchain paths
- [x] First from-source build completed
- [x] Build compared against official patch (does NOT match SHA-1 — explained
      below; differences fully accounted for and benign)
- [x] **Built ROM tested in DuckStation — boots, loads a level, splits the
      screen. Stage 1 COMPLETE (2026-08-03).**

**Stage 3 progress:**

- [x] Spyro 1 ROM verified against open-spyro's baseline
- [x] Reference repos cloned; six backbone symbols mapped
- [x] ~18 KB of free BIOS RAM confirmed in PCSX-Redux (title screen + in-level)
- [x] `mod/` skeleton builds; **byte-identical rebuild gate PASSES**
- [x] Rebuilt disc boots in DuckStation and plays like retail (2026-08-06)
- [x] **First hook WORKS (2026-08-06).** `Sp1x2Graphics` wraps `GamestateDraw`
      at `0x8000b0f0`. Confirmed in PCSX-Redux: `0xDEADBEEF` at `0x8000C000`
      and a per-frame counter at `0x8000C004` climbing continuously. Game
      plays identically to retail.
      Observed: the counter rises in levels AND during FMV, and pauses during
      CD loading then resumes — so `GamestateDraw` is the universal render
      entry for every game state, and the counter tracks real frames.
- [x] **Camera control PROVEN (2026-08-06).** `Sp1x2Graphics` offsets
      `g_anCameraPos[2]` by `0x800` before calling `GamestateDraw`, then
      restores it. In-game the camera sits far above Spyro. Learned:
      - **axis 2 is vertical (up)**
      - `0x800` = 2048 world units is a LARGE move; a viewport needs far less
      - **the game-state guard works**: pausing restores the normal camera,
        because pause is a non-zero gamestate and takes the pass-through
        branch. Menus and cutscenes unaffected.
- [x] **Phase A WORKS (2026-08-06).** `Sp1x2Graphics` reimplements
      GamestateDraw's gameplay branch — frame-env flip, buffer cursors,
      camera matrix, all five scene calls, VSync pacing, frame submit — and
      the game is indistinguishable from retail. 868/2048 LOADER bytes used
      (1180 free). The VSync `while` rewrite is confirmed equivalent.
- [x] **Phase B1: viewport control (2026-08-06).** Shrinking `DRAWENV.clip.h`
      confines drawing to the top half. Measured real geometry: **512x240
      display, 512x224 draw area**, double buffered (buffer 0 disp y=0/draw
      y=8, buffer 1 disp y=240/draw y=248). `ofs.y` = `clip.y - 8` = disp y.
- [x] **Phase B2: TWO VIEWPORTS WORKING (2026-08-06).** `Sp1x2Graphics`
      renders the scene twice per frame from two camera positions into the top
      and bottom 112 lines. Confirmed on real gameplay. 1104/2048 LOADER bytes.

- [x] **Views SQUASHED, not cropped (2026-08-06).** Each viewport now shows the
      whole scene. Runs at good speed with the 300% overclock.

      **This was 3 lines, not a primitive-list walker.** `BuildCameraViewMatrix`
      already scales row 1 of the view matrix by `0x140>>9` (= 320/512) for
      aspect correction, writing the full matrix to `0x80076de4` and the
      Y-scaled copy to `g_anWorldToCameraRotMtx` (`0x80076dd0`). We just halve
      that row again after each build. Spyromain needed
      `Sp2x2ReducePrimitives` in Spyro 2 because he had no such hook.
      NOTE: must re-apply after EVERY `BuildCameraViewMatrix()` — it rebuilds
      from the Euler angles and wipes the change.

**Known limitations of the current build (all by design):**
- **The HUD is not squashed** — it is 2D sprites in screen space, never
  projected through the view matrix. Needs separate per-player handling.
- **Framerate suffers** — there is a `DrawSync(0)` stall between passes
  because both share the OT at fixed addresses. Removable by giving pass 2 the
  other env's scratch buffer, as Spyromain does.
- Player 2's camera is just player 1's offset upward. No second Spyro, no
  player-2 input yet.
- **HUD (gem counter) is clipped away.** Centring uses `DRAWENV.ofs.y`, which
  moves 2D sprites too. `SetGeomOffset` fixes the HUD but makes actors float
  (the world rasteriser is hand-written asm driving the GTE directly). Full
  analysis in `SPYRO1_PORT_PLAN.md`.

- [x] **Controller 2 WORKS (2026-08-06).** Holding up/down on pad 2 raises and
      lowers player 2's camera; player 1 unaffected.

      **No hook was needed.** `InitPadSystem` (`0x800123c8`) already hands the
      BIOS two pad buffers — `0x800786A0` (port 1) and `0x80078E50` (port 2) —
      and polls both every frame. Spyro 1 simply never reads the second. We
      read the raw buffer directly from our render hook.
      Layout confirmed: standard PSX, ACTIVE LOW.
      `[0]` status (0 = pad present), `[1]` type, `[2]`/`[3]` buttons,
      UP = `0x0010`, DOWN = `0x0040`. NOT Spyro's remapped layout.
      Our scratch state lives at `0x8000C100` (a C `static` would land in an
      orphan `.bss` and be discarded by the linker script).

- [x] **PLAYER 2 INPUT FULLY WORKING (2026-08-07).** Spyromain's trick ported:
      our VSync callback (hook 2, at `0x80012444`) runs the game's own
      `PollPadAndDistributeInput` twice — once per player — swapping both the
      raw port buffers AND the derived state block around each poll.
      Player 1 plays exactly as retail; player 2's fully-processed input
      (calibration, sticks, button history, substep ring) lives at
      `0x8000C300`.

      **The ordering is load-bearing.** The handler computes pressed/released
      by comparing against whatever is in the derived block, so each poll must
      see ITS OWN player's previous state. Getting this wrong (first attempt,
      no state swap) produced edges that were the *difference between the two
      controllers*: controller 1's jump and flame still fired while charge and
      look-around did not.

      Derived state block: `g_abPadDerivedState` = `0x80077378`, **164 bytes**,
      verified to contain only pad globals.

      **GOTCHA — global clocks must not double-tick.** `PollPadAndDistributeInput`
      writes 28 globals, not just the input block. Three of them are real-time
      counters shared by the whole game, NOT per-player:
      `g_nFrameTicks` (`0x80075760`), `g_nVblankTickCount` (`0x800758c8`),
      `g_nCdStallWatchdogTicks` (`0x8007588c`). Calling the handler twice
      advanced them at double rate, so the game thought twice as much time had
      passed and time-integrated movement broke — Spyro walked normally then
      slid onward as if decelerating without stopping (charging was fine, being
      a held state rather than integrated). Fixed by saving/restoring them
      around the second poll.
      Still unprotected, harmless so far because player 2 has no rumble:
      `g_nPadIsDualshockFlag`, `g_nPadSetMainModePending`, `g_nVibrationLevel`,
      `g_nPulseRumbleTimer`, `g_nPulseRumbleAmount`, `g_abPadActCommand`,
      `g_nPadActAlignedFlag`, `g_nHitRumbleTimer`. Suspect these first if
      rumble or pad-mode oddities appear.

- [x] **BIOS2 DELIVERY WORKS (2026-08-07).** Boots, split-screen and both
      controllers unaffected. Code space went from 232 usable bytes to
      **8,344 free** (LOADER 432/1920 + BIOS2 1336/8192). BIOS3 (+7168) still
      holds runtime data only but can be delivered the same way if needed.

      **TRAP THAT COST A BOOT FAILURE: `t_size` must be a multiple of 2048.**
      First attempt used a `0x1F00` payload (Spyromain's BIOS2 size), giving
      `t_size = 0x67700` = 206.875 sectors. The PS1 BIOS loads executables in
      whole 2048-byte sectors and **hangs at the PlayStation logo** rather than
      handling a partial one — no error, just a freeze before anything of ours
      runs. Fixed by padding the payload to `0x2000` (4 sectors), `t_size =
      0x67800` = 207.0. Spyromain never hit this because his code arrived via
      WAD loads, not appended to the executable. **Keep `PAYLOAD_BYTES`,
      the BIOS2 region LENGTH, and t_size in header.S in agreement, and keep
      the total sector-aligned.**
      `src/c/Sp1x2Boot.c`. How it works:
      1. `.bios2` is appended to `SCUS_942.28` at file offset `0x66000`,
         padded to the full `0x1F00` so its size is a compile-time constant
         (header.S must state the total in t_size — a variable size would be
         circular).
      2. `header.S` patches TWO header fields: `t_size` (0x1C)
         `0x65800 -> 0x67700` so the BIOS loads the payload, and `PC` (0x10)
         `0x8005b8e0 -> Sp1x2Boot`.
      3. The BIOS loads the payload to `0x80075800` (t_addr + old t_size).
         That is where `.bss` begins, but NOTHING has run yet — the BIOS loads
         and jumps straight to us — so we copy it out before handing over.
      4. `Sp1x2Boot` copies `0x1F00` bytes to `0x8000C000`, calls
         **`FlushCache` (`0x800626e8`)**, and `jr`s to `0x8005b8e0`.

      **FlushCache is not optional**: we WRITE instructions then EXECUTE them
      later, and the R3000 I-cache does not observe memory writes.

      Result: LOADER dropped from 1756/2048 to **560/2048** (1488 free) with
      `Sp1x2Graphics` moved into BIOS2. Runtime scratch moved BIOS2 -> BIOS3:
      markers `0x8000E400`, P2 camera offset `0x8000E500`, P2 pad state
      `0x8000E600`.

      Gate now checks size == original + `PAYLOAD_BYTES`, and diffs only the
      first `orig` bytes (`cmp -n`).

- [x] **Camera batch built (2026-08-17) — needs testing.** Ports the rest of
      `Sp2x2LogicSpyro`'s swap list + all of `Sp2x2LogicCamera` in one
      camera-domain batch: `g_Camera` (`0x80076dd0`, ONE struct, 0x110 bytes,
      spyro-1 static_assert) is swapped around P2's tick AND around a second
      `UpdateCameraFrame` call (hook 4 at `0x80033b4c`; the other 4 call sites
      hold P2's camera during death/respawn). Render pass 2 now uses P2's REAL
      camera — the fixed-offset placeholder and the up/down camera control are
      gone. P2 reseeds on level change (`g_LevelId`, last-seen at
      `0x8000ED04`; camera shadow at `0x8000EE00`). P2's edge-latch consume
      moved to after his LAST reader (camera, not tick).

- [x] **CAMERA BATCH CONFIRMED + the walk-in-place saga CLOSED (2026-08-17).**
      P2 camera follow works, portals reseed correctly, and the two remaining
      P2 bugs (minimum-height jumps, the walking-in-place idle) were ONE bug:
      **the buffered-input ring off-by-one.** `g_UnprocessedFrames`
      (`0x80075760` — open-spyro misnames it `g_nFrameTicks`) is the write
      index for `g_Pad.m_BufferedInputs[]`, which Spyro's substep loop reads.
      P1's poll increments it before P2's poll ran, so P2's fresh input landed
      one slot late and his FIRST substep every frame read stale data. Fix: restore
      the pre-P1 index before P2's poll, so both players' rings are written at
      the same slot. THE LESSON, again: the fix came from disassembling the
      `slti < 4` ring-bound check, not from name-based reasoning.

- [x] **Batches 3+4 CONFIRMED (2026-08-17).** Moby `m_WasDrawn` synced across
      render passes; moby update runs once per player via nearest-player
      partition (hook 5 over the g_UpdateMoby jalr, masking via
      m_WasDrawn+m_UpdateDistance both 0 = skipped by the active-list
      builder). **Enemies react to P2; P2 collects gems.** Sparx pinned to P1.
- [x] **P2 death: Spyromain-style (2026-08-17).** Either player's death runs
      the game's own sequence untouched — one life spent, BOTH players resume
      at the checkpoint (P2 re-seeds from P1 on the first gameplay frame
      back; ticks 4/5 skip P2, which is what prevented the old re-trigger
      loop). **TWO crash lessons (both user-verified):**
      (1) Cancelling the sequence (reverting g_nGamestate after the trigger)
      crashes — the trigger initialises fade/camera/sequence state beyond the
      gamestate. (2) Letting it run but swapping P2's state back out ALSO
      crashes — the trigger wrote the dying state and death-cam setup into
      the LIVE spyro+camera (P2's, mid-tick), and the sequence then holds
      P1's un-dying state with no setup. Crash appeared exactly when the
      camera swap was added; pre-camera-batch it merely glitched.
      **Working approach: when death fires during P2's tick, leave his
      spyro+camera LIVE** (swap only the pad back) — the sequence keeps the
      state it initialised, the dying dragon plays its own death, and both
      players re-seed from the respawned live set at the checkpoint.
      Desired future refinement (logged, not now): dead player respawns while
      the other keeps playing.

**2026-08-22 — INDIVIDUAL DEATH/RESPAWN: KEPT. LAST-LIFE SURVIVOR MODE:
REMOVED (user decision).** Hooks 17/18 over both `jal TriggerRespawnOrGameOver`
sites (0x80042f10, 0x8004a4d8 — both inside Spyro's tick, so the LIVE dragon is
the dying one). With lives remaining and a partner alive: one shared life
spent, that dragon alone is placed at g_Checkpoint.m_StartingPosition
(rot >>4 only if m_StoodOnCheckpoint), health 3, ResetSpyroState(1), 90
i-frames. Solo / P2 absent / partner already dead / ZERO LIVES -> stock
TriggerRespawnOrGameOver. COSMETIC, accepted 2026-08-22: P2's respawn shows a brief odd view before he
appears. Hypothesis (unverified): retail hides every respawn behind the level
reload's fade-to-black; we skip the reload, so those frames are P2's camera in
transit from death spot to checkpoint. If it ever needs fixing, snap his
camera shadow to the checkpoint in Sp1x2Die instead of letting it fly.
THREE LESSONS THAT COST BUILDS:
  (1) The HUD life counter CAN ONLY COUNT UP (hud.c:284); retail always
      reloads the level on death and HudReset assigns the count outright. Our
      respawn skips the reload, so we assign g_Hud.m_LifeCount (+0x28)
      ourselves or the display rolls to 99 while the real count falls.
  (2) OUR OWN teleport detector (live-Spyro jump > 0x4000 = "level restart",
      reseeds P2 onto P1) fires on our respawn teleport — update its sample
      at 0x8000F1C0 whenever we move a live dragon.
  (3) Health cannot serve as a "down for good" flag: it goes below zero
      during ORDINARY deaths too, before the trigger fires.
The survivor-plays-on mode (park the dead dragon, spectator cam) was built,
mis-attributed which player was down, and was cut rather than debugged —
BIOS2 could not even fit four diagnostic stores at the time.

**NEW 2026-08-22 — 1/2 PLAYER MODE, and it cost almost nothing.**
`SP1X2_PLAYERS` (0x8000F198, 1 = solo, anything else incl. the 0 of
uninitialised BIOS RAM = 2) is the first row of the Multiplayer menu.
ONE guard implements it: Sp1x2DrawPlayer2Spyro is the only place player 2 is
ever seeded, and the tick, camera update, moby partition and portal draw all
return early on SP1X2_P2_READY — so refusing to seed switches the whole mod
off, and switching back re-seeds him beside player 1. Drop-in/drop-out free.
Plus a solo render path: no squash, the game's own clip/ofs untouched, one
pass, second pass and its DrawSync skipped (close to retail framerate).
Widescreen still applies in solo — it is a projection choice, so
Sp1x2SquashView skips only the SPLIT squash.
SPLIT and VIEW draw in shade 12 (grey) when solo to show they are inert.
The shade argument to BuildTextSprites becomes m_SpecularMetalType, an index
into D_8006E44C ("specular shaded color list", 17 entries) because the text
builder memsets the moby into the SHADED path. 11 = the stock gold. 12 = grey
(NOT the red the gem mapping suggested — entries above 12 are unmapped, so
anything else there is trial and error).

**BUILD: CFLAGS IS NOW -Os, NOT -O2 (2026-08-22).** Both code regions were
nearly full (BIOS2 down to 204 bytes). -Os recovered ~1160 bytes in BIOS2 and
~96 in LOADER with no behaviour change — seven times what gutting the pause
menu would have saved (measured: Sp1x2Menu.o is 1656 bytes and has no fat;
only the wobble and chimes are optional, ~170 bytes, and they are what makes
it look native). The code ceiling at 0x8000E000 cannot be raised.
**The Makefile does not track flag changes — `make clean` after editing.**

**PROCESS TRAP, cost a full cycle: `make` STOPS AT THE GATE. It does NOT
repack the disc; `make disc` does.** Check build/rom/SCUS_942.28 against
build/disc/spyro1-coop.bin timestamps before asking anyone to test.

**LESSON 2026-08-23, expensive:** during the space crunch a NULL CHECK was
deleted to save FOUR BYTES on a "provably set" argument — and the proof was
wrong (the game re-nulls g_Sparx during the moby passes, which run between
the heal and the bookkeeping that maintains SEEN, so SEEN can be null).
Result: the heal "adopted" address zero and the spawn arm never ran —
permanent no-sparx, found by user memory reads (all three pointers zero).
NEVER delete safety checks for byte golf; find the bytes structurally
(that is what BIOS2B is for).

**CAMERA STRATOSPHERE — ROOT FOUND BY EXPERIMENT (2026-08-26): IT IS THE
MOBY-OWNERSHIP SPLIT, NOT CAMERA STATE.** Two user experiments closed it:
(1) a PCSX-Redux WRITE watchpoint on m_Focus 0x80076EA0 (condition: Change)
NEVER fired — the focus is never corrupted in this build; the old focus=1
capture was an artifact of an earlier instrumented build. The whole
focus-guard/clamp line of defence was aimed at a ghost. (2) With P2 far away
in a cave, P1's hits produced only a mild local jolt and NO stratosphere —
and the wandering/running-in-place enemies went calm at the same time. Both
symptoms need P2 NEARBY = both live in the moby-ownership split: a moby that
hits a player it does not OWN runs its multi-frame hit reaction (Spyromain's
letter) in the WRONG player's pass — against an un-hurt Spyro and the wrong
camera — and the garbage lands in that camera.
FIX SHIPPED (Sp1x2HitLatch, Sp1x2Spyro.c): when a player's tick ENTERS a
moby-caused hurt state (7 hazard, 25 hurt bounce, 29 charge interrupted —
one masked compare, 25/29 differ only in bit 2), latch {timer=60, player} at
0x8000ED50; while the latch runs, the assign walk force-owns every moby
within 0x1000 (Manhattan, vs the victim's CURRENT position — d1/d2 are
already computed in the loop, no position capture needed) to the victim.
The attacker's reaction then plays out retail-consistent in the victim's
pass. Timer decays unconditionally (negative is harmless, all readers check
> 0).
**CHANGES.md CREATED 2026-08-28 — and the audit that built it FOUND A REAL
BUG on its first pass.** `/CHANGES.md` catalogues every hook (address, old
instruction, new instruction, category, purpose, confidence — read out of the
BUILT ROM vs the original, not from comments), the full memory map with
overlap checking, and the per-player swap list. Read it before adding
anything; update it when you do.
**THE BUG IT FOUND: g_PadBackup was never swapped.** Sp1x2Pad.c's blocks[]
had offset 0x16D8, which from the 0x80075000 base resolves to 0x800766D8 —
but g_PadBackup is at 0x800776D8, so the correct offset is 0x26D8. We were
swapping 164 bytes of the ACTOR MESH TABLE in its place. Symmetric, so
nothing ever crashed, which is exactly why it survived: player 2's pad backup
simply never travelled with him, and mesh pointers carried shadow bytes
through his tick. Verified against spyro-1's game.bss.s AND the instruction
pair at 0x8004A278 (lui 0x8007 / addiu 0x76D8) before changing anything.
FIXED. **This is the argument for the inventory: a wrong constant in a table
of correct-looking constants is invisible to testing and invisible to
review, and only an address-by-address audit finds it.**
ALSO CLEANED: symbols.ld kept resume points and a flag for gates that no
longer exist. One of them, sp1x2_fairy_mute, ALIASED 0x8000ED18 — the LIVE
SP1X2_LAST_SEQ. Nothing referenced it, but a stale symbol pointing at live
data is how the next collision starts. Removed.
DOC DRIFT STILL OUTSTANDING (listed in CHANGES.md, harmless but misleading):
the BIOS3 allocation map in Sp1x2Spyro.c understates its own base and omits
nine live allocations; its moby-table sizing describes 1024 mobys while the
code caps at 512; the table comment says "12 ranges, 1065 bytes" for what is
now 15 ranges / 1092; main.S's header still says "HOOK LIST (15)" for 24;
Sp1x2Graphics.c calls 0x8000C000 "marker RAM" when it is BIOS2 CODE; and
Sp1x2Boot.c/Sp1x2Sparx.c still cite BIOS2B as 0x8000E600/512B (it is
0x8000E400/1024B since 2026-08-27).
**THE STRATOSPHERE, SOLVED 2026-08-28: PLAYER 2'S CAMERA WAS FOLLOWING A
NULL POINTER.** The focus probe (called right after P2's own camera update,
while his camera is live) caught it outright:
    m_Focus (g_Camera+0xD0) = 0x00000000     <- NULL
    camera mode = 0x8000000A   radius = 16,975
    camera Z = 54,150,062      LIVE SPYRO Z = 21,058  (perfectly normal)
The camera was faithfully following ADDRESS ZERO and reading kernel memory as
a position. The camera maths was never wrong; its TARGET was garbage — which
is why every camera-side fix (clamps, shake, spherical state, the four
unswapped globals, the swap guard) missed.
THE ARITHMETIC THAT FORCED THE RIGHT QUESTION, and the transferable trick:
the camera sits at focus + radius*direction, so it can never be further from
its focus than the radius. Radius 16,975 with position 54 million is
IMPOSSIBLE unless the focus itself is at 54 million. Checking an invariant
between two measured numbers pointed at the focus in one step, after days of
theories.
WHERE THE NULL COMES FROM: nothing in the game ever stores zero into m_Focus
(every writer in the binary was checked). But func_8003FE40, inside SPYRO'S
TICK, sets camera mode 0x8000000A together with
    m_Focus = <the pointer at g_Spyro + 0x21C>
That pointer is per-player state inside g_Spyro and is NULL for player 2,
who never entered the sequence that fills it. So the GAME installs a null
focus into whichever camera is live during his tick.
FIX: Sp1x2FixFocus (Sp1x2Gates.c/BIOS2B), called immediately before EACH
UpdateCameraFrame in Sp1x2UpdateCameras. A focus outside RAM or unaligned is
never legitimate — the game's own loaders.c:507 and init.c:335/512 assign
exactly &g_Spyro.m_Position — so it repairs to that. The null is written in
the TICK and consumed by the CAMERA UPDATE, so repairing between them closes
the window. Repairs counted at 0x8000ED70; nonzero confirms it is firing.
NOTE this is the same idea as the 2026-08-23 focus guard that was retired as
"defending nothing" after a watchpoint never fired — the watchpoint simply
never coincided with a reproduction. Do not retire it again without a
measurement showing the counter stays zero across many hits.
**ALSO FOUND AND FIXED 2026-08-28 — A MEMORY-MAP COLLISION IN OUR OWN CODE:**
the retired leak-sweep instrument declared SP1X2_SUMS at **0x8000EE00, the
same address as player 2's camera shadow**, and would have written 1,440
bytes through the camera shadow, the camera extras, the markers AND the pad
state. It was dead code (no call sites, so the compiler dropped it) and
therefore latent, not the cause — but it is exactly the failure this file's
memory map warns about, and it is now deleted. LESSON: delete instruments
when they are retired; a dead one with a live address is a loaded gun.
**THE SWAP-GUARD FIX WAS REAL BUT NOT THIS BUG (user: "still happens,
something feels a bit better"). KEEP IT — the asymmetry was a genuine
invariant violation — but the spasm survives it.**
**THE ARITHMETIC PINS THE ANSWER: THE FOCUS POINT IS IN THE STRATOSPHERE,
NOT THE CAMERA.** From the cleared-probe reading: m_Sphere.radius 16,975 AND
m_Simulation.radius 16,975 (the latter IS |m_Position - *m_Focus|, so camera
and focus really were only ~17k apart) while m_Position.z was 54,150,062.
The camera sits at focus + radius*direction (func_80034204), so it can never
be further from its focus than the radius. A 17k radius CANNOT put the camera
54 million units up unless *m_Focus IS ALSO AT 54 MILLION. So player 2's
camera is not misbehaving — it is correctly following a focus point that is
in the sky. Every camera-side theory (clamps, spherical state, the camera
update's maths) was therefore looking at the wrong end of the equation.
m_Focus (+0xD0) is a POINTER, normally &g_Spyro.m_Position = 0x80078A58.
FOCUS PROBE shipped, called right after P2's UpdateCameraFrame while HIS
camera is still live (the only moment the pointer can be read in context).
It records the pointer, what it dereferences to, the radius, the camera mode
and the live Spyro's Z:
    0x8000ED70 caught  0x8000ED74 m_Focus pointer  0x8000ED78/7C/80 *m_Focus
    0x8000ED84 radius  0x8000ED88 m_State  0x8000ED8C pos.z  0x8000ED90 spyro z
THREE OUTCOMES, THREE DIFFERENT BUGS: pointer = 0x80078A58 with huge values
=> the LIVE SPYRO POSITION is garbage at that instant, i.e. our swap put bad
bytes there. Pointer = something else => the camera was re-aimed (camera mode
6 points m_Focus at the shared anchor D_80077798; sequences point it at
mobys) and that target is bad. Pointer bad/unaligned => the pointer itself is
corrupt (the probe refuses to dereference it, so it cannot fault).
Clear 0x8000ED70..0x8000ED93 before testing.
**SWAP-GUARD FIX 2026-08-27 (kept, real, but not the root): THE TWO SWAP
FUNCTIONS HAD
DIFFERENT GUARDS.**
    Sp1x2SwapSpyroState()   swapped UNCONDITIONALLY
    Sp1x2SwapCameraState()  returns early when SP1X2_P2_READY == 0
Every call site pairs them and assumes they move together (verified: all 11
pairs are adjacent lines). In any window where the ready flag is clear, the
SPYRO swapped and the CAMERA did not — so the live camera and the live Spyro
belonged to DIFFERENT PLAYERS. The camera's follow radius is
|m_Position - *m_Focus| (func_80033F08) with m_Focus pointing at the LIVE
Spyro, so the radius became THE DISTANCE BETWEEN THE TWO DRAGONS and
func_80034204 duly placed the camera that far out.
THE MEASUREMENT THAT NAMED IT (probe v4, cleared by hand first): player 2's
radius first went bad IN SPAN 4, his own camera update — clearing the moby
pass and the tick — at 16,975 with the target radius identical, against a
normal follow distance of ~2,560. 16,975 is not a camera distance; it is a
plausible inter-player separation. Position exploded in the same span to
54 million. Everything fits, including the CAVE TEST of 2026-08-26 (parking
P2 far away stopped it: no separation, no bad radius) and the P1/P2 asymmetry
(P1's update runs first, before most windows where the flag is clear).
FIX: Sp1x2SwapSpyroState now carries the SAME guard. The seed does not go
through it (it calls Sp1x2SpyroTableWalk directly), so nothing else changes.
INVARIANT, worth stating plainly: **either both players' state is exchanged or
neither is. Any future per-player swap helper must share one guard.**
THE PROCESS LESSON, after ~8 failed fixes on this bug: every one of those was
aimed by reasoning at a subsystem (clamps, shake, focus, mode 6, unswapped
globals, moby-driven camera writes, reseeds) and every one missed. The answer
came from a probe that was cleared by hand immediately before reproducing the
fault, watching the DERIVED QUANTITY (radius) rather than the visible symptom
(position). Two earlier probe versions were worthless because they latched
the FIRST event since boot, which is always the level entry.
**MEASURED 2026-08-27 WITH A CLEARED PROBE (the first trustworthy reading):
SPAN 4 — PLAYER 2'S CAMERA POSITION EXPLODES DURING HIS OWN CAMERA UPDATE.**
first span 4, jump 0x2A952944 (714 million), P2 camera Z afterwards
0x033B4517 (54 MILLION), 8 events; most recent also span 4. So this is not a
teleport onto player 1 and not a reseed — the position is genuinely computed
as garbage inside UpdateCameraFrame while player 2's camera/spyro/pad are
swapped in. Player 1's update, running the same code moments earlier, is
fine.
CAVEAT THAT MATTERS: span 4 is where it MANIFESTS. The camera position is
DERIVED from m_Sphere.m_Coords.radius (func_80034204: x = r*cos(el)*cos(az)
etc., then + *m_Focus), so a radius poisoned during the moby pass or the tick
would be invisible to a POSITION probe until the update converts it. Probe v4
therefore also latches the first sample point at which the radius is already
insane (> 0x2000 or negative):
    0x8000ED80  span where the RADIUS was first bad = THE ORIGIN
    0x8000ED84  that radius   0x8000ED88  m_Simulation radius (the target)
    0x8000ED8C  count
READING IT: origin 4 => the camera update itself manufactures the value, so
look inside func_80034480 / func_80035FB4 and at what our swap feeds them.
Origin 2 => the MOBY PASS poisons the target, which would finally vindicate
the "enemy AI writes camera state" finding (func_level_20_8007E3A0 calls
func_80033F08). Origin 3 => the Spyro tick.
CLEAR 0x8000ED70..0x8000ED9F BEFORE EACH TEST — see the retraction below for
why this is not optional.
**RETRACTED — and the retraction is the important lesson.** The
reseed probe measured ONE reseed, at LEVEL ENTRY, and NONE on hits (user:
"those 01s appeared when I warped into the level... no real change when I got
hit"). So reseeds are NOT what moves player 2's camera during a hit, and the
"breakthrough" below was drawn from data that never described the bug.
**ROOT PROCESS ERROR, committed twice: A LATCH THAT RECORDS THE FIRST EVENT
SINCE BOOT RECORDS THE LEVEL ENTRY, NOT THE BUG.** Entering a level seeds
player 2 from player 1 INSIDE THE RENDER, which is a genuine ~140,000-unit
camera move in span 1 — exactly the "span 1 / 140,545 / 8 events" reading
that produced the reseed theory, and probably also probe v1's "both cameras
identical" reading. ANY future probe must either be CLEARED BY HAND
immediately before reproducing the bug, or ignore the first N seconds, or
record the most recent event rather than the first. Probe v3.1 now records
BOTH first and most-recent, and its header says to zero 0x8000ED70..ED8F
before testing.
STILL-VALID FACTS from the retracted round: the seed is the only thing in the
render that writes P2's camera; identical P1/P2 camera values are the seed's
signature; the teleport detector fires on level entry only.
NEXT SUSPECT (untested), which needs NO reseed to explain a shadow jump: the
HANDOVER path. When g_nGamestate != 0 during P2's tick or moby pass we
deliberately leave P2's spyro+camera LIVE and P1's in the SHADOW until the
next frame. Any probe reading "P2's shadow" during such a frame sees P1's
camera — a full inter-player jump — with no reseed at all. If the cleared
probe reports span 3 (the ticks), that is almost certainly what it is.
Original (now unsupported) note follows.
**RETRACTED — THE "CAMERA SPASM" IS NOT A CAMERA BUG. PLAYER 2
IS BEING RESEEDED ONTO PLAYER 1 WHEN SOMEBODY IS HIT.** Stage probe v3
returned: span = 1 (THE RENDER), jump = 0x22501 = **140,545 units in one
frame**, 8 events. A camera does not drift 140k units — it was TELEPORTED.
The only thing in the render that writes P2's camera is the SEED
(Sp1x2InitPlayer2Spyro, reached from Sp1x2DrawPlayer2Spyro), and the seed
copies P1's camera into P2's shadow. That also retro-explains probe v1's
"both cameras hold the identical value 0xDB7" reading, which I misread as
corruption at the time — identical values ARE the seed's signature.
So every camera theory of the last several days (clamps, shake trio, focus,
mode 6, the four unswapped globals, moby-driven camera writes) was aimed at
the wrong subsystem. The camera is following orders correctly; something is
ORDERING A RESEED during ordinary hits.
NOW MEASURING WHICH TRIGGER: six sites clear SP1X2_P2_READY. The prime
suspect by far is the TELEPORT DETECTOR in Sp1x2HandoverResume — the only one
that can fire mid-level with no gamestate change: it reseeds when the LIVE
Spyro's position jumps > 0x4000 in a frame. Note it runs TWICE per frame
(called first from the moby hook AND from the tick hook) and always samples
whatever Spyro is LIVE, so any path that leaves the OTHER player's Spyro live
across one of those samples produces a false "teleport" of the inter-player
distance. Probe shipped (BIOS2 was 4 bytes short, so ONLY site 1 is
instrumented — the others were dropped):
    0x8000ED84  reason 1 = teleport detector fired
    0x8000ED88  reseeds counted
If 0x8000ED88 climbs on hits -> the teleport detector is the bug, and the fix
is to make it sample a FIXED player (or to gate it on the swap state) rather
than "whoever is live". If it stays 0 -> re-instrument the death/handover
sites (codes 3/5/6 in the history of this file).
**2026-08-27: the four-global camera swap DID give P2 the vanilla zoom-back
he never had (user-confirmed) but did NOT fix the spasm. STAGE PROBE v3 is
the live instrument — read it before changing anything.** v3 watches player
2's camera POSITION (a real coordinate, no wrapping) and measures the JUMP
between four samples taken at the ENTRY of each hook, which always executes.
The four samples cut the frame into four spans, and the first oversized jump
names the span:
    0x8000ED70 = 1  the RENDER (previous frame) moved P2's camera
                 2  the MOBY passes moved it
                 3  the SPYRO TICKS moved it
                 4  the CAMERA UPDATES moved it
    0x8000ED74  how far      0x8000ED78  P2 camera Z after the jump
    0x8000ED7C  jump count   0x8000ED90  scratch (previous sample)
This design fixes both flaws that made v1/v2 useless: no absolute threshold
on a wrapping quantity, and no sampling point sitting behind an early return.
PRIOR-PROBABILITY NOTE for whoever reads this next: answer 2 is the one to
expect, because the enemy AI provably writes camera state
(func_level_20_8007E3A0 calls func_800342F8 and func_80033F08). If it comes
back 3 or 4, that fact is a red herring for THIS bug and the swap discipline
in the ticks/camera updates is where to look.
**THE CAMERA IS NOT JUST g_Camera — FOUR GLOBALS WERE NEVER SWAPPED
(2026-08-27).** Found by AUDITING EVERY STORE in the camera module's five
assembly functions instead of reasoning. The complete set of globals camera
code writes:
    g_Camera    139 stores   swapped
    g_Spyro       9 stores   swapped
    D_8007AA10    1 store    swapped (SpyroShadow)
    D_80075938    4 stores   *** NOT SWAPPED ***
    D_80075894    4 stores   *** NOT SWAPPED ***
    D_80075924    2 stores   *** NOT SWAPPED *** (L2/R2 rotate speed)
    D_800756B8    2 stores   *** NOT SWAPPED *** ("camera was FORCED TO ITS
                                                   DESTINATION" flag)
That is a CLOSED list, not a guess. All four are file-scope statics in
camera.c reached through $gp — **which is why the earlier %hi-based footprint
scan missed them completely. ALWAYS scan for %gp_rel AS WELL AS %hi when
taking a function's global footprint; small globals live in the gp window.**
They now swap with the camera (shadow at 0x8000EF10, 4 words) and are seeded
with it.
WHY THIS FITS THE SYMPTOM the user reported (and nothing else did): P1's
camera SOMETIMES does the correct vanilla zoom-back on a hit, but **P2's
NEVER does** — a systematic asymmetry that only unswapped shared state
explains. D_800756B8 is the clearest: CameraForceToDestination sets it, that
runs on the hit path (camera mode 6 via D_8006C588[state 29]), so a hit on
either player leaves the OTHER player's camera update believing it was just
teleported.
SAME SHAPE AS EVERY EARLIER BUG OF THIS FAMILY — the walking-in-place idle,
the endless jump/glide loop and the mis-aimed flame were all "a global we
forgot to swap", each fixed by completing the list. This is the camera's
equivalent, and it was found only after the baseline retreat cleared our own
interference out of the way.
Camera functions still in ASM (checked, per user request): func_80033F08,
func_80034480, func_80034CE8, func_800357A4, func_80035FB4 — five, all in
asm/nonmatchings/camera/. Their stores are the audit above, so being
undecompiled did not hide anything from this analysis.
**BASELINE BUILD 2026-08-27 (user's call, and it was the right one): EVERY
CAMERA INTERVENTION REMOVED.** The Sp1x2CamGate entry patch on
UpdateCameraFrame is GONE from main.S (hooks 25 -> 24; the camera function's
first four words now match the original ROM byte for byte, verified), and
with it every clamp, the shake-trio zeroing and the focus repair. The hit
latch / ownership freeze is removed too. KEPT: the two collision entry gates,
because they are what stopped the freezes and they only ever act on
coordinates that are already impossible.
WHY: we had layered five interventions on a bug we had NEVER ONCE OBSERVED
without our own interference — and one of them (the elevation clamps) was
proven by the user's probe to be generating the symptom. Our own notes
already carried this rule from the flame-bend saga ("establish the baseline
before bisecting"); it was not applied here, and that cost roughly six build
cycles. The user identified this before I did.
WHAT THIS BUILD IS FOR: characterising the RAW bug. Retail-identical camera
code, two swapped cameras, nothing else. Whatever it does now is the real
behaviour, and any future fix must be aimed at a measured field with the game
itself shown writing an impossible value.
NOTE the still-true READ FACTS that survive the retreat (they are
observations, not guesses): enemy AI writes camera state directly
(func_level_20_8007E3A0 -> func_800342F8 / func_80033F08, seven overlays do
it); elevation is a WRAPPING angle; the shake trio is dead code; m_Focus is
never corrupted; the seed copies P1's camera into P2's shadow, so identical
P1/P2 camera values mean a reseed happened.
**2026-08-27, THE PROBE'S FIRST READING — AND IT CONVICTED MY OWN CLAMPS.**
User read 0x8000ED70 after a spasm: stage 4, P2 value 0xDB7, P1 value 0xDB7
(IDENTICAL), 83 frames. Two conclusions, one of them a genuine own-goal:
**(1) NEVER APPLY A MAGNITUDE CLAMP TO A WRAPPING ANGLE. The elevation
clamps were GENERATING the spasm.** m_Simulation/m_Sphere elevation is an
angle in 0..0xFFF: a camera below its focus looking up reads ~0xF00, and the
probe caught BOTH cameras legitimately holding 0xDB7 (= -585, ~51 degrees
below) for 83 consecutive frames. Our gate snapped anything >= 0x380 to 0xA0
EVERY FRAME, so ordinary play was being violently re-aimed — which is exactly
why adding the elevation clamps made it "maybe worse than before". BOTH
elevation clamps are now DELETED. The radius clamps stay: radius is a true
magnitude and 0x809C (32,924) really is impossible.
**(2) The identical P1/P2 values mean the frame sampled was a RESEED** (the
seed copies P1's camera into P2's shadow, Sp1x2InitPlayer2Spyro), and "stage
4" only meant the other three probes had been SKIPPED — stages 1-3 sit after
early returns that bail while P2 is not ready. Probe design lesson: a staged
probe must record WHICH STAGES RAN, or a skipped stage reads as a late
failure. It now watches m_Sphere.radius (unambiguous) and records a
stages-run bitmask at 0x8000ED80.
STILL UNKNOWN after this: whether any real cross-player corruption remains
once the self-inflicted clamping is gone. Re-test before theorising further.
**OWNERSHIP FREEZE DID NOT FIX THE SPASM (user, 2026-08-27: "looks pretty
similar as before"). STAGE PROBE SHIPPED — read it before changing anything
else.** Three consecutive fixes aimed by reasoning have missed, so the next
move is MEASUREMENT, exactly as the flame-bend bug was settled after nine
wrong guesses. Sp1x2Diag (Sp1x2Gates.c) is called at the end of each of the
four things we run twice per frame and LATCHES the first one to leave P2's
camera poisoned:
   0x8000ED70  first bad stage: 1 moby pass / 2 Spyro tick /
                                3 camera update / 4 render.  0 = never
   0x8000ED74  the offending elevation
   0x8000ED78  P1's live elevation at that moment
   0x8000ED7C  frames bad since (spasm duration)
Watched value: P2 SHADOW m_Simulation.m_Coords.elevation (0x8000EE94), the
field that drives look pitch via func_800342F8; > 0x300 is impossible
legitimately (0x300 is the steepest preset in the game's own D_8006CAB4).
WHAT EACH ANSWER MEANS: 1 = a moby writes P2's camera (the multi-frame
reaction theory, and the freeze is insufficient — go to the D_80077798 /
camera-mode-6 lead below); 2 = Spyro's tick; 3 = the camera update itself,
i.e. P2's camera legitimately computing a bad value from state we have not
made per-player; 4 = the RENDER, which would be the swap-camera-but-not-
Spyro mismatch noted below (m_Focus reads P1 during P2's pass).
Note the theory below is still UNFALSIFIED as the mechanism for the WANDERING
ENEMIES — only its role in the camera spasm is in doubt.
**ROOT CAUSE FOUND BY READING THE DECOMP, 2026-08-27 — SPYRO 1'S ENEMY AI
DRIVES THE CAMERA ITSELF.** After the user (rightly) called a halt to
trial-and-error clamping, camera.c + the level-20 asm gave the answer in
under an hour. THE FACTS:
  - `func_level_20_8007E3A0` (the Peace Keepers moby megafunction — the RAM's
    own code) calls `func_800342F8` (writes g_Camera.m_Rotation) and
    `func_80033F08` (rewrites the spherical coords): FOUR camera calls in
    that one function. SEVEN level overlays do the same (10/20/30/31/40/50/60).
  - Spyro 2 triggers a hit in ONE frame and lets Spyro's own variables play
    it out. Spyromain PREDICTED this difference in his 2026-08-25 letter
    before we found it.
  - We run the moby update TWICE per frame with a different camera swapped
    in. A moby always hits its OWNER (it only sees the Spyro live during its
    own pass), so frame 1 is self-consistent. The damage is done when
    OWNERSHIP FLIPS MID-REACTION: the rest of P1's hit reaction executes in
    P2's pass and writes P1's reaction into P2'S CAMERA.
  - Explains ALL of it: P2's camera flying on P1's hit; the inconsistency
    (does a flip land inside a reaction?); and the user's CAVE TEST, where
    parking P2 far away stopped the spasms AND the strange enemy behaviour
    together, because a distant P2 never takes ownership.
FIX: the hit latch now FREEZES the owner table for 90 frames on any damage
state (7, 22-31 — from HandleSpyroDamage's own branches, pete.c 1050-1080;
the old list checked 7/25/29 and would miss 27/28). The moby that starts a
reaction finishes it in the same pass against the same camera. The previous
"force nearby mobys onto the victim" version was at best a no-op (a moby
already belongs to whoever it hit) and at worst harmful, since forcing flips
is the very thing that causes this bug.
**TWO MORE FACTS FROM THE FULL DAMAGE-PATH TRACE (2026-08-27), both
actionable:**
(a) **THE CLAMP THRESHOLD WAS BREAKING NORMAL PLAY.** func_80034480 has a
    legitimate steep-camera fallback: when the camera stays blocked by
    geometry for 31+ frames (m_SpyroOffCenterFrames +0xC8 >= 31) it walks
    FIVE fallback presets at D_8006CAB4, two of them steep — elevation 0x200
    and 0x300 — snapping the camera with func_80034358. Our elevation clamp
    tripped at 0x2C0, BELOW the game's own 0x300 preset, so we were clamping
    retail behaviour. Raised to 0x380 (above every legitimate preset, below
    the measured spasm 0x3AB). LESSON: never choose a clamp threshold without
    checking what the game's own DATA TABLES consider legal.
(b) **NEXT SUSPECT IF THE OWNERSHIP FREEZE IS NOT ENOUGH: camera mode 6 and
    D_80077798.** Spyro state 29 (charge interrupted — what a charging RAM
    inflicts) maps through D_8006C588 to camera mode 6, and mode 6
    (func_80035FB4 @0x800368EC) is the FROZEN-FOCUS camera: it snapshots the
    current focus into the STATIC vector D_80077798 and points m_Focus there.
    That address is our known shared "player anchor" — NOT inside g_Camera,
    NOT swapped, and our tick deliberately restores P1's value into it after
    P2's tick (so Sparx follows P1). So a player 2 camera in mode 6 focuses
    on PLAYER 1'S POSITION. If spasms persist, make D_80077798 per-player
    (swap its 12 bytes with the camera) and re-check the Sparx anchor
    behaviour that the current restore exists for.
Confirmed NOT the cause: func_8003EA68 (the state setter) writes ZERO bytes
of camera state — its only g_Camera access is a READ of m_Rotation.z for
charge lock-on. The damage path reaches the camera ONLY via
D_8006C588[g_Spyro.m_State] on the NEXT frame. Also confirmed: the screenshake
trio (D_800756DC/D_80075848/D_8007590C) is never written non-zero ANYWHERE in
the game — genuinely dead code, so our zeroing of it is harmless but was
never a fix. And the vibration globals (D_80075904/D_80075764/D_800757D0/
D_8007584C) that damage DOES write are rumble, not camera.
**STRUCT FACTS worth keeping (include/camera.h — READ IT, the empirical
offsets in these notes were incomplete):** the camera's LOOK direction is
`m_Rotation` (+0x4C, three SHORTS), built by func_800342F8 as
  x = m_Sphere.m_Offset.azimuth
  y = m_Simulation.m_Coords.elevation + m_Sphere.m_Offset.elevation
  z = 0x800 - m_Simulation.m_Coords.azimuth - m_Sphere.m_Offset.radius
so the "on the ground but LOOKING at the sky" symptom is the m_Offset
fields, which the clamps never touched. Each of m_LastSimulation (+0x60),
m_Sphere (+0x78), m_Simulation (+0x90) and unk_0xA8 (+0xA8) is a
SphericalCoordsOffset = TWO coord triples (m_Coords then m_Offset), and the
"offset" triple is a second set of ANGLES (m_Offset.radius is used as an
angle). The clamps cover 4 of ~24 spherical fields — they are containment
only, which is why every clamp round merely relocated the symptom.
ALSO NOTED, latent: our RENDER swaps the camera but NOT g_Spyro, so during
P2's render pass m_Focus (-> the live Spyro slot) reads P1. Harmless today —
no renderer calls any camera-state function (checked) — but it is exactly
the shape that would compute radius = INTER-PLAYER DISTANCE, and the measured
spasm radius 0x809C (32,924) is an inter-player distance. Revisit if a
renderer ever gains a camera call.
Red herrings cleared on the way: D_80075914 (22 refs in the camera module) is
the save-file camera-speed SETTING, not per-hit state; D_8007592C is "is
Spyro in look mode", recomputed per update from the live Spyro.
**FREEZES CLOSED 2026-08-27 (user: "No crashes").** The restored collision
gates hold. Remaining work on this item is the VISUAL spasm only.
**ELEVATION IS WHAT MAKES IT FLY — clamp all FOUR spherical fields, not two.**
After the guards went back the user reported no crashes but a spasm "maybe
worse than before", with a decisive asymmetry: the VICTIM's camera is mild,
the OTHER player's is violent. Cause of the regression was mine: restoring
only the RADIUS clamps after the crash build left elevation loose, reasoning
elevation cannot produce the huge coordinates that freeze — true for the
freeze, false for the visual, since elevation is exactly what puts the camera
overhead (measured spasm: elevation 939 ~= 82 degrees up vs ~170 calm). The
CamGate now clamps m_Sphere AND m_Simulation, radius (>= 0x2000 -> 0xA00) AND
elevation (>= 0x2C0 -> 0xA0). Clamping the sphere alone is not enough either:
the spring re-drives it toward a poisoned TARGET within the same update.
**Sp1x2CamGate MOVED to Sp1x2Gates.c / BIOS2B** (all three entry gates now
live together; BIOS2 had no room). It touches $v0/$at/$t0-$t2 and never $v1 —
verified by grepping the built disassembly, since a $v1 clobber here is what
froze two builds.
**COLLISION GUARDS RESTORED 2026-08-27 — AND THE LESSON: NEVER RETIRE A
MEASURED-WORKING GUARD ON A THEORY.** Both entry gates (Sp1x2QueryGate on
func_8004BE4C, Sp1x2ProbeGate on func_8004AE38 — refuse any position with a
coordinate >= 0x400000 or negative, return "no collision", count refusals)
were built 2026-08-23, MEASURED holding (50, then 168/2337 refusals, no
faults), then RETIRED during the BIOS2C retreat because the m_Focus repair
was believed to fix things at the source. The watchpoint later proved the
focus is never corrupted, so that repair defended nothing — and the ram-hit
freeze returned. Source had been deleted; rewritten from these notes.
Counters MOVED: query 0x8000ED60, probe 0x8000ED64 (the old 0x8000E4C8/E4EC
block is now code). Read them after a session — each tick is one averted
freeze.
SPACE CAME FROM A STRUCTURAL WIN, not byte golf: **BIOS2B grew DOWN from
0x8000E600/0x200 to 0x8000E400/0x400** (+512 bytes of code). 0x8000E400-E5FF
held only long-retired diagnostic counters and has been read reliably for
months, so it is not clobbered. THIS IS NOT THE BIOS2C MISTAKE — that was
0x8000EC30, which the game DOES clobber during level loads. The payload
already carried 0x400 of slack, so t_size, PAYLOAD_BYTES and the sector
padding are ALL unchanged; only Sp1x2BootStage2's copy length and the ld
region moved. BIOS3 now starts at 0x8000E800.
Both new hooks were verified by disassembling the built ROM (jump target,
delay slot = the original insn 1, resume word = `sw s0,0(at)`) per the
standing rule.
**TWO CRASH BUILDS 2026-08-26 — CAUSE WAS AN ENTRY-GATE REGISTER BUG OF MY
OWN MAKING, NOT THE CLAMPS AND NOT THE LATCH.** Freezes on the first ram hit
(screen frozen, music playing). First diagnosis blamed the clamps I had
stripped; restoring them did NOT help, which is what exposed the real fault.
MECHANISM: the entry patch at 0x80037bd4 replaces insn 0 (`addiu sp,-24`)
with `j Sp1x2CamGate`, so insn 1 (`lui v1,0x8008`) executes in the DELAY
SLOT. The body resumes at 0x80037bdc whose FIRST instruction is
`lw v1,-30000(v1)` — it dereferences v1. An "optimisation" had dropped the
gate's replayed `lui v1,0x8008` as redundant (the delay slot ran it), but the
clamp code used $v1 as SCRATCH — so the body dereferenced a camera radius,
read a garbage address and hung. Fixed by moving the clamps' scratch to
$t0/$t1 (caller-saved, dead at a function entry); $v1 is now never touched
and the delay slot's setup survives.
**RULE, generalised: an entry gate must leave every register the delay-slot
instruction set up EXACTLY as the body expects. Before claiming a register is
free, DISASSEMBLE THE RESUME POINT — `lw v1,-30000(v1)` was one instruction
away and would have been seen instantly.** This is the same family as the
hooks-19/20 off-by-four: the byte-identical gate cannot see it, only reading
the target code can.
Also settled: the clamps do NOT fix the camera scramble (v2 proved that) but
DO bound it — keep them. The focus guard stays deleted; the watchpoint proved
it defends nothing. The hit latch was NEVER FAIRLY TESTED — both builds that
carried it died on this register bug before the latch could show anything.
PAID FOR by deleting the focus guard, plus three structural finds when BIOS2
came up 1 byte short: Sp1x2FlameChainLoad/Store merged into one direction-
flagged Sp1x2FlameChainXfer (macro shims keep the call sites, Sp1.h) — the
MaskWalk trick again; the duplicate "MULTIPLAYER" literal (stored twice,
-Os does not merge across literals) cut to a "SQUARE" hint; and "1"/"2" (8
bytes, each padded to a 4-byte boundary) folded into one 4-byte
sp1x2_digits[] indexed by +0/+2. NOTE the header trap: editing Sp1.h does
NOT trigger a rebuild — `make clean` after any header change, or the link
fails on stale references to the old names. Sp1x2CamGate is now 6 instructions — trio zeroing
(still needed: double consumption of the shared shake trio is a separate,
real issue) + `j CameraFrameBody` with the replayed `addiu sp,-24` in the
JUMP'S OWN DELAY SLOT (the entry patch's delay slot already executed insn 1,
the lui — only insn 0 needs replaying). Also: Sp1x2HitLatch is
__attribute__((noinline)) — -Os inlined it at both call sites and the dedupe
was the final 8 bytes.
NEAR-MISS during the strip, caught by the disassemble-every-hook rule: the
first cut removed the gate's TAIL too, leaving it to fall through into
Sp1x2HudShift — the built-ROM disassembly check caught it before any test.
IF THE STRATOSPHERE SURVIVES the hit-latch: the reaction may need >2s, the
radius may need widening, or the reacting moby may be outside 0x1000 —
instrument WHICH moby runs a hit-reaction in the wrong pass before touching
camera state again.
Superseded notes follow.
**CAMERA STRATOSPHERE — MECHANISM FULLY MAPPED, CONTAINMENT AT THE CHOKE
POINT (2026-08-23, SUPERSEDED — the clamps did not stop it and the focus
was never corrupted).** The user's calm-vs-spasm camera dumps closed it:
during a spasm m_Simulation (+0x90 az/+0x94 el/+0x98 radius — the spring's
TARGET) holds elevation 939 and radius 0x809C (32924) vs normal ~170/~2500,
while m_Focus reads healthy (the guard repairs it). func_80033F08 explains
the poisoning: the target is DERIVED from |m_Position - *m_Focus| (VecSub
against the focus pointer, magnitude -> radius, Atan2 -> angles) — so ONE
frame of corrupt focus writes a poisoned TARGET that outlives the focus
repair, and the spring chases it (out and back = the exact observed shape).
FIX v2 (2026-08-25; v1 clamped only m_Simulation.radius and FAILED — the
DESTINATION position is computed from m_SPHERE, which the spasm dump showed
equally poisoned): the CamGate now clamps m_Simulation.radius AND
m_Sphere.radius (>= 0x2000 -> 0xA00) AND m_Sphere.elevation (>= 0x2C0 ->
0xA0) before every update. Sim.elevation left unclamped (sphere's is re-cut
every entry; spring creep is bounded). THE FOCUS-REPAIR COUNTER AT 0x8000E4D4
WAS REMOVED for space — it proved its point; do not watch that address.
Spyromain's letter (2026-08-25) supports the multi-frame theory: in Spyro 1
the MOBY may drive the hit reaction across several frames, unlike Spyro 2's
one-frame trigger — his suggested proper fix is a "currently-hit player"
global / manual moby-state handling. If v2 fails: PCSX-Redux write watchpoint
on 0x80076EA0 (m_Focus) at hit time names the corrupting PC — the decisive
forensic we have never run.
Old v1 note: clamp m_Simulation.radius (>= 0x2000 -> 0xA00) before
every update — the poisoning happens in the tick, the gate runs before the
spring reads the target, so the spasm is physically impossible regardless
of who corrupts the focus. The focus repair + counter stay (they measure the
underlying corruption); the shake-trio zeroing stays (retired the glitchy
hit-pulse). STILL UNKNOWN, now optional forensics: which hit-path code
writes 1 into m_Focus (a PCSX write watchpoint on 0x80076EA0 would name it).
Earlier partial notes follow.
**CAMERA STRATOSPHERE — SECOND ROOT theory (2026-08-23, superseded).** The
focus guard ALONE did not stop the spasms: its counter climbed 3-7 per hit
(focus corruption real, repaired) yet P2's camera still flew when P1 was
hit. That cross-player signature named the true root: the HIT-PULSE, driven
by the shake trio (0x800756DC / 0x80075848 / 0x8007590C) which is SHARED
between the two swapped cameras — armed by a hit, consumed correctly by
player 1's camera, then consumed AGAIN by player 2's update against the
wrong camera's reference, exploding the spherical integrator. FIX: the
CamGate now ZEROES the trio before EVERY camera update — the hit-pulse
mechanic is deleted for both players, which also closes the old "zoom pulse
on hits" glitch (logged since 2026-08-18). The focus guard + counter stay
(m_Focus corruption was real; likely a side effect of the same pulse code
writing through camera state — if the counter goes quiet now, that confirms
it).
**BIOS2C RETRACTED SAME DAY — 0x8000EC30 IS NOT FREE.** The game froze at
"The Adventure Begins": something clobbers that gap during LEVEL LOADS (boot
itself was fine). The BIOS3 map gap EC30-ECFF is hereby marked FORBIDDEN for
code; runtime data may also be unsafe there. Payload back to 0x2800/5
sectors (t_size 0x68000, pad 0x68800). THE TWO-STAGE BOOT SURVIVES (LOADER
at 40 free — its best all day). CONSOLIDATION with the retreat: the probe
gate AND the query gate were RETIRED — both defended downstream of the
camera's garbage focus, now repaired at the source by Sp1x2CamGate (entry
patch on UpdateCameraFrame, currently at 0x8000C000, repair + count at
0x8000E4D4). The Baruti addu patches stay. If AdEL freezes at 8004cb68 ever
return, a second scramble source exists — resurrect the gates from history.
Original note follows.
**STRUCTURAL 2026-08-23 (later, partially retracted) — BIOS2C + TWO-STAGE BOOT.** A fourth code
region: 0x8000EC30, LENGTH 0xD0 (the BIOS3 map gap between the P2 Spyro
shadow ending 0x8000EC29 and the flags at 0x8000ED00). Payload now 0x3000
(6 sectors, t_size 0x68800, --pad-to=0x69000): chunk A->BIOS2, B->BIOS2B,
C->BIOS2C. The boot is TWO-STAGE: the LOADER stub copies only chunk A, calls
FlushCache, then calls Sp1x2BootStage2 (which lives INSIDE chunk A, in
Graphics.c) to copy B and C, then FlushCache again — moving the copy loops
out of LOADER entirely (LOADER got FREER: 40 bytes). BIOS2C holds the
entry-gate asm (Sp1x2ProbeGate, Sp1x2CamGate), evicted from the C files
whose -Os cross-jumping fought every byte. Entry patches re-verified in the
built ROM after relocation (the rule).
FOCUS GUARD LIVE: Sp1x2CamGate wraps UpdateCameraFrame's entry (ALL five
call sites): m_Focus outside 0x80010000..0x80200000 or unaligned -> logged
(bad value 0x8000E4D0, count 0x8000E4D4) and repaired to
&g_Spyro.m_Position before the update runs. If stratosphere spasms stop
while the count ticks, focus corruption was the root.

**STRUCTURAL 2026-08-23 — BIOS2B: A THIRD CODE REGION (0x8000E600, 512
bytes).** Space ran out for the fourth time in a day (correct Sparx handling
was 73-109 bytes over with every trick exhausted), so the payload was SPLIT:
now 0x2800 (5 sectors, t_size 0x68000, --pad-to=0x68800 in the objcopy), and
Sp1x2Boot performs a SECOND copy: payload[0x2000..0x2200) -> 0x8000E600.
WHY THIS IS SAFE where the old attempt black-screened: the old attempt copied
CONTIGUOUSLY through 0x8000E000-0x8000E400 — now identified (live TCB dump)
as the kernel's event/thread tables. The new chunk SKIPS them and lands in
BIOS3 space our runtime data has used for months. BIOS2B holds Sp1x2Sparx.c
(the dragonfly lifecycle). ld region BIOS2B, section .bios2b AT(0x68000).
PAID FOR IN LOADER (the second copy loop) by halving the pad-swap blocks
table to 16-bit offsets and inlining Sp1x2RegionSave as a macro.
BIOS3 map: 0x8000E600-0x8000E7FF is now CODE — nothing may allocate there.

**CONFIRMED WORKING 2026-08-23 (user: "flawless, no visual bug either"):
the true-start respawn.** The old respawn-transit camera flash is also gone —
the start point sits far enough from the death spots that the transit reads
as a normal cut. Original fix note follows.
**FIXED 2026-08-23: the under-the-world respawn loop.** After exiting a
level to the homeworld, an individual respawn used g_Checkpoint's
m_StartingPosition RAW — which holds STALE LEVEL coordinates then (retail
only trusts that slot after m_StoodOnCheckpoint, and repairs it inside the
death-reload we skip). Result: respawn under the world, fall, die, repeat to
game over (solo immune — stock path). Fix, per the user's own suggestion:
Sp1x2CaptureSpawn (BIOS2B) records the ARRIVAL position+rotation once per
level entry (called from the P2 seed on a real level change; capture at
0x8000ED20-ED2C, the retired rumble-guard buffer), and Sp1x2Die uses it
whenever no checkpoint has been stood on. Both the position write and the
teleport-detector update follow the same redirected pointer.

**CAMERA STRATOSPHERE — ROOT MEASURED (2026-08-23): m_Focus = 1.** The
refusal-path log caught g_Camera.m_Focus (+0xD0, offset verified against the
compiled store at 0x80013af0) holding the INTEGER 1 at scramble time. Every
decompiled writer stores a real pointer (the asm sites write base-rebased
position pointers, e.g. a1-0x1F4 where a1=&g_Spyro+0x1F4), so the corruptor
is one of ~10 untraced asm store branches. FOCUS GUARD SHIPPED
(Sp1x2FocusGuard, Graphics.c/BIOS2): before EACH camera update (P1 live and
P2's shadow swapped in), a focus outside 0x80010000..0x80200000 or unaligned
is logged (bad value 0x8000E4D0, count 0x8000E4D4) and REPAIRED to
&g_Spyro.m_Position. If spasms stop while the count ticks, focus corruption
was the whole root; the writer can then be hunted at leisure via a PCSX write
watchpoint on 0x80076EA0.
Original hunt note follows.
**CAMERA-STRATOSPHERE ROOT HUNT (2026-08-23, superseded).** Structure now
known from camera.c: m_DestinationPosition = spherical(m_Sphere) + *m_Focus,
where m_Focus (g_Camera+0xD0) is a POINTER (normally &g_Spyro.m_Position;
sequences repoint it at mobys) and m_Sphere (+0x78, coords triple) is spring-
integrated by func_80034480 — the very function whose collision query faults
(caller 0x8003460C is inside it). Two candidate roots: sphere radius
exploding (spring math) or *m_Focus dangling (stale moby pointer). The query
gate's REFUSAL path now logs, at each scramble:
  0x8000E4CC  m_Focus pointer — 0x80078A58 means healthy (focused on the
  live Spyro); a moby-array or garbage value means a DANGLING FOCUS and the
  root is sequence code leaving a stale pointer; healthy focus means the
  SPHERE (spring math) is what explodes, and the next instrument targets
  m_Sphere (+0x78..+0x88). (A sphere log was drafted but cut — BIOS2 byte
  limits; the focus pointer alone splits the two hypotheses.)
Read after any stratosphere event — no crash needed, the counter at
0x8000E4C8 ticking marks the moment.

**OPEN 2026-08-23, priority order:**
1. ~~PAUSE IN A LEVEL SOFT-LOCKS~~ **SOLVED 2026-08-23: hooks 19/20 were
   placed OFF BY FOUR — in the two jals' DELAY SLOTS.** The original `jal
   BuildTextSprites` still ran with a second jal in its delay slot (undefined
   on MIPS) and the PC wandered into the LOADER tail (user's register reads:
   pc AND ra orbiting a000b8xx uncached, sp/gp still the game's, IRQs off,
   CD music playing). Bisect confirmed, correct offsets shipped, VERIFIED by
   disassembling the built ROM at both addresses.
   **PROCESS RULE, hard-won: the byte-identical gate CANNOT catch a +/-4
   hook placement error — it bounds how MANY bytes changed, not WHERE.
   Every new hook must be verified by disassembling build/rom/SCUS_942.28
   at its intended address. The entry patches got this; hooks 19/20 did
   not, and it cost a full soft-lock investigation.**
2. ~~THREE SPARX after an individual respawn~~ **SOLVED 2026-08-23 by the
   user's observation**: "the old sparx flies over from where he died" = the
   death only NULLS g_Sparx (dead-owner reaction) — our same-tick respawn
   restores health before the dragonfly moby dies, so it LIVES ON, and the
   null-heal was spawning a duplicate next to it. Fix: RE-ADOPT — when
   g_Sparx reads null, re-point it at SP1X2_SPARX1_SEEN if that moby's state
   byte is >= 0. NO spawn-fresh fallback (every observed death orphans
   rather than kills; a fallback is what doubled the fly — if some rare path
   truly kills the moby, that player goes sparxless until the next level
   load, the lesser evil). The old fly then visibly swoops from the death
   spot to the respawn — correct, and charming.
   SPACE NOTE: this file's -Os cross-jumping makes small edits cost ~200
   bytes unpredictably (a 7-insn branch was 190 over; removing an 8-byte
   check swung it from 1 over to 136 FREE). When an edit inexplicably
   overflows BIOS2, try removing provably-dead checks before restructuring.
3. Simultaneous double death charges ONE life (shared-sequence design;
   user wants two). A ~7-instruction fix costs ~190 BYTES of lost GCC
   cross-jumping in Sp1x2Spyro.c and does not fit BIOS2 — pending space.
CONTAINED (counters live): ram-hit freezes — guards at 0x8000E4C8/E4EC
tick instead of crashing; camera still visibly flies on some hits (root:
the promoted shake/zoom-pulse item).

**Known issues (logged, not mysterious):**
- ~~PARTICLES strobe white~~ **FIXED 2026-08-22 by porting
  `Sp2x2RenderParticles` after all — the original hypothesis was right, and
  three days of narrowing to `func_800521C0` was chasing the wrong function.**
  `Sp1x2SyncParticles(pass)` walks `g_ParticlesPtr` (0x80075824): 32-byte
  entries, byte 1 = m_Type with 0xFF terminating, 256 max (from spyro-1's
  loaders.c). Pass 1 snapshots bytes 2 (m_Life) and 3 (m_03) to 0x8000F600;
  pass 2 restores the MAX of snapshot and current, so drawing the scene twice
  cannot age a particle twice. Confirmed on the dragon-head nostril smoke.
  THE LESSON: the audit note said Spyromain syncs particle state between
  passes and we had written "deliberately NOT ported until an artefact shows
  a need". When the artefact appeared, the fix was the port we had already
  identified — not the new investigation. Check the reference mod's answer
  BEFORE opening a hunt, which is now the standing research order.
  Original investigation notes follow, kept for the addresses.
  STRONG HYPOTHESIS, and it was predicted in advance: Spyromain's
  `Sp2x2RenderParticles` syncs particle animation state between his two render
  passes, and we deliberately did not port it, noting "deliberately NOT ported
  until an artefact shows a need". THIS IS THAT ARTEFACT. Drawing the scene
  twice advances particle animation twice per frame.
  INVESTIGATED 2026-08-19, NOT SOLVED. A footprint scan of every renderer
  found a real per-draw side effect: `func_80058BA8` ("render glows and
  sparkles", the LAST call in ComposeFrameScene) draws the glows and then
  calls `TickSparkles(g_DeltaTime)` (0x800584c4) — so drawing the scene twice
  aged every particle in the game at double rate. Hook 13 now ages them only
  in pass 1, which is CORRECT and kept... but it did NOT fix the strobing, so
  the nostril smoke is not sparkles.
  ALSO RULED OUT: world texture animations (TickWorldChunkAnimations is called
  from GamestateUpdate, once per frame, not from the draw path) and the world
  renderer (it only READS g_EnvironmentAnimations).
  **STILL UNSOLVED. Narrowed to "queue render mobys" (`func_800521C0`)
  running once per PASS** — and NOT to the region table, which was the same
  function's other side effect and did fix the chests (above) without touching
  the strobe. Calling it only
  in pass 1 FIXES the strobe completely — but it also leaves player 2's
  viewport with NO MOBYS AT ALL, so it cannot simply be hoisted; each pass
  needs its own queue. The side effect that breaks the smoke is inside that
  function and is still unidentified. What it does: walks the level mobys,
  fills THREE lists (around D_8006FCF4 and D_800771C8), and clears m_WasDrawn
  (`sb $zero, 0x51(moby)`) on the ones it does not queue.
  NEXT STEP: find what in that walk advances per call — most likely an
  animation/emitter field on the moby — and give it the flame-chain treatment
  (one saved value per viewport) instead of trying to skip the call.
- **FLIGHT LEVELS ARE SINGLE-PLAYER FOR NOW (2026-08-19).** Two bugs, both
  bisected to a cause, both worked around rather than fixed:
  (1) *Flight steering is FRAMERATE-SENSITIVE — not a state bug.* MEASURED
      2026-08-19 by reading `g_DeltaTime` (0x800756CC, the physics SUBSTEP
      count) live while flying:
        heavy scene  -> dt 6-7  -> steering barely responds
        turn away    -> dt 3-4  -> steering normal again, instantly
      The correlation is with FRAME TIME, and it reverses as soon as the view
      empties out. `main()` sets dt from elapsed vblanks and clamps it (stock
      range 2..4); pitch and yaw are integrated per SUBSTEP while other motion
      is applied per frame, which is why flight is the only place it shows.
      RULED OUT ALONG THE WAY, each by a built-and-tested build: the flight
      attitude controller (touches only g_Spyro, which we swap),
      g_nSpyroPitchRateAccum (already swapped), the substep counter
      g_UnprocessedFrames (protected both ways), g_CollisionNormal (added to
      the swap set — kept, it was genuinely missing), the player-collision
      push (disabled in flight — kept, it does not belong there), and raising
      the substep clamp from 4 to 8 (REVERTED: dt then reached 7 and steering
      was still wrong, so the clamp is not the limiter).
      THREE LEAK SWEEPS over 0x80075600-0x80079600 and the overlay region
      found NO unrestored state — the tick leaks nothing.
      **PARKED 2026-08-20 after a full Spyro2x2 audit. THE REFERENCE MOD DID
      NOT SOLVE THIS EITHER: `Sp2x2TeleportSpyro.c` carries the comment
      "// TODO for icy speedway minigame" — Spyro 2's equivalent content.** So
      there is no missing port to find; flight/speedway levels are the one
      thing Spyromain left unfinished too.
      COMPLETE AUDIT of his per-player state vs ours (2026-08-20):
        PORTED: sp2_camera, sp2_spyro, sp2_spyro_flames, sp2_spyro_shadow,
                sp2_pad / pad_2 / pad_port_1 / pad_queue,
                sp2_8006eba8 (256B, swapped per RENDER PASS — this is his
                region-visibility table; we found Spyro 1's D_800771C8
                independently and it fixed the flight chests),
                sp2_calculation_results (Spyro 1 splits it into four collision
                scalars: g_SurfaceBelowFlags 0x80075718,
                g_CollisionTriangleIndex 0x80075808, g_CollisionPoint
                0x80076B80, g_CollisionNormal 0x80077368 — all now swapped;
                correct, though it did NOT fix flight steering)
        NOT PORTED, no Spyro 1 equivalent identified: sp2_ticks (0x8006c580),
                sp2_tick_counter (0x8006c5fc), sp2_level_timer (0x8006c5c4),
                sp2_8006c7e0, sp2_actuator_data (rumble), sp2_is_reticle_displayed,
                sp2_sparx (we pin Sparx to P1 by design)
      Spyro 1's g_DeltaTime and g_GameTick were checked as candidates for the
      tick globals: NEITHER is modified by the Spyro tick, so swapping them
      would be a no-op.

      **RULED OUT 2026-08-20 using spyro-1 PR #68** (which decompiles
      func_8003DAE4, the flight attitude controller): it is a spring-damper
      driving pitch toward the floor slope, called once per ROTATION SUBSTEP
      (so dt times a frame) while steering input is applied once per frame.
      Capping it to retail's 2 pulls per frame did NOT restore pitch
      authority — so the spring is not the limiter either. Reverted.
      (The PR did settle one thing: an earlier attempt capped
      RotateSpyroToNeutral, which only runs when m_slopeAngle >= 0x17 — over
      flight-level terrain that branch never executes at all.)
      THAT LEAVES func_80047B60 as the prime suspect: the per-frame dispatch,
      where the steering INPUT itself is applied. Until it is decompiled we
      cannot see how input becomes pitch, and every theory about the output
      side has now been tested and failed.

      **WATCH THESE FIVE FUNCTIONS in the spyro-1 decomp — when any of them is
      decompiled, this becomes tractable** (all still INCLUDE_ASM as of
      2026-07-14; pete.c has 11 functions left in asm):
        func_80047B60  DispatchSpyroPhysicsByState  0xd2c   <- MOST WANTED:
                       the PER-FRAME state dispatch, where flight steering
                       input is applied once a frame while the attitude
                       controller neutralises once per SUBSTEP
        func_80043FE4  IntegrateSpyroMotionForSubstep 0x3b7c
        func_8003DAE4  UpdateSpyroFlightAttitudeNearGround 0x360
        func_80041670  UpdateSpyroStateBehavior     0x2974
        func_8004A200  TickSpyroGameplayFrame (the function we hook)
      Already decompiled and read: func_8004888C (the rotation substep).

      Superseded lead follows — **it comes from Spyro2x2.**
      `Sp2x2LogicSpyro` swaps FOUR time/scratch globals around each player's
      logic that we have NEVER found Spyro 1 equivalents for:
        sp2_ticks (0x8006c580), sp2_tick_counter, sp2_level_timer,
        sp2_calculation_results (56 BYTES of scratch)
      He treats elapsed time and physics scratch as PER-PLAYER state. Our audit
      noted this gap long ago and it was never closed — we ported his camera
      swap and stopped.
      Checked and NOT the answer: `g_GameTick` (0x8007572C) is written only by
      GamestateUpdate, once per frame, so it is not double-advanced.
      NEXT: find Spyro 1's analogue of `sp2_calculation_results` — a ~56-byte
      physics/collision scratch buffer written during the Spyro tick — and add
      it to the swap set. Note the leak sweeps flagged blocks at 0x80075700 and
      0x80075800 which were dismissed as "clocks we already handle"; that
      dismissal deserves re-checking against a full list of what we swap.
      ALSO TRIED AND REVERTED 2026-08-20: capping RotateSpyroToNeutral (called
      once per SUBSTEP from the flight attitude controller, against input
      applied once per FRAME) to retail's 2 pulls per frame. The mechanism is
      real and confirmed in pete.c, but capping it did not restore steering.
      MEASURED: g_DeltaTime sits pinned at 4 in normal play, in both the
      homeworld and flight levels.
      Earlier conclusion, now doubtful: the fix is PERFORMANCE, not logic. Flight levels are the
      most expensive scenes in the game and we draw them twice. Options, in
      order of expected value: (a) the parked `DrawSync` stall, (b) render
      flight levels single-view (PROVEN to restore steering completely), or
      (c) auto-select the CROPPED view fit there, which measured ~+1 FPS per
      step. Do NOT go looking for a shared variable again — it is not one.


  (2) ~~Destructible chests invisible~~ **FIXED 2026-08-19 — and the fix was
      a REAL bug affecting the whole game, not a flight-level quirk.**
      `D_800771C8` is a 256-entry REGION VISIBILITY table: the world renderer
      (SetupFrameOT / r_environment) writes it for whichever camera just drew,
      and "queue render mobys" (func_800521C0) reads it per moby —
        `lbu t1, 0(D_800771C8 + region); bgtz t1, keep; sb zero, 0x51(moby)`
      — dropping any moby whose region is not visible. With one camera that is
      correct. With two passes each pass queued against the OTHER player's
      visible regions. In flight levels player 2 is frozen, so his region set
      never included the chests and player 1 never saw them.
      FIX: one saved table per viewport (0x8000F400 / 0x8000F500, valid-bits
      at 0x8000F3F0) — restore before the queue reads it, save after the world
      renderer writes it. Same shape as the flame chains.
      Ruled out before this, each by a build: moby update once, Spyro tick
      once, m_WasDrawn sync, particle ageing, and restoring the culling grace
      after the queue call.

  PROPER FIXES, when someone picks this up:
    - for (1) find the flight state the tick shares and add it to the swap
      set, the way Sp2x2LogicSpyro's list was completed;
    - for (2) find the chest renderer and give it per-viewport state, the way
      the flame matrix got one chain per (dragon, viewport). The r_flame trick
      applies: grep the renderer's asm for `%hi(...)` to get its footprint.
  Also noted: the "+3" pickup text appears in BOTH viewports (enqueued once,
  drawn by both passes) — cosmetic, same family.

- ~~P2 sequence triggers broken (portal float, dragon rescue with no Spyro,
  balloonist dialogue loop, phantom life loss)~~ — addressed 2026-08-18 by
  GENERALISING the death handover: any global sequence P2 triggers (tick OR
  moby pass) leaves his spyro+camera live and re-seeds on return to gameplay.
  Needs testing.
- ~~No Sparx for P2~~ **BUILT 2026-08-22, needs testing** — and the old note
  claiming this was "beyond even Spyromain's finished work" was WRONG: he
  built it (`sp2x2_rayz`, created via sp2_create_moby_function(0x78,0) and
  pointer-swapped with sp2_sparx around P2's moby pass). Ours is the same
  shape: g_SpawnMoby(120,0) — the overlay factory POINTER at 0x800758CC,
  retail's own Sparx spawn from LoadLevelScene — called lazily inside P2's
  moby pass (overlay certainly loaded, P2's Spyro live), pointer kept at
  0x8000ED10, cleared on reseed. Around P2's pass: g_Sparx swapped to his
  dragonfly, the 0x80077798 anchor pointed at P2 (restored after, except on
  handover), force-owned owner 1 in the partition. Health colour is free —
  P2's Spyro is live during his pass. Flight levels: no Sparx, as retail.
  FIRST BUG FOUND IN TESTING, fixed same day: a THIRD Sparx wandering between
  players, appearing mid-play with NO obvious trigger (user: "just running
  around" — it predated the dragon rescue that session). Mechanism: the
  pointer was cleared on every RESEED while the old moby survived in the
  level, so each reseed orphaned one dragonfly and spawned another. Reseeds
  follow ANY P2-triggered sequence — save-fairy prompt, dialogue, portals,
  shared death — so the specific trigger was never identified and did not
  need to be.
  SECOND BUG, caused by the first fix: content-validating the moby used a
  m_Class offset GUESSED by counting struct fields through a SHORTMATRIX of
  assumed size. Wrong offset -> the check failed every frame -> clear+respawn
  became a SPARX FOUNTAIN that overflowed the moby array and crashed at
  spawn. Final fix, IDENTITY ONLY, no struct offsets: g_Sparx is respawned by
  every path that rebuilds the moby array (LoadLevelScene assigns it), so
  `g_Sparx != last-seen` (0x8000ED14) IS the rebuild detector — clear our
  pointer then and only then, plus the g_Sparx-aliasing check. Reseeds never
  clear -> no orphans. LESSON: never gate a per-frame spawn on a check whose
  failure mode is "spawn again" unless every offset in it is VERIFIED.
  If a third Sparx appears on a build after this, the orphan theory is wrong
  — measure, do not re-guess.
  RESPAWN FOLLOW-UPS (2026-08-22):
  (a) Individual respawn at a rescued pedestal auto-opens the SAVE-FAIRY
      prompt (retail hides this behind the level reload). **A gate was built
      and REVERTED the same day — DO NOT REBUILD IT the same way.** The
      entry-patch technique itself worked (InitFairyCutscene 0x8002c924 insn
      0 -> `j gate`, insn 1 `move a3,a0` side-effect free in the delay slot,
      gate replays insn 0 and resumes at +8 — keep this pattern in the
      toolbox), but SUPPRESSING the init is unsafe: the overlay fairy sets
      state BEFORE the call (freezes the player, hides his Sparx), so a muted
      init left P2 FROZEN at the pedestal, unrecoverable — the same lesson as
      the death-sequence crashes: never refuse a sequence its caller has
      already begun setting up. And the prompt STILL appeared at respawn, so
      the overlay draws the box independently. Any real fix must start by
      reading the overlay fairy moby's code. ACCEPTED for now: the prompt
      auto-opens on pedestal respawn; press through it.
  (b) BOTH players' dragonflies vanished after an individual respawn: the
      death leaves the Sparx moby in a terminal state and only the (skipped)
      level reload rebuilds it. MEASURED: that state is NOT negative — a
      state<0 revive never fired. Fix that works: Sp1x2Die DECLARES the dying
      player's dragonfly dead (state byte 0x80 = the game's own dead-but-
      holds-slot, NEVER -1 which terminates the array walk), and the moby
      passes respawn any dead dragonfly fresh. SP1X2_TICKING_P2 (0x8000ED18)
      re-added to know whose dragonfly to kill.
  (c) The row-4 pause item has THREE variants (draw.c:1235): QUIT (flight) /
      EXIT LEVEL (level) / QUIT GAME (homeworld). Only the homeworld one was
      suppressed under our Multiplayer page, so "EXIT LEVEL" bled through in
      levels. Hooks 19+20 (0x8001b4f0, 0x8001b574) -> Sp1x2MainMenuItem.
  KNOWN, LOGGED, NOT YET INVESTIGATED:
  - Ram enemy hitting either player OCCASIONALLY FREEZES the game (several
    builds old, confirmed reproducible 2026-08-22). MITIGATION SHIPPED same
    day: Sp1x2RumbleGuard saves/restores the EIGHT rumble/pad-mode globals
    (IsDualshock 0x800756d8, SetMainModePending 0x80075730, VibrationLevel
    0x80075764, PulseRumbleTimer 0x800757d0, ActCommand 0x80075800,
    PulseRumbleAmount 0x8007584c, ActAlignedFlag 0x800758e0, HitRumbleTimer
    0x80075904) around poll 2, making it rumble-neutral — completing the
    protection list the pad notes always said was incomplete. IF A RAM
    FREEZE HAPPENS ON A BUILD AFTER THIS, the hypothesis is falsified —
    measure, and suspect a wait loop (music kept playing in every freeze).
  - SPARX AFTER RESPAWN — SOLVED 2026-08-22 BY MEASUREMENT, third attempt.
    The reads showed g_Sparx == 0 AND our pointer == 0 while one dragonfly
    still flew. Root cause, confirmed in the decomp: EVERY level overlay
    contains `sw $zero, %lo(g_Sparx)` — THE GAME ITSELF nulls the pointer
    when the dragonfly moby dies. The declare-dead+eager-respawn design
    fought that: the overlay nulled the pointer to our replacement (the
    stealable orphan), and both spawn gates (`g_Sparx != 0`) then locked
    shut forever. WORKING DESIGN: let the overlay kill and null, and HEAL
    THE NULL — in non-flight gameplay `g_Sparx == 0` only ever means "sparx
    died", so respawn on seeing it, assigning ONLY on spawn success (the
    unchecked assign is how the pointer got nulled permanently).
  - TELEPORT DETECTOR gated by sequence type (2026-08-22): a >0x4000 jump
    right after gamestate 8/11/12 (dragon/fairy/balloonist) is the SEQUENCE
    repositioning the live dragon, not a restart — skip the reseed and let
    handover resume swap identities back, so each player keeps his own
    position (was: rescues snapped the pair together). Last non-zero
    gamestate at 0x8000ED18, consumed on use. Flight retry (results screen,
    7) and transitions reseed as before.
  - PEDESTAL-RESPAWN DIALOGUE: fires on SOME respawns, not others, on BOTH
    emulators — not overclock-related. Unexplained; accepted.
  - RAM FREEZE — ROOT MECHANISM IDENTIFIED 2026-08-23 by CP0 crash report
    (user-driven PCSX-Redux debugging): Cause 0x10 = ADDRESS ERROR ON LOAD,
    EPC 0x8004cb68 = the collision query's chain walk (asm/collision.s,
    .L8004CB5C: it walks m_CollisionChainNext at moby+4 per bucket), BadVAddr
    0x008eb8ca = GARBAGE. Something overwrites a moby's chain pointer.
    Readable garbage -> the walk spins forever (freeze, "In ISR: no");
    unaligned garbage -> AdEL fault, no handler, kernel spins in its
    dispatcher (freeze, "In ISR: yes", identical GPRs every pause). Music
    survives both — CD-streamed by hardware. Rumble hypothesis falsified
    earlier (guard kept anyway; it is correct protection).
    Chain facts for later: buckets = g_MobyCollisionChain (loaders.c:663),
    cell = (x>>13, y>>13) cached in m_CollisionRegion (+0x34), re-filed on
    cell change by func_800529E4 (unlink walk + head insert); heads staged
    through scratchpad 0x1F800000 by the query.
    MITIGATION+INSTRUMENT SHIPPED: a sweep riding Sp1x2AssignMobys' walk
    (twice per frame, before the ticks' queries) cuts any next pointer that
    is not null/aligned/in 0x80010000..0x80200000 and records at 0x8000E4A0:
    repairs-ever / victim moby address / garbage value. READ THOSE AFTER ANY
    SESSION. Nonzero count = corruption fired, victim+value name the writer.
    OUTCOME 2026-08-23: count stayed ZERO through sessions INCLUDING a
    freeze — moby chain pointers are NEVER corrupted; sweep retired to pay
    for the next instrument. Second freeze CP0: same EPC 8004cb68, garbage
    fp 0x05200002 (first was 0x008eb8be) — both shaped like COORDINATE data,
    both players' positions sane at freeze time. Working theory: a query
    around a BOGUS CENTRE indexes past the bucket table and walks level data
    as chain heads. (Also learned: 0x8000E000+ holds the kernel EVENT TABLE,
    not saved GPRs.) **CAUGHT 2026-08-23 by the query logger
    (hook 21, entry patch on func_8004BE4C, log at 0x8000E4B0):** the fatal
    query came from CAMERA code (caller 0x8003460C) around
    g_Camera.m_DestinationPosition (+0x34) with a HALF-GARBAGE centre —
    X sane (0x1FF08), Y absurd (0xCB463A ~ 13M), Z zero. Y>>13 indexes
    ~200KB past the bucket table -> level data walked as chain heads. SAME
    FAMILY as the parked zoom-pulse item: the camera shake/hit-reaction
    globals are SHARED between the two swapped cameras (touching them froze
    on a hit TWICE, historically). Zoom pulse = mild face; freeze = severe
    face. ROOT FIX = that item.
    GUARD SHIPPED: the gate refuses any query whose centre has a coordinate
    >= 0x400000 or negative (beyond every legitimate level coordinate),
    returns "no collision", and COUNTS refusals at 0x8000E4C8 — each tick is
    one averted freeze, measurable in the wild. Log stays at 0x8000E4B0.
    ROUND 2 (2026-08-23): guard 1 HELD — 50 refusals counted, no fault at
    8004be4c — and the freeze MOVED to the sibling: EPC 0x8004b7c8 inside
    func_8004AE38 (the two-vector SEGMENT probe Spyro's tick uses), BadVAddr
    0xb. The last camera query before that freeze had ALL THREE dest coords
    insane. Sp1x2SeparatePlayers was suspected (div-by-zero on coincident
    dragons) and EXONERATED by reading — dist<=0 is handled. Positions were
    SANE in freeze 1's dump, so Spyro's position is not exploding; the
    camera-side state alone goes bad on hits. GUARD 2 SHIPPED (hook 22,
    Sp1x2ProbeGate): logs a0/a1/ra at 0x8000E4E0/E4/E8, refuses when the
    END vector has a coordinate >= 0x400000 or negative (start+offset ⇒
    checking the end suffices), counts at 0x8000E4EC. At a freeze the logged
    pointers still lead to the frozen vectors.
    ROUND 3 (2026-08-23): guard 2 held too — counters read 168 (camera) and
    2337 (probe) with NO crash on a hit that visibly SHOULD have crashed:
    the user WATCHED P2's camera fly stratosphere-high on impact and spring
    back. The probe refusals' logged caller = 0x80033EC0 = inside
    func_80033E40, the camera's LINE-OF-SIGHT RAYCAST — and the decomp
    documents a RETAIL CRASH at that exact spot ("Baruti crash", camera.c:97:
    VecMagnitude's signed adds overflow-trap on huge distances; their
    recommended fix is add->addu). Our camera-spasm distances trigger it
    readily, and VecMagnitude is called everywhere (incl. our sound hook), so
    hooks 23/24 apply the addu fix — a RETAIL bug fix, zero code bytes.
    STATUS: crash contained by guards 1+2 + Baruti fix; counters at
    0x8000E4C8 / 0x8000E4EC measure the underlying camera scramble live.
    STILL OPEN (the true root): WHY the camera state goes stratospheric on
    hits — the parked zoom-pulse/shake item, now promoted: it is the last
    unexplained piece, everything downstream of it is guarded, and the
    raycast segments of one spasm may still cause a visible HITCH (a
    million-iteration segment loop is refused fast but not free).
    RUMBLE GUARD RETIRED to pay for guard 1: built for this freeze, now proven to
    be the camera query; weeks of pre-guard builds showed no rumble
    oddities. Addresses if ever needed: IsDualshock 0x800756d8,
    SetMainModePending 0x80075730, VibrationLevel 0x80075764,
    PulseRumbleTimer 0x800757d0, ActCommand 0x80075800, PulseRumbleAmount
    0x8007584c, ActAlignedFlag 0x800758e0, HitRumbleTimer 0x80075904.
  - BIOS2 space was reclaimed by merging duplicated loop skeletons into
    moded walkers: Sp1x2SpyroTableWalk (seed=copy / swap), Sp1x2MaskWalk
    (mask / unmask), and the rumble guard's src/dst form. Same pattern is
    available if space runs out again.
  - After a P1-triggered dragon rescue, P2 snaps to P1's side (the rescue
    moves P1 far enough to trip the teleport detector -> reseed). Design
    trade-off, not a defect; refine only if it grates.
  PAID FOR BY: the obsolete SELECT/START toggles (~437 bytes, superseded by
  the pause menu 2026-08-19) and compressing the Spyro swap table to 16-bit
  offsets from 0x80075000. -Oz was measured: identical to -Os, zero help.
- ~~Sound distance measured from P1 only.~~ FIXED for one-shots (2026-08-18):
  attenuation is camera-based (spu.c), so P2's camera is now swapped around
  his moby pass. Residual: LOOPING voices re-attenuate against P1's camera
  in SoundsUpdate — cosmetic, deferred.
- Camera briefly zooms out/in on some hits. Suspect: camera SHAKE globals
  (g_nCameraShakeMagnitude 0x800756dc etc.) live OUTSIDE the swapped g_Camera
  struct, shared between players. Polish pass.
- ~~Sparx midpoint hover~~ FIXED 2026-08-18: `g_anCameraLatchedAnchorPos`
  (0x80077798, XYZ) is the general "where the player is" anchor written by the
  Spyro tick each frame — followers home on it. Restored to P1's value after
  P2's tick (except on the handover path, where P2's sequence may latch his).
  open-spyro's camera-centric name misled, again.
- ~~Sheep/some enemies fast-forward near P2~~ — addressed 2026-08-18: dyn
  mobys are a contiguous continuation of the level array (sentinel moves on
  spawn), so P1's pass could spawn fodder past our recorded count, leaving it
  unmasked (double-updated) in P2's pass. Fix: reassign between passes. Suspect: DYNAMIC mobys (fodder
  is spawned, not placed) are not in the level array we mask, so they update
  in BOTH moby passes = double speed. Spyromain special-cased fodder spawners
  (`_Sp2x2UpdateFodderSpawner`, moby id 3) for exactly this family of reason.
  Needs the dyn-moby list found and masked, or a fodder special case.
- **P2-context one-shot sounds start correctly but truncate** ("a quick second
  of the sound"). Cause: TickActiveSoundVoices re-attenuates and RANGE-KILLS
  active voices per frame against the LIVE (P1) camera, so a voice started at
  P2-relative volume dies next frame. Fix candidates: patch the range-kill in
  TickActiveSoundVoices (instruction patch), or min-distance over both
  cameras. Polish pass.
- Enemies occasionally wander aimlessly. Suspect: owner-flicker — a moby
  equidistant from both players is reassigned every frame, so its AI target
  alternates. Fix when confirmed: hysteresis in Sp1x2AssignMobys (keep owner
  unless the other player is meaningfully closer). Polish pass.

**STATUS 2026-08-18: CORE CO-OP IS BEHAVIOURALLY COMPLETE.** Two dragons,
independent control, per-player cameras, enemies/gems/fodder per nearest
player, Sparx with P1, sequences (dragons/balloonist/portals/death) correct,
lives shared, level transitions clean. Remaining COSMETIC punch list:
0. ~~Flame breath bends mid-jet~~ **FIXED 2026-08-18.**
   CAUSE: Spyro's flame has NO orientation of its own. `r_pete` — the MODEL
   renderer (0x80023ac4, which we also call as RasterizePairedActor) — NUDGES
   a matrix at g_SpyroFlame+0xB8 (5 words) every time it draws, and the flame
   renderer (0x80058d64, r_flame.s) reuses it. It is a RUNNING CHAIN:
   new = nudge(previous, current camera). Retail draws the model once per
   frame so the chain stays smooth; we draw the scene TWICE (once per
   viewport), so ONE chain was dragged between two cameras twice per frame and
   never settled — showing up as a bend in the jet's outer half.
   FIX: one chain per (dragon, viewport) — four in all, at 0x8000E500 +
   player*0x40 + pass*0x20. Load before the model draw, store after, so each
   chain gets exactly one nudge per frame with a consistent camera, which is
   what retail's single chain gets.

   **HOW IT WAS FOUND — the process lessons matter more than the fix:**
   - NINE experiments aimed at player 2 (his tick, draw, ribbons, camera,
     state) ALL came back negative. The bug reproduced with **controller 2
     DISCONNECTED** — a zero-cost test that should have been FIRST. Before
     hunting which of our features causes a bug, check whether the bug needs
     that feature at all.
   - Same for comparing against RETAIL: one minute, and it proved the bend was
     ours rather than stock. Establish the baseline before bisecting.
   - Four bisections then found it fast: single pass + full viewport = smooth;
     single pass + half viewport = smooth (so viewport innocent); two passes
     with the model suppressed in pass 2 = smooth; only the matrix write
     suppressed = smooth.
   - FOUR fix attempts still failed (skip the second write; snapshot a base
     and restore it before pass 2; also restore pass 1's result afterwards) —
     each only MOVED the bend between viewports, because they all shared ONE
     chain.
   - What settled it was MEASURING: a probe reporting four booleans (did pass
     1 write / did pass 2 write / are the two results identical / is the value
     unchanged since last frame) showed the matrix is BOTH camera-dependent
     AND self-referential. That combination forces the per-viewport-chain
     design — no way of juggling a single chain could ever have worked.
0b. ~~Player-to-player collision~~ **DONE 2026-08-19.** Confirmed working:
   players push each other, charging shoves, pushing into walls is safe,
   jumping on top is harmless.
   Spyro is NOT a moby, so no engine collision machinery applies and there was
   nothing to hook — and Spyromain never implemented this either, so it is our
   own design. `Sp1x2SeparatePlayers()` runs at the end of the tick hook, once
   both dragons have moved: if they overlap horizontally, each is pushed half
   the overlap apart along the line between them.
   Choices that made it behave: HORIZONTAL ONLY (axis 2 is up), POSITION not
   velocity (velocity fights the game's physics and feels mushy), and small
   per-frame corrections so the game's terrain collision resolves anything we
   push them into. One knob: `SP1X2_BODY_RADIUS` = 0x1A0 (416 world units,
   centre to centre). Possible future variant if ever wanted: make the pusher
   stop instead of both sliding, for more wall-like behaviour.
0c. ~~Portal transition should show all dragons~~ **DONE 2026-08-22.**
   Hook 16 over the ONE `jal RasterizePairedActor` at 0x8001a0d8, inside the
   draw handler (0x8001a050) shared by GS_LevelTransition (1) and
   GS_EntranceAnimation (9). Draws the live dragon TWICE — not player 2's
   state swapped in, because then he stood frozen in his own pose while
   player 1 played the flying animation; reusing the live dragon gives both
   the same animation for free, and Spyro 1 has no per-player colouring.
   **THE FLIGHT IS FAKE, and that is what made the offset hard.**
   camera.c:452-460 parks Spyro at a fixed m_SpyroPosition with a fixed
   m_SpyroRotation and ORBITS THE CAMERA around him; the motion is the
   scrolling cyclorama. So there is no stable "beside him" in world space.
   Three attempts each failed differently before the answer:
     - world axis 0: the orbit periodically looks down that axis and the gap
       collapses into DEPTH;
     - row 0 of the matrix at g_Camera+0x14: came out VERTICAL — and that is
       not even the matrix Spyro is drawn with. A %hi footprint scan of
       r_pete shows it reads ONLY g_Camera+0x00 (projection) and +0x28
       (m_Position);
     - perpendicular to the camera->dragon line: right on screen, and
       MEASURED right, but anchored to the CAMERA, so the wingman slid around
       the dragon as the camera orbited.
   ANSWER: offset along the dragon's OWN lateral axis, from
   m_Physics.m_SpeedAngle.m_RotZ (g_Spyro + 0x11C, cross-checked via
   pete.c's m_TrueVelocity.z read at 0x80078B6C = +0x114). The pair is then a
   RIGID FORMATION that turns with him. Heading is (cos, -sin) BY OBSERVATION
   — deriving it from Atan2's argument order gave the wrong axis and drew the
   second dragon nose-to-tail.
   **DEPTH SORT IS DRAW ORDER.** No Z-buffer; Spyro's model enters the OT at a
   coarse depth, so with two dragons in one bin the later draw wins outright
   and the second showed through the first. Fixed by drawing the FARTHER one
   first. Spacing shares SP1X2_P2_START_OFFSET with the level seeding, so the
   dragons cannot jump apart when the split returns.
   KNOWN, ACCEPTED: on the first frames of an entrance the wingman snaps into
   formation, because camera.c writes the fly-in rotation a frame or two in
   and until then the yaw is the previous level's.
0e. ~~Dialogue only advances with player 1's buttons~~ **DONE 2026-08-22**,
   the port of `Sp2x2LogicDialogue`. At the end of the VSync pad callback,
   when `g_nGamestate != 0` (i.e. a dialogue/menu state, where only one input
   stream matters), player 2's `m_Down` and `m_Held` are OR'd into the live
   block. Either controller advances a conversation; neither can steer during
   gameplay, because the merge is gated on the non-gameplay state.
0d. **PvP flame — TRIED AND REVERTED 2026-08-22. Full write-up in
   `PVP_FLAME_NOTES.md`; read that before any retry.** Optional friendly fire
   as a Multiplayer-menu switch. Detection all worked and was MEASURED
   (setting, call count, both flame-active bytes, distance). Applying the
   damage did not. Two findings worth having regardless of PvP:
   - **`m_DamageFlags` (g_Spyro+0x2C = 0x80078A84) is the set of hazards
     TOUCHING Spyro this frame, not "damage allowed" or "damage applied"** —
     spyro-1's header comment says the latter and is misleading.
     `HandleSpyroDamage` (0x80040f68) ANDs its argument against it and returns
     at once when nothing is touching him, which is why calling it from
     outside the collision path can never do anything.
     Also confirmed: m_invulverabilityTimer +0x160, m_health +0x164,
     god mode 0x800756A0, real-damage bits 0x5F1, hazard = 0x20,
     state setter func_8003EA68 = 0x8003ea68.
   - **Forcing a state transition from outside the tick FROZE the game** —
     the same shape as the two death-handling crashes above. The likely right
     design (and what Spyromain actually does) is to set the contact bit on
     the victim BEFORE his tick and let the engine consume it.
   **PROCESS TRAP THAT COST A CYCLE: `make` stops at the verification gate —
   it does NOT repack the disc. `make disc` does.** A change was reported as
   failing when the disc under test was 18 minutes stale. Check
   `build/rom/SCUS_942.28` against `build/disc/spyro1-coop.bin` timestamps.
1. ~~P2 one-shot sound truncation~~ DONE 2026-08-18 — hook 6 redirects the
   `jal VecMagnitude` in TickActiveSoundVoices to a min-over-both-cameras
   distance, so a sound near EITHER player is near. Vanilla kill radius.
2. Zoom pulse on hits. **PARKED 2026-08-18 after two freezes — and the
   diagnosis was WRONG anyway.** A whole-binary xref proves the shake trio
   (0x800756dc / 0x80075848 / 0x8007590c) is touched ONLY by
   UpdateCameraFrame and ResetCameraStateToTarget — so NOTHING in the damage
   path writes them, and they cannot be what pulses on a hit. Swapping them
   also froze the game on a hit, twice, cause unknown (shadow placement was
   ruled out the second time; music kept playing = main loop stuck, so
   suspect a wait loop). Since g_Camera IS swapped yet BOTH views zoom
   together, the real culprit is a SHARED projection/FOV-ish global outside
   the struct. Candidates from the UpdateCameraFrame scan, unverified:
   0x800756cc, 0x8007572c, 0x80075844, 0x80075864, 0x8007592c. Start there,
   and MEASURE which changes on a hit before touching anything.
   Old (wrong) note follows: FIRST ATTEMPT CRASHED (2026-08-18, reverted) —
   froze the game on the first hit taken, enemies misbehaving beforehand.
   Cause was ONE thing: the shadow buffer was placed at 0x8000DF00, which is
   INSIDE the BIOS2 payload (boot copies 0x2000 bytes to 0x8000C000, i.e.
   through 0x8000E000) — every hit overwrote our own code. **RULE: never put
   runtime data in 0x8000C000-0x8000E000.** The three shake addresses
   (0x800756dc / 0x80075848 / 0x8007590c) turned out to be CORRECT — a scan
   of UpdateCameraFrame shows it reads all three and writes two. Retried
   2026-08-18 with the shadow at 0x8000E500 (BIOS3 free space).
3. ~~Occasional aimless enemies~~ DONE 2026-08-18 — 25% hysteresis in
   Sp1x2AssignMobys: keep the current owner unless the other player is
   meaningfully closer. (Matches the standard co-op/aggro approach: sticky
   target with a switch threshold, rather than nearest-wins every frame.)
4. ~~Per-player HUD~~ **DONE 2026-08-22, and it was never really blocked.**
   `EnqueueLoadingScreenSprites` is open-spyro's name for func_80019300, which
   spyro-1 shows is the HUD COMPOSER (it gathers g_Hud.m_Mobys into the shaded
   list) — and we ALREADY called it once per pass, so both viewports had been
   building their own HUD all along. Only the position was wrong: the HUD is
   2D and DRAWENV.ofs moves 2D sprites, so it drew off the top edge.
   Sp1x2HudShift adds half/2 along the split axis for BOTH passes (pass 0
   wants +half/2, pass 1 wants +half/2 — same number), applied for the whole
   pass and undone at the end so HudTick's state is untouched.
   g_Hud = 0x80077FA8 (two self-addressing fields in spyro-1's hud.h agree,
   and open-spyro names the address AND notes "icon-record base at +0x44").
   Groups: mobys 0-4 gems, 5-7 dragons, 8-10 lives, 11 key; rects 0-11 eggs,
   12+ life orbs (placed relative to moby 10 by HudReset and only recomputed
   there, so they must travel with the lives group).
   SIDE-BY-SIDE needs a relayout, not a shift: 256 columns against a 464-wide
   HUD. Per-MOBY offsets, because the digit sits a different distance from its
   icon in each group (+44 gems, +34 dragons, +42 lives) — translating whole
   groups left the numbers in a ragged column. Icons share x=34, digits x=82,
   rows 32 apart.
   **HALF SIZE WAS TRIED AND REVERTED. Do not retry without measuring first.**
   z is depth, and screen = C + (pos - C) * z_stock/z with C = (SP1_GEOM_OFX,
   SP1_GEOM_OFY) — so doubling z to halve the size ALSO drags the HUD toward
   screen centre. Compensating (pos = C + 2*(want - C)) made the projection
   correct but pushed raw coordinates to x=-164 / y=472, where the moby
   renderer culls them and the HUD vanished entirely. The projection model is
   sound; what is unknown is what the renderer culls a HUD moby on.
   MEASURED sprite geometry, useful for any future tweak: an icon is ~36 wide
   centred on its position and draws ~9 lines BELOW it, standing ~20 tall.
5. ~~`DrawSync` stall~~ **CLOSED 2026-08-20 — IT COSTS NOTHING. Do not
   revisit.** Pass 2 was successfully given its own primitive buffer and its
   own ordering table inside the other frame buffer (verified rendering
   correctly through dragon releases and normal play), and the inter-pass
   DrawSync was then removed. MEASURED: 18 FPS by the homeworld waterfalls
   before, 18 FPS after — no gain whatsoever — plus a freeze when retrying a
   flight level. WHY: the mod is CPU-bound (the four-viewport probe measured
   exactly half framerate, i.e. geometry-dominated), so the GPU had already
   finished viewport 1 long before the CPU wanted the buffer. The wait was
   almost free. Reverted to shared scratch.
   Layout notes kept for reference: world OT 0x4000, HUD OT 8 bytes, poly
   buffer 0x1C000 per frame buffer, tables must go at the BOTTOM of a borrowed
   buffer (at the top, a dense scene overflowed primitives into the table and
   froze the GPU).
   Original attempt notes follow — **ATTEMPTED AND REVERTED 2026-08-19/20.** The plan came
   from spyro-1's `AllocateBuffers` (src/4BEF8.c), which shows the engine
   ALREADY has a per-buffer ordering table field that the game never uses
   ("it's odd that they even have the OTs in the struct, they're always the
   same"), plus the exact sizes: world OT 0x4000 (0x800 bins x 8 bytes), HUD OT
   just 8 bytes, poly buffer 0x1C000 per frame buffer. LinkOTPrimitives zeroes
   bins as it links, so an OT self-clears after one memset.
   Pass 2 was given its own OT and primitives inside the OTHER buffer's
   0x1C000. **Result: coloured noise at 0 FPS.**
   WHY IT FAILED, from three lines further down the same function: the two
   poly buffers are ADJACENT, and `g_HudMobys = polyBuf + 0x1C000` — so the
   ACTIVE frame's HUD mobys live at the START of the other buffer. It is NOT
   idle, and pass 2's primitives landed on top of them.
   ANY RETRY must first establish where the HUD mobys end, and carve below
   that. The rest of the plan (per-pass OT) is sound and the addresses are
   above.
   Original note follows — **Spyromain's fix does NOT transfer. Measured
   2026-08-18 at the env init (0x8005b780-0x8005b7a8): the two frame envs
   have SEPARATE primitive buffers (env0 0x80076f50, env1 0x80076fd4 — stored
   from different registers) but the SAME ordering table: otbase (+0x74) and
   otslot (+0x78) are stored from the same register into both envs. Spyro 2
   gives pass 2 the other env's whole scratch; Spyro 1 has only one OT, and
   pass 2's SetupFrameOT would clear the bins the GPU is still walking —
   which is exactly what the stall prevents.** Remaining option if ever
   wanted: carve a second OT out of the tail of env B's 0x1C000 primitive
   buffer (bins are 8 bytes each x SP1_OT_DEPTH 0x800 = 16 KB needed; no
   scratch region has that) and repoint g_pOtDepthBinArrayBase (0x80075820)
   + g_pOtActiveDepthSlot (0x8007581c) for pass 2. Non-trivial: otslot's
   exact role is not yet understood. Payoff unquantified.
6. ~~Side-by-side (vertical) split~~ **DONE 2026-08-19**, confirmed working.
   Built as a RUNTIME MODE, not a compile-time constant, because the planned
   pause menu will just write into it: `SP1X2_SPLIT_MODE` at 0x8000E5C0
   (anything != 1 means horizontal, so uninitialised BIOS3 falls back to the
   proven layout). `Sp1x2SetViewport()` expresses both layouts as offsets from
   the game's own clip rect — same three knobs with X substituted for Y — and
   `Sp1x2SquashView()` halves matrix row 0 (camera X) for vertical instead of
   row 1 (camera Y). Adding 3/4-player quadrants = more cases in those two
   small functions.
   TEMPORARY toggles (remove when the pause menu lands): SELECT on pad 1
   flips the orientation, SELECT on pad 2 toggles widescreen. SELECT also
   opens the inventory, so it takes a second press to close it — accepted as
   easier to hit than a four-shoulder combo on a keyboard.
   ASPECT NOTE: each half is a skinny 256x224, and the X squash compresses the
   image to fit — same see-everything-but-distorted trade as the horizontal
   split's Y squash. Widescreen is where aspect gets addressed properly.
6c. **Widescreen edge pop-in (known limitation, 2026-08-19).** With widescreen
   ON at the widest fit, distant terrain and portal doors blink in and out at
   the left/right edges: we draw a wider view than the game's visibility logic
   expects. TRIED AND REVERTED TWICE: scaling the pure camera matrix
   (g_Camera+0x14) as well. It reduced the popping but produced blocky green
   distant geometry, because that matrix is NOT a cull matrix — r_environment
   loads it into the GTE and transforms far world geometry with it
   (0x80025934, spyro-1 comments it "Load in the camera matrix"), and
   r_cyclorama/r_shadows read it too.
   PROPER APPROACH if ever wanted: find r_environment's actual per-chunk
   visibility test and widen THAT, rather than the matrix it draws with.
   Milder than the cure so far — widescreen is optional and the artefact only
   shows at the widest settings. PARKED 2026-08-19 by user agreement.
6d. **View fit DONE 2026-08-19** (`SP1X2_VIEW_FIT` at 0x8000E5D0):
   0 FULL (squash 1/2), 1 BALANCED (3/4), 2 CROPPED (no squash). Cycled by
   START on pad 2 (temporary — pad 2's START does not pause, since the pause
   check reads the live pad, which is always player 1's).
   MEASURED: each step toward a narrower view is worth about +1 FPS, since
   fewer chunks/actors pass culling and fewer pixels are filled. Real but
   modest — not enough to lower the overclock on its own. Worth re-measuring
   when the scene is drawn FOUR times for 3-4 player split, where the same
   proportional saving is larger in absolute terms.
6b. Widescreen (16:9 anamorphic) — DONE 2026-08-19, confirmed working
   (anamorphic: compress X by 3/4 on the render matrix, display stretches it
   back; set DuckStation aspect to 16:9 and leave its own Widescreen Rendering
   OFF). Original note follows. Separate mechanism from the split:
   it changes the PROJECTION (horizontal FOV) rather than where things are
   drawn, and it interacts with the vertical split since both scale X.
7. **3 and 4 player co-op — PERFORMANCE MEASURED 2026-08-19, and it is the
   wall.** A throwaway probe built the scene an extra time per pass (CPU cost
   of four viewports, fill cost unchanged — correct, since four quarter
   viewports fill the same pixels as two half ones):
     no overclock, by the homeworld waterfalls: 18 FPS -> 9 FPS.
   EXACTLY half, so the cost is GEOMETRY/CPU and scales linearly with viewport
   count. Four players would therefore run at roughly half the current
   two-player framerate at the same overclock, and 300% is already the
   ceiling (the reference mod crashes above it).
   CONCLUSION: 4-player needs optimisation FIRST, not a multitap driver first.
   Levers, in order of expected value:
     (a) the parked `DrawSync` stall — CPU idles every frame waiting for the
         GPU; that budget is exactly what extra passes need
     (b) per-viewport culling — the CROPPED view fit is measurably cheaper
     (c) 3 players (1.5x) may be viable where 4 (2x) is not
   The rendering side is otherwise READY: `Sp1x2SetViewport` and
   `Sp1x2SquashView` were written to take quadrant cases.
   (Probe side note: at 300% overclock it stuttered to 0 FPS every few
   seconds — probe-specific, not investigated, gone with the probe.)
8. ~~Settings in the PAUSE menu~~ **DONE 2026-08-19.** OPTIONS -> SQUARE opens
   a MULTIPLAYER page with SPLIT / WIDESCREEN / VIEW / DONE, native-looking:
   the game's own text routine, its box, its letter wobble on the selected row
   (`g_HudMobys` rotations driven by COSINE_8 — note the table is SIGNED, an
   unsigned read spun letters backwards), and its menu chimes (menuCursor 45,
   menuConfirm 46 in the table at 0x800761d4).
   BUILT AS ITS OWN SUBSTATE (3) rather than extra rows in the options list,
   whose cursor bounds are compiled constants inside PauseMenu_Update.
   Hooks: 7 (the "PAUSED" text call — wrapping the whole pause draw was tried
   and MEASURED as too late, the frame is already composed), 8
   (PauseMenu_Update), 9-12 (the four main-menu item strings, suppressed while
   our page is open, because the stock draw renders the MAIN menu for any
   substate that is not 1 or 2), and two instruction patches so our substate
   gets the BIG box. Housekeeping (HudTick, SpecularUpdate(3)) must be called
   ourselves while we own the update, or the HUD and PAUSED freeze.
   Player count joins this page when 3/4-player rendering exists.
- [ ] Port the swap logic; two cameras rendering

Build with: `cd mod/projects/ntsc && make setup && make`
Full details and the gate's ongoing use: `SPYRO1_PORT_PLAN.md`.

## Folder layout

Project root is `~/Documents/Spyro 1 - Co-Op Mod/`. Note the spaces in the
path — always quote it in shell commands.

```
~/Documents/Spyro 1 - Co-Op Mod/
├── CLAUDE.md          <- this file
├── SPYRO1_PORT_PLAN.md <- stage 3 design + build-system plan
├── Roms/              <- pristine originals, never modified, gitignored
│   │                     (also holds the patched "Spyro 2x2.bin" — that one
│   │                      is for playing, NEVER for building from)
│   └── BIOS/          <- SCPH1001.BIN. BOTH emulators point here.
├── Spyro2x2/          <- Spyromain's repo (env.mk, projects/, scripts/, src/)
├── mod/               <- OUR Spyro 1 co-op mod. Builds; gate passes.
├── tools/             <- mkpsxiso 2.30 macOS binaries (symlinked onto PATH)
└── reference/           <- third-party Spyro 1 reverse-engineering references
    ├── open-spyro/                  <- splat decomp. Best symbol source.
    ├── spyro1-reverse-engineering/  <- C0mposer. Hand-named syms + Ghidra.
    └── spyro-1/                     <- TheMobyCollective. Not yet evaluated.
```

Nothing in `reference/` is ours and nothing there is built — they are read-only
references we grep for addresses. Keep our code out of that folder.

**If `Roms/BIOS/` is ever moved, TWO configs must be updated** (neither
emulator keeps its own copy of the BIOS):

- PCSX-Redux: `~/.config/pcsx-redux/pcsx.json` -> `emulator.Bios` and
  `emulator.BiosPath` (absolute paths). **Close PCSX-Redux first — it
  rewrites this file on quit and will undo your edit.**
- DuckStation: `~/Library/Application Support/DuckStation/settings.ini` ->
  `[BIOS] SearchDirectory` (path relative to that folder).

This matters more than it looks: if PCSX-Redux cannot find a real BIOS it may
silently fall back to **OpenBIOS**, which lays out the BIOS kernel region
completely differently — invalidating every finding in the free-RAM table
below without any visible error.

## macOS toolchain notes (hard-won — don't lose these)

There is no ready-made PSX MIPS compiler for macOS. Homebrew core has no
`mipsel-*-gcc` and no `mkpsxiso`. What worked on this machine (Apple Silicon,
Darwin 25.5 / macOS 26 SDK):

**Compiler** — community tap, GCC 14.2.0 (chosen over PCSX-Redux's GCC 16.1.0
because it's closer to what Spyromain likely built the official patch with):

```
brew tap namelocmas/mipsel-none-elf-gcc
brew trust namelocmas/mipsel-none-elf-gcc     # Homebrew 6 blocks untrusted taps
brew install mipsel-none-elf-gcc
```

Two local edits to the tap's formulae were required, both caused by the same
20-year-old zlib bug. Old zlib contains `#define fdopen(fd,mode) NULL` for
"Macs don't have fdopen" — untrue since Mac OS 9, and it mangles the macOS 26
SDK's `_stdio.h`. Fixed upstream by Mark Adler in Dec 2023. The tap predates
the fix, so in
`/opt/homebrew/Library/Taps/namelocmas/homebrew-mipsel-none-elf-gcc/Formula/`:

- `mipsel-none-elf-binutils.rb` — bumped binutils 2.43 -> **2.46.1** (its
  bundled zlib already has the fix) and added `depends_on "texinfo" => :build`.
- `mipsel-none-elf-gcc.rb` — added **`--with-system-zlib`** so GCC never builds
  its bundled zlib at all.

Re-run `brew trust` after editing a formula; trust is tied to file contents.
**If Homebrew ever updates this tap, these edits are overwritten** and the build
breaks again with `all-zlib ... Error 2`. Reapply them.

**mkpsxiso** — no formula, but the official release ships a universal macOS
binary. Kept in `tools/`, symlinked onto PATH so `env.mk` needs no absolute
paths (which would break anyway: this project's path contains spaces):

```
ln -sf "<project>/tools/mkpsxiso-2.30-Darwin/bin/mkpsxiso"  /opt/homebrew/bin/mkpsxiso
ln -sf "<project>/tools/mkpsxiso-2.30-Darwin/bin/dumpsxiso" /opt/homebrew/bin/dumpsxiso
```

**env.mk edits for macOS:** `CCPREFIX = mipsel-none-elf` (not
`mipsel-unknown-linux-gnu`) and `PYTHON = python3` (macOS has no bare `python`).

**`scripts/create_mkpsxiso_xml.py` patched** (our change to Spyromain's MIT
code — keep the attribution comment in the file). mkpsxiso 2.30's `dumpsxiso`
no longer writes a `source=` attribute on each `<file>`; the path is now
implied by `<directory_tree source="rom">` plus enclosing `<dir name="...">`.
The script assumed the old format and died with `KeyError: 'source'`. It now
walks the tree recursively to rebuild each path, and still accepts an explicit
`source=` if present, so it works with both old and new dumpsxiso. The sector
arithmetic is untouched.

## Environment

- macOS
- Dev/debug emulator: **PCSX-Redux — installed** at `/Applications/PCSX-Redux.app`
  (Dynarec OFF, Debugger ON, GDB Server ON, Web Server ON — set these in-app).
  Native arm64. No Homebrew cask and no GitHub release exists; the official
  download is a JavaScript page, so fetch the binary from the redirect:
  `curl -L -o PCSX-Redux-Arm.dmg "https://distrib.app/pub/org/pcsx-redux/project/dev-macos-arm/latest"`
  (the `/latest` suffix 302s to the real .dmg). Ad-hoc signed, so `spctl`
  rejects it — but curl sets no `com.apple.quarantine`, so it launches fine.
  If macOS ever does block it, right-click the app -> Open.
  **Ignore third-party mirrors (Uptodown/Softonic) — adware risk.**
- Play/verify emulator: DuckStation — installed and working
- CPU overclock up to 300% helps split-screen framerate. Do not exceed 300%;
  the reference mod crashes between levels above that.

## Build

From inside the relevant project directory (`reference/Spyro2x2/projects/rr` for
Ripto's Rage NTSC):

```
make setup    # extracts files from the ROM placed in disc/
make          # builds to build/disc/spyro2x2.bin
```

A **copy** of the original ROM, renamed `spyro2.bin`, goes in `projects/rr/disc/`
where `rom_file_goes_here.txt` lives. Always the original, never a patched ROM.

Edit `env.mk` with toolchain paths before the first build.

**Gotcha: `make` does not track the Makefile itself.** Editing a `-D` define in
`projects/rr/Makefile` (e.g. `SP2X2_DRAW_HMARGIN`) prints
`Nothing to be done for 'disc'` and rebuilds nothing, because the object-file
rules only depend on the `.c` sources. Run `make clean && make` after changing
any compiler flag. `make clean` only deletes `build/`; the extracted `rom/` and
`data/wad/` survive, so `make setup` does NOT need re-running.
Confirm a rebuild really happened by checking the .bin's SHA-1 changed.

**`SP2X2_DRAW_HMARGIN` is NOT a "gap between the two views" knob.** Tested
0xc -> 0x28: the game booted but the image flickered violently between two
split positions (~2/3 and ~1/4 down) and looked like the camera was
convulsing. Reverted. Why:

- `sp2_env_1` / `sp2_env_2` are the game's **two double-buffers**, swapped
  every frame (`Sp2x2Graphics.c:116`) — NOT one per player. Both players are
  drawn into whichever is current; player 2 via a copy of the draw env offset
  by `HEIGHT/2` (`Sp2x2Graphics.c:195-199`).
- The buffers sit at VRAM y = `HMARGIN` and y = `2*HMARGIN + HEIGHT`, so
  raising HMARGIN moves buffer 2 **twice as far** as buffer 1.
- The env struct is 116 bytes: DRAWENV 0-91, DISPENV 92-111. `SetDefDrawEnv`
  writes only 0-91, and **the mod never sets the DISPENV** — it just passes the
  game's original one to `PutDispEnv` (`Sp2x2Graphics.c:130`). So display
  positions are fixed by stock Spyro 2 while HMARGIN moves only where things
  are *drawn*. At 0xc they line up; change it and alternate frames land in
  different places -> per-frame jump.

HMARGIN is a fine alignment offset tied to the game's fixed display windows.
A genuinely wider divider means setting the DISPENV too — a real change, and a
good stage-2 project. VRAM does also cap things
(`2 * (HMARGIN + HEIGHT) <= 512`, so HMARGIN <= 0x28 at stock HEIGHT), but it
breaks well before that limit for the reason above.

## Verification gate

Before writing any mod code, confirm a from-source build of the *unmodified*
reference mod matches the SHA-1 of the officially patched .bin. If those
diverge, stop and fix the toolchain first — do not start debugging mod code on
an unverified build.

Both hashes confirmed locally with `shasum` on 2026-07-30:

| File                                     | SHA-1                                    |
| ---------------------------------------- | ---------------------------------------- |
| Source ROM, unpatched (Ripto's Rage NTSC) | b3a28f8c5ec02dfbd1f9bd47adbe1d75186a5d3f |
| **Officially patched .bin — BUILD TARGET** | 78ff615dab6bfccbbe7f4b421220008b1851d846 |

The build target is the number `build/disc/spyro2x2.bin` must match. It was
derived by applying Spyromain's official `spyro2x2_rr.xdelta` to the verified
source ROM above, and that patched file is confirmed working in DuckStation.

Check a build with:
`shasum "reference/Spyro2x2/projects/rr/build/disc/spyro2x2.bin"`

### Result of the first from-source build (2026-08-03)

**SHA-1 does not match, and that is expected. Do not treat it as failure.**
Our build: `9bfbae1ab9a198a864d1d15ebb2697960632a4ea`

A byte-level comparison explains every difference:

| Comparison                    | Bytes differing | Of total |
| ----------------------------- | --------------- | -------- |
| Official patch vs original ROM | 111,093,630     | 19.65%   |
| **Our build vs official patch** | **14,612**      | 0.0026%  |

Our build independently reproduced ~111 MB of Spyromain's changes (the whole
`WAD.WAD` repack and every shifted offset) and lands within 14 KB of his .bin.
The 13 differing regions are all in exactly two categories:

1. **Compiled code** — `SCUS_944.25` (LBA 24-198) and `WAD_072` / `WAD_074`
   inside `WAD.WAD`. This is our GCC 14.2.0 emitting different instructions
   than Spyromain's compiler from identical source. Unavoidable without his
   exact toolchain.
2. **Two ISO-metadata spots** (a KART directory record ~byte 514,387,146 and an
   XA area ~byte 537,168,618). Checked directly: **our build matches the
   pristine original byte-for-byte there and the official patch does not.**
   An mkpsxiso version artifact, not a defect in our build.

Spyro2x2 v1.0.0 was released 2025-09-09, when mkpsxiso 2.10 was current
(2.20 shipped Dec 2025). 2.10 has no macOS binary, so an exact match would
require building mkpsxiso 2.10 from source — and would still fail on the code
regions unless his GCC version were also identified. Not worth chasing.

**Therefore the gate for stage 1 is functional, not byte-exact:** does the
built ROM boot and split the screen? Re-verify with the byte comparison above
after any toolchain change — a sudden jump in differing bytes, or differences
outside these regions, means something really did break.

## Stage 3: the Spyro 1 port

### ROM version — VERIFIED COMPATIBLE (2026-08-04)

| File                                  | SHA-1                                    |
| ------------------------------------- | ---------------------------------------- |
| `Spyro the Dragon (USA).bin` (whole disc) | cf3ce6bedeb89dfbc40990336180f3b9b0f40d9f |
| `SCUS_942.28` extracted from it       | 84e3728ab94720d0873e2514adf4aade4935e0c5 |

That SCUS hash **exactly matches open-spyro's baseline**, so every address in
the reference repos is valid for our ROM as-is. No rebasing needed. Re-check
with `dumpsxiso -x <dir> "Roms/Spyro the Dragon (USA)/Spyro the Dragon (USA).bin"`
if the ROM is ever replaced.

### Scope of the port

`reference/Spyro2x2/src/c/Sp2.h` IS the port spec: 112 base-game symbols declared, 109
actually used, 9 of those standard PSX library. But usage is very top-heavy —
`sp2_spyro` alone is referenced 110x, `sp2_camera` 60x. Find the six below and
you have the backbone; the rest (sparx, flames, mobys) is per-feature and can
be added incrementally.

**First milestone is NOT "co-op Spyro 1" — it is "two cameras rendering one
level".** That needs only about five of these.

### Symbol map: Spyro 2 -> Spyro 1

| Spyro2x2 symbol   | Spyro 1 equivalent                        | Address      |
| ----------------- | ----------------------------------------- | ------------ |
| `sp2_spyro[708]`  | `_spyro`                                  | `0x80078A58` |
| `sp2_camera[500]` | `_cameraPosition`                         | `0x80076DF8` |
|                   | `_cameraAngle`                            | `0x80076E1C` |
|                   | `_cameraRotationMatrix`                   | `0x80076dd0` |
|                   | `_cameraPureRotationMatrix`               | `0x80076de4` |
| `sp2_env_1[116]`  | `g_abFrameDrawEnv0` (0x5c = DRAWENV)      | `0x80076ee0` |
|                   | `g_abFrameDispEnv0` (0x14 = DISPENV)      | `0x80076f3c` |
| `sp2_env_2[116]`  | `g_abFrameDrawEnv1`                       | `0x80076f64` |
|                   | `g_abFrameDispEnv1`                       | `0x80076fc0` |
| `sp2_current_env` | `g_pActiveFrameDrawEnv`                   | `0x80075888` |
| `sp2_game_state`  | `_gameState`                              | `0x800757D8` |
| `sp2_pad`         | `_currentButtonsHeld`                     | `0x80077380` |
|                   | `_currentButtonsPressed`                  | `0x80077378` |
| (none)            | `_secondController` — **Spyro 1 already reads port 2** | `0x80078E50` |
| `Sp2Graphics()`   | `GamestateDraw` / `GamestateUpdate` hooks | see open-spyro |

Note Spyro 1 splits the camera into separate globals rather than one struct,
so the `Sp2x2Swap(&sp2_camera, ...)` pattern becomes several swaps. Also note
the DISPENV is a **separately named symbol** in Spyro 1 — do not repeat the
Spyro 2 mistake of moving the draw env while leaving the display env behind
(see the HMARGIN note above).

### Reference repos (cloned, stage 3)

- `reference/open-spyro/` — splat decomp. `config/symbol_addrs.txt` = **613
  named functions + globals**, richest source. Matches our ROM exactly.
- `reference/spyro1-reverse-engineering/` — C0mposer. `symbols/symbols.txt` (112
  hand-named variables) and `symbols/funcs.txt` (70 funcs). Fewer but
  higher-confidence, human-readable names. Also a Ghidra project and a
  symbol-map website.
- `reference/spyro-1/` — TheMobyCollective. **Do not overlook this one.** ~18k
  lines of C across 151 files, and crucially it has real **STRUCT
  DEFINITIONS** (`include/spyro.h`, `moby.h`, ...) where open-spyro has only
  one symbol per address. Function names are `func_8004A200`-style, which made
  it look less useful at a glance — it is not.

**RESEARCH ORDER (user directive, 2026-08-20). Follow it before writing any
code:**
  1. **Spyro2x2 FIRST.** Spyromain solved this problem once already. Whatever
     we are about to invent, check whether he did it — his swap lists are the
     spec for what counts as per-player state. Re-read them with fresh eyes
     when stuck; the gaps in our port ARE the bug list. (Example: the flight
     steering hunt burned ~10 theories, and the answer came from noticing he
     swaps `sp2_calculation_results` while we swapped nothing equivalent.)
  2. **Then the spyro-1 decomp**, to find the Spyro 1 equivalent and its real
     behaviour.
  3. **Only then build and test.** Not before.

**USE `spyro-1` ONLY for decomp lookups (user directive, 2026-08-19). open-spyro has misled us
too many times; stop consulting it unless something is genuinely unavailable.**
This costs nothing, because spyro-1 names every function BY ITS ADDRESS
(`func_800521C0`), so it is its own address index — and its headers and
`src/**/*.c` comments say what each one DOES, which is the part open-spyro got
wrong. Its `asm/**/*.s` keep real global names too.
Names open-spyro got WRONG for us, each costing real time: `g_nFrameTicks`
(really the input ring index), `DrawSpyroHornStrikeTrails` (really the flame
renderer), `g_anCameraLatchedAnchorPos` (really the general player anchor),
`RasterizeEmitList` (really the PARTICLE renderer, func_800573C8), and the
"camera view matrix" at g_Camera+0x14 (a matrix the world renderer DRAWS with,
not a cull matrix). Treat any surviving open-spyro name in our symbols.ld as a
label of convenience, not a fact.

**Superseded — earlier research order (2026-08-18):** It has named functions in `include/*.h`, decompiled C in `src/`, and —
crucially — `asm/**/*.s` files that keep REAL SYMBOL NAMES. One grep of
`asm/renderers/r_flame.s` for `%hi(...)` produced the flame renderer's complete
global footprint in seconds, which no amount of raw disassembly had managed.
Use open-spyro only as an ADDRESS INDEX (it has 613 named addresses spyro-1
leaves as `func_8004A200`), and treat any open-spyro NAME as a hypothesis until
confirmed — its names have misled us repeatedly (g_nFrameTicks was really the
input ring index; DrawSpyroHornStrikeTrails is really the flame renderer;
g_anCameraLatchedAnchorPos is really the general player anchor).

**USE BOTH. They are complementary, and assuming otherwise cost hours:**

| Need | Use |
| ---- | --- |
| "what lives at address X" | `open-spyro/config/symbol_addrs.txt` (613 named) |
| "what SHAPE is this data" | `spyro-1/include/*.h` (structs, sizes, field docs) |
| readable logic | both — check each, coverage differs per function |

Concrete example: Spyro's state is **three structs**
(`g_Spyro` `0x80078A58` size `0x2a4`, `g_SpyroFlame` `0x800786C8` size `0x138`,
`SpyroShadow` `0x8007AA10` size 40), confirmed by a `static_assert` in
`spyro.h`. open-spyro names several fields INSIDE `g_Spyro` as though they were
camera globals (`g_nCameraScriptStickRequestY`, `g_pScriptedCameraActor`) —
they are camera-related fields of Spyro, not camera state. Grouping by name
therefore produced a swap with 112 bytes of holes in the middle of the struct.

### How to inject code — SOLVED IN PRINCIPLE (2026-08-04)

**Do NOT copy C0mposer's approach.** Their `mods/` places code in extra 8 MB
developer RAM (`config.json` has `"8mb": 1`) and hooks by overwriting a
function's first instruction. That is a *testing* technique — real PS1s and
default emulator configs have 2 MB, so it cannot ship.

**Copy Spyromain's approach instead.** Two separate mechanisms:

**1. Hooking = rebuild the executable with surgical instruction replacement.**
`projects/rr/src/asm/main.S` is a binary patcher written as assembly:

```asm
.incbin "rom/SCUS_944.25", 0x800, 0x1afc    # original bytes verbatim
/* 0x80011afc */ jal Sp2x2Graphics           # ONE instruction replaced
.incbin "rom/SCUS_944.25", 0x2300, 0x2a0    # original bytes resume
```

The output is byte-identical to retail except at the replaced instructions.
`header.S` is just `.incbin "rom/SCUS_944.25", 0, 0x80` — the real PS-EXE
header, preserved so the disc still boots.

**2. Code bodies live in the PS1 BIOS scratch region**, not in the game's
memory. From `spyro2x2.ld`:

| Region   | Origin       | Length   | Delivered via              |
| -------- | ------------ | -------- | -------------------------- |
| `LOADER` | `0x8000B070` | `0x800`  | SCUS header padding (0x80-0x7FF) |
| `BIOS2`  | `0x8000C000` | `0x1F00` | WAD slot 72                |
| `BIOS3`  | `0x8000E400` | `0x1C00` | WAD slot 74                |
| `RAM`    | `0x80010000` | `0x1F0000` | the game itself          |

~18 KB total. The gaps he left (`0x8000B870`-`0x8000C000`,
`0x8000DF00`-`0x8000E400`) presumably ARE used by something — respect them.

**Why this transfers to Spyro 1:** the BIOS region is a property of the
PlayStation, not the game. Confirmed from open-spyro's splat config that
Spyro 1 also loads at `0x80010000` and the lowest known symbol is
`0x80010a70` — **nothing below `0x80010000`**, same as Spyro 2.

**CODE CEILING IS 0x8000E000 — VERIFIED THE HARD WAY (2026-08-19).** Growing
the BIOS2 payload from 0x2000 to 0x2800, so code ran to 0x8000E800, BLACK-
SCREENED the game immediately after the PlayStation logo. Every size agreed
(t_size 0x68000 = 208.0 sectors, boot copy, linker LENGTH), so it was not the
sector-alignment trap — it is that 0x8000E000-0x8000E800 is genuinely in use,
exactly as the note below about Spyromain's gaps suspected. **Keep CODE inside
0x8000C000-0x8000E000.** Runtime DATA above 0x8000E400 is fine (proven for
months). When BIOS2 fills up again, trim code or move a whole object into
LOADER — do NOT grow the payload.

**Verified on Spyro 1 in PCSX-Redux, 2026-08-04** (real SCPH1001 BIOS,
dynarec off), checked at the title screen AND in-level in Artisans Home:

| Address      | Finding                                           |
| ------------ | ------------------------------------------------- |
| `0x8000B070` | ASCII `PS-X EXE` — the BIOS header buffer. Proven. |
| `0x8000B0F0` | header padding text — dead after load. **FREE**    |
| `0x8000B870` | `E0 B8 05 80` = Spyro 1's entry point — the BIOS **EXEC struct. IN USE, hard ceiling.** |
| `0x8000C000` | zeros at title AND in-level. **FREE**              |
| `0x8000DF00` | zeros at title AND in-level. **FREE**              |
| `0x8000E400` | zeros at title AND in-level. **FREE**              |

~18 KB available. Spyromain's memory map ports over essentially unchanged.
Not exhaustively tested — FMV, the `S0/CRASH.EXE` demo, memory-card access and
other levels could still touch it. Re-check these addresses if a mystery crash
appears later.

**Still to settle:** delivery for `BIOS2`/`BIOS3` (~16 KB). `LOADER`'s 1,920
bytes need no delivery mechanism at all — the BIOS copies the EXE header there
for free — and that is enough for milestone 1. See `SPYRO1_PORT_PLAN.md`.

## Reference material

- Spyro2x2 source: https://github.com/Spyromain/Spyro2x2 (MIT)
- Toolchain: https://github.com/mateusfavarin/psx-modding-toolchain
- Spyro 1 decomp: https://github.com/theMagicalKarp/open-spyro
- Spyro 1 decomp: https://github.com/TheMobyCollective/spyro-1
- Spyro 1 symbols/Ghidra: https://github.com/C0mposer/spyro1-reverse-engineering
- Memory inspector: https://github.com/FranklyGD/Spyro-Scope
- Community: "Mod the Dragon" Discord (invite in the Spyro2x2 README)

## Rules

- **Never commit ROM files, .bin, .cue, or extracted game assets.** Source,
  build scripts, and patches only. Check .gitignore before adding files.
- Spyromain's code is MIT — preserve attribution in any derived files.
- Spyro 3 (Year of the Dragon) has anti-tamper that checksums the memory
  regions we would inject into, with deliberately delayed and misleading
  symptoms. Do not target Spyro 3 without addressing that first.
