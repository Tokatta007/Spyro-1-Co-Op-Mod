/*
 * PS-EXE header for the rebuilt SCUS_942.28, with TWO fields patched.
 *
 * The PlayStation BIOS reads the first 0x800 bytes of an executable into a
 * kernel buffer at 0x8000B070 and leaves it there. Only the first 0x80 bytes
 * are real header fields; the rest is the region marker string plus padding,
 * which nothing reads after load. That padding is where LOADER code lives.
 *
 * Header layout (little-endian words):
 *   0x00  "PS-X EXE" magic + padding
 *   0x10  PC      entry point          <- PATCHED to Sp1x2Boot
 *   0x14  GP
 *   0x18  t_addr  load address (0x80010000)
 *   0x1C  t_size  bytes to load        <- PATCHED to cover the .bios2 payload
 *   0x20  ... region marker string, then padding
 *
 * WHY: BIOS2 (0x8000C000) is free RAM but nothing loads anything into it.
 * Enlarging t_size makes the BIOS load our appended .bios2 payload into RAM
 * as part of the executable, and redirecting PC lets Sp1x2Boot relocate it to
 * 0x8000C000 before the game starts. See src/c/Sp1x2Boot.c.
 *
 * Confirmed on Spyro 1 in PCSX-Redux: "PS-X EXE" is present at 0x8000B070.
 *
 * Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
 */

.incbin "rom/SCUS_942.28", 0, 0x10      /* magic + padding                   */
.word   Sp1x2Boot                       /* 0x10 PC   -> our boot stub        */
.incbin "rom/SCUS_942.28", 0x14, 0x8    /* 0x14 GP, 0x18 t_addr — unchanged  */
.word   0x68000                         /* 0x1C t_size = 0x65800 + 0x2800.
                                           MUST stay a multiple of 2048: the
                                           BIOS loads whole sectors and hangs
                                           at the boot logo otherwise.        */
.incbin "rom/SCUS_942.28", 0x20, 0x60   /* marker string + padding           */
