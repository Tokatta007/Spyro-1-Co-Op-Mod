/*
 * Memory layout for the Spyro 1 co-op mod.
 *
 * Every region below was verified by hand in PCSX-Redux on 2026-08-04 with a
 * real SCPH1001 BIOS, checked both at the title screen and in-level. See the
 * table in CLAUDE.md.
 *
 * Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
 */

INCLUDE "symbols.ld"

MEMORY {
    /* The BIOS reads the executable's 2 KB header here and leaves it. Only
       the first 0x80 bytes are real header fields, so 0x80..0x7FF is free.
       CONFIRMED: "PS-X EXE" is present at 0x8000B070 on Spyro 1.

       HARD CEILING: 0x8000B870 holds the BIOS EXEC struct (its first word is
       Spyro 1's entry point, 0x8005b8e0). LENGTH is exactly 0x800 so the
       linker errors out rather than silently trampling it. Do not raise it. */
    LOADER : ORIGIN = 0x8000B070, LENGTH = 0x800

    /* Unused BIOS kernel RAM. Zeros at the title screen AND in-level.
       No delivery mechanism yet — unlike LOADER, the BIOS does not fill
       these for us. See SPYRO1_PORT_PLAN.md. Unused until then. */
    /* 0x2000, not Spyromain's 0x1F00: the payload is appended to the
       executable and the PS1 BIOS loads executables in whole 2048-byte
       sectors, so t_size MUST be a sector multiple. 0x1F00 is 3.875 sectors
       and the console hangs at the boot logo. 0x2000 is exactly 4.
       0x8000C000 + 0x2000 = 0x8000E000, clear of BIOS3 at 0x8000E400, and
       0x8000DF00 was verified free (zeros at the title screen AND in-level). */
    BIOS2 : ORIGIN = 0x8000C000, LENGTH = 0x2000
    /* Second boot-copied code region: AFTER the kernel's event/thread tables
       (0x8000E000-0x8000E3FF, identified live from a TCB dump). Delivered as
       payload chunk B by Sp1x2Boot.
       GREW DOWN 2026-08-27 from 0x8000E600/0x200 to 0x8000E400/0x400: the
       0x8000E400-0x8000E5FF marker block held only diagnostic counters, all
       long retired (the moby chain sweep, the focus-repair count). The
       region has been read reliably across many sessions, so it is not
       clobbered — code is as safe here as the data was. Note this is NOT the
       BIOS2C mistake: that was 0x8000EC30, which the game DOES clobber
       during level loads. The payload already carried 0x400 of slack, so
       t_size and the sector padding are unchanged. */
    BIOS2B : ORIGIN = 0x8000E400, LENGTH = 0x400
    BIOS3 : ORIGIN = 0x8000E800, LENGTH = 0x1800

    /* The game itself. t_addr = 0x80010000, t_size = 0x65800, from the
       PS-EXE header. Length is exact so an accidental overflow is an error
       rather than a corrupt ROM. */
    RAM : ORIGIN = 0x80010000, LENGTH = 0x65800
}

SECTIONS
{
    /* File offset 0. header.o is the real 0x80-byte PS-EXE header; everything
       after it is mod code living in the header's padding, which the BIOS
       parks at 0x8000B0F0. LOADER's LENGTH caps this at 0x800 total — if you
       overflow it the linker errors rather than trampling the EXEC struct at
       0x8000B870. */
    .header : AT(0) {
        build/src/asm/header.o(.text .text.*)
        build/src/c/Sp1x2Boot.o(.text .text.*)
        build/src/c/Sp1x2Pad.o(.text .text.*)
        build/src/c/Sp1x2Pad.o(.rodata*)
    } > LOADER

    /* File offset 0x800. The game body, with hooks patched in by main.S. */
    .main : AT(0x800) {
        build/src/asm/main.o(.text .text.*)
    } > RAM

    /* Appended to the executable at file offset 0x66000, loaded by the BIOS
       to 0x80075800 along with the game, then relocated here by Sp1x2Boot
       before the game starts. See src/c/Sp1x2Boot.c.

       Padded to the full region so its size is a compile-time constant —
       header.S has to state the total in t_size, and a variable payload size
       would make that circular. Wasted disc space only. */
    .bios2 : AT(0x66000) {
        build/src/c/Sp1x2Graphics.o(.text .text.*)
        build/src/c/Sp1x2Spyro.o(.text .text.*)
        build/src/c/Sp1x2Menu.o(.text .text.*)
        . = ORIGIN(BIOS2) + LENGTH(BIOS2);
    } > BIOS2

    .bios2b : AT(0x68000) {
        /* BIOS2's READ-ONLY DATA LIVES HERE (2026-08-28). Moving the three
           .rodata blocks out of BIOS2 buys 240 bytes in the region that is
           actually full, and costs nothing: -G0 means every global is
           addressed absolutely, so relocating data changes a constant in an
           instruction, never the instruction count. Verified — the functions
           are byte-identical either side of this change. */
        build/src/c/Sp1x2Graphics.o(.rodata*)
        build/src/c/Sp1x2Spyro.o(.rodata*)
        build/src/c/Sp1x2Menu.o(.rodata*)
        build/src/c/Sp1x2Sparx.o(.text .text.*)
        build/src/c/Sp1x2Sparx.o(.rodata*)
        build/src/c/Sp1x2Gates.o(.text .text.*)
        build/src/c/Sp1x2Gates.o(.rodata*)
        . = ORIGIN(BIOS2B) + LENGTH(BIOS2B);
    } > BIOS2B


    /* Runtime scratch only (markers, player-2 shadow state) — written at
       runtime, so it needs no delivery. Not yet used for code. */
    .bios3 : { } > BIOS3
}
