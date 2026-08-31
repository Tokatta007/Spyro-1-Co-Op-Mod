/*
 * coop.c - proof that the Spyro 1 Co-Op Mod can live inside the decompilation.
 *
 * This is a TEST, not the mod. It reproduces the very first thing the mod ever
 * did back in August: park a marker in BIOS scratch RAM and count frames beside
 * it. Then, the marker took a patched instruction, a boot stub and a hand-built
 * memory map to arrive. Here it is an ordinary function called by name from
 * main(), because we have the source.
 *
 * Verify in PCSX-Redux (NOT DuckStation - this needs the memory viewer):
 *   0x8000C000 should read DEADBEEF
 *   0x8000C004 should climb continuously while the game runs
 */

#include "common.h"

void Sp1x2Frame(void) {
  *(volatile unsigned int *)0x8000C000 = 0xDEADBEEF;
  *(volatile unsigned int *)0x8000C004 += 1;
}
