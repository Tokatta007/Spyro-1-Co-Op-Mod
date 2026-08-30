#include "Sp1.h"

/*
 * Spyro 1 co-op: boot stub — the "door" into BIOS2.
 *
 * THE PROBLEM. Our code has to live in RAM the game does not use. Three such
 * regions exist (see CLAUDE.md), but only LOADER has a way in: the BIOS copies
 * the executable's 2 KB header there for free, so code hidden in the header's
 * padding is simply present. BIOS2 (0x8000C000, 7936 bytes) and BIOS3
 * (0x8000F000, 7168) are equally free but nothing puts anything in them.
 * LOADER alone was nearly exhausted at 1816/2048 bytes, and Spyromain's
 * finished Spyro 2 mod needed 16904 bytes across all three.
 *
 * THE SOLUTION. Ride along with the executable, then relocate at boot:
 *
 *   1. The .bios2 section is appended to SCUS_942.28 on disc, and the PS-EXE
 *      header's t_size (file offset 0x1C) is enlarged to cover it. The BIOS
 *      therefore loads it into RAM for us, at 0x80010000 + 0x65800 = 0x80075800.
 *   2. The header's entry point (file offset 0x10) is redirected here instead
 *      of the game's real entry.
 *   3. We copy the payload up to 0x8000C000, flush the instruction cache, and
 *      jump to the real entry. The game then starts normally, unaware.
 *
 * WHY 0x80075800 IS SAFE, despite being where the game's .bss begins. Nothing
 * has run yet: the BIOS loads the file and jumps straight here, so .bss is
 * still untouched. We copy the payload out before handing over. Any later
 * point would be too late.
 *
 * WHY FlushCache MATTERS. We WRITE instructions and later EXECUTE them. The
 * R3000's instruction cache does not observe writes to memory, so without a
 * flush the CPU can run whatever stale bytes it cached from those addresses.
 *
 * Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
 */

/* Where the BIOS parks the appended payload: t_addr + the original t_size. */
#define SP1X2_PAYLOAD_SRC   ((volatile unsigned int *)0x80075800)
#define SP1X2_PAYLOAD_DST   ((volatile unsigned int *)0x8000C000)
/* 0x2000, NOT more. Growing this to 0x2800 put code at 0x8000E000-0x8000E800
   and the game BLACK-SCREENED right after the PlayStation logo — that range
   is inside the region CLAUDE.md warns is used by something (Spyromain left
   0x8000DF00-0x8000E400 alone for a reason). 0x8000C000-0x8000E000 is proven;
   treat it as the hard ceiling for CODE. */
#define SP1X2_PAYLOAD_WORDS (0x2000 / 4)   /* 4 whole sectors */

/* The game's real entry point, from the untouched header (PC at offset 0x10). */
#define SP1_ORIGINAL_ENTRY  0x8005b8e0


void Sp1x2Boot(void)
{
    volatile unsigned int *src = SP1X2_PAYLOAD_SRC;
    volatile unsigned int *dst = SP1X2_PAYLOAD_DST;
    int i;

    /* 0x2000, a whole number of 2048-byte sectors. t_size must be a sector
       multiple or the BIOS hangs at the boot logo before anything runs. */
    for (i = 0; i < SP1X2_PAYLOAD_WORDS; i++) {
        dst[i] = src[i];
    }

    /* SECOND CHUNK -> BIOS2B (2026-08-23). The old black-screen came from
       copying THROUGH 0x8000E000-0x8000E400 — the kernel's event/thread
       tables (finally identified from a live TCB dump). This copy SKIPS
       them: 0x8000E600-0x8000E7FF is inside the BIOS3 area our runtime data
       has used safely for months. 512 bytes of code space. */
    /* TWO-STAGE (2026-08-23): chunk A (BIOS2) is copied above; chunks B
       (BIOS2B) and C (BIOS2C) are copied by Sp1x2BootStage2, which lives
       INSIDE chunk A — so this stub stays within LOADER. The first flush
       makes stage 2 executable; the second covers the code it copied. */
    FlushCache();
    Sp1x2BootStage2();
    FlushCache();

    ((void (*)(void))SP1_ORIGINAL_ENTRY)();
}
