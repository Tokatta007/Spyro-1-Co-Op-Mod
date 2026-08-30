CCPREFIX = mipsel-none-elf
CC = $(CCPREFIX)-gcc
LD = $(CCPREFIX)-ld
OBJCOPY = $(CCPREFIX)-objcopy
OBJDUMP = $(CCPREFIX)-objdump
# -Os, not -O2: our code lives in BIOS scratch RAM and BOTH regions were
# nearly full (BIOS2 down to 204 bytes free, LOADER to 124). -Os recovered
# ~1160 bytes in BIOS2 and ~96 in LOADER with no behaviour change. The code
# ceiling at 0x8000E000 cannot be raised -- growing the payload black-screens
# the game -- so codegen size is one of the few levers we have.
# NOTE: the Makefile does not track flag changes. `make clean` after editing.
# -ffunction-sections/-fdata-sections added 2026-08-28: each function gets its
# own section, so the linker packs them without per-object alignment padding.
# VERIFIED SAFE: a symbol-by-symbol diff before/after showed NOTHING dropped.
# Worth 152 bytes, all of it in BIOS2B. (--gc-sections was NOT added: with no
# conventional entry point it has no root set and could discard everything.)
CFLAGS = -fno-builtin -Os -ffunction-sections -fdata-sections -nostdlib -EL -march=r3000 -mno-abicalls -msoft-float -G0

PYTHON = python3

CP = $(PYTHON) ../../scripts/cp.py
MKDIR = $(PYTHON) ../../scripts/mkdir.py
RM = $(PYTHON) ../../scripts/rm.py

DUMPSXISO = dumpsxiso
MKPSXISO = mkpsxiso
