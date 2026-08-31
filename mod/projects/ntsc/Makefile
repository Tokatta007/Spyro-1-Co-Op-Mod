include ../../env.mk

# Spyro 1 (NTSC, SCUS_942.28) co-op mod.
# Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
#
# Usage:
#   make setup    extract the ROM (run once; needs disc/spyro1.bin)
#   make          build the executable and run the byte-identical gate
#   make disc     repack a full bootable .bin/.cue
#   make clean    delete build/ (extracted rom/ survives)

# Number of instructions replaced in main.S. Each hook changes EXACTLY 4 bytes
# of the executable body. Bump this when you add a hook.
EXPECTED_HOOKS = 24

# Bytes appended to the executable for the BIOS2 payload (see Sp1x2Boot.c).
# Must match the .bios2 region size and the t_size in header.S.
PAYLOAD_BYTES = 10240  # 0x2800 = 5 sectors; MUST be a 2048 multiple

MAINOBJ = build/src/asm/header.o \
	build/src/asm/main.o \
	build/src/c/Sp1x2Graphics.o \
	build/src/c/Sp1x2Pad.o \
	build/src/c/Sp1x2Boot.o build/src/c/Sp1x2Ground.o \
	build/src/c/Sp1x2Spyro.o build/src/c/Sp1x2Sparx.o \
	build/src/c/Sp1x2Gates.o \
	build/src/c/Sp1x2Menu.o

# Spyro 1's disc is far simpler than Spyro 2's 197 WADs. S0/ is a playable
# Crash Bandicoot demo — irrelevant to the mod, but it must be preserved.
DISCLIST = build/rom/license_data.dat \
	build/rom/SYSTEM.CNF \
	build/rom/SCUS_942.28 \
	build/rom/WAD.WAD \
	build/rom/PETEXA0.STR \
	build/rom/PETEXA1.STR \
	build/rom/PETEXA2.STR \
	build/rom/PETEXA3.STR \
	build/rom/PETEXA4.STR \
	build/rom/PETEXA5.STR \
	build/rom/S0/CRASH.EXE \
	build/rom/S0/WARP.BIN \
	build/rom/S0/S000000D.NSD \
	build/rom/S0/S000000D.NSF \
	build/rom/S0/S000003C.NSD \
	build/rom/S0/S000003C.NSF \
	build/rom/SOURCE/SOURCE.TRD


.PHONY: all
all: verify


# ---- the verification gate -------------------------------------------------
# With zero hooks applied, the rebuilt executable MUST be byte-identical to
# retail. That single check proves header.S, main.S, the linker script and
# objcopy are all correct, BEFORE any mod code exists to muddy the diagnosis.
# Once hooks are added this will fail by design — that is the point at which
# you switch to checking that ONLY the intended bytes changed.

.PHONY: verify
verify: build/rom/SCUS_942.28
	@echo "--- hook gate ---"
	@orig=$$(wc -c < rom/SCUS_942.28 | tr -d ' '); \
	new=$$(wc -c < $< | tr -d ' '); \
	want=$$((orig + $(PAYLOAD_BYTES))); \
	if [ "$$new" -ne "$$want" ]; then \
		echo "  FAIL - size is $$new, expected $$want"; \
		echo "         ($$orig original + $(PAYLOAD_BYTES) payload)"; \
		echo "  Either a .incbin length in main.S is wrong, or the payload"; \
		echo "  size disagrees with t_size in header.S."; exit 1; \
	else echo "  size: $$new = $$orig original + $(PAYLOAD_BYTES) payload"; fi
	@hex=$$($(OBJDUMP) -h build/spyro1-coop.elf | awk '$$2 == ".header" {print $$3}'); \
	used=$$((16#$$hex)); \
	echo "  LOADER region: $$used of 2048 bytes used ($$((2048 - used)) free)"
	@orig=$$(wc -c < rom/SCUS_942.28 | tr -d ' '); \
	body=$$(cmp -l -n $$orig rom/SCUS_942.28 $< 2>/dev/null | awk '$$1 > 2048' | wc -l | tr -d ' '); \
	max=$$(($(EXPECTED_HOOKS) * 4)); \
	echo "  body bytes changed: $$body (max $$max = $(EXPECTED_HOOKS) hook(s) x 4)"; \
	if [ "$$body" -gt "$$max" ]; then \
		echo "  FAIL - more bytes changed than $(EXPECTED_HOOKS) hook(s) can explain."; \
		echo "         A .incbin length is almost certainly wrong, shifting"; \
		echo "         everything after it. First few:"; \
		cmp -l -n $$orig rom/SCUS_942.28 $< | awk '$$1 > 2048' | head -5; \
		exit 1; \
	elif [ "$(EXPECTED_HOOKS)" -gt 0 ] && [ "$$body" -eq 0 ]; then \
		echo "  FAIL - expected hooks but nothing changed. Did main.S apply?"; \
		exit 1; \
	else \
		echo "  PASS - only the intended instructions changed"; \
		echo "  (fewer than 4 bytes per hook is normal: bytes shared between"; \
		echo "   the old and new instruction encodings do not differ)"; \
	fi
	@echo "--- build verification ---"
	@$(PYTHON) ../../scripts/verify.py


# ---- executable ------------------------------------------------------------

build/rom/SCUS_942.28: build/spyro1-coop.elf
	@$(MKDIR) -p $(@D)
	$(OBJCOPY) -O binary -j .header -j .main -j .bios2 -j .bios2b --pad-to=0x68800 $< $@

build/spyro1-coop.elf: coop.ld symbols.ld $(MAINOBJ)
	@$(MKDIR) -p $(@D)
	$(LD) --orphan-handling=discard -T $< -o $@

# The .S files .incbin the original executable, so they depend on it.
build/src/asm/%.o: src/asm/%.S rom/SCUS_942.28
	@$(MKDIR) -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

build/src/c/%.o: ../../src/c/%.c
	@$(MKDIR) -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@


# ---- disc ------------------------------------------------------------------

.PHONY: disc
disc: build/disc/spyro1-coop.bin

build/disc/spyro1-coop.bin: build/spyro1-coop.xml $(DISCLIST)
	@$(MKDIR) -p $(@D)
	$(MKPSXISO) -y -lba build/spyro1-coop.lba -c build/disc/spyro1-coop.cue -o $@ $<

build/spyro1-coop.xml: ../../scripts/create_mkpsxiso_xml.py spyro1.xml $(DISCLIST)
	@$(MKDIR) -p $(@D)
	$(PYTHON) $< spyro1.xml $@

build/rom/%: rom/%
	@$(MKDIR) -p $(@D)
	$(CP) $< $@


# ---- release patch ---------------------------------------------------------
#
# What gets distributed. An xdelta patch encodes only the DIFFERENCE between
# the player's own disc and the modded one, so it carries no game data and is
# safe to share — the player supplies the game.
#
# The target verifies itself: it applies the patch it just made to a fresh
# copy of the source disc and checks the result matches the built disc
# byte-for-byte. A patch that does not round-trip is worse than no patch,
# because it fails on someone else's machine after they have downloaded it.

.PHONY: patch
patch: build/release/spyro1-coop.xdelta

build/release/spyro1-coop.xdelta: build/disc/spyro1-coop.bin disc/spyro1.bin
	@$(MKDIR) -p $(@D)
	@echo "--- building patch ---"
	xdelta3 -e -9 -f -s disc/spyro1.bin build/disc/spyro1-coop.bin $@
	@echo "--- verifying it round-trips ---"
	@xdelta3 -d -f -s disc/spyro1.bin $@ build/release/roundtrip.bin
	@a=$$(shasum build/disc/spyro1-coop.bin | cut -d' ' -f1); \
	 b=$$(shasum build/release/roundtrip.bin | cut -d' ' -f1); \
	 $(RM) -f build/release/roundtrip.bin; \
	 if [ "$$a" != "$$b" ]; then \
		echo "  FAIL - patched disc does not match the built disc"; \
		echo "         built:   $$a"; \
		echo "         patched: $$b"; exit 1; \
	 fi; \
	 echo "  PASS - patch reproduces the build exactly ($$a)"
	@$(CP) build/disc/spyro1-coop.cue build/release/spyro1-coop.cue
	@src=$$(shasum disc/spyro1.bin | cut -d' ' -f1); \
	 sz=$$(wc -c < $@ | tr -d ' '); \
	 echo "  patch:  $@ ($$sz bytes)"; \
	 echo "  source: expects a disc with SHA-1 $$src"



# ---- setup / clean ---------------------------------------------------------

.PHONY: setup
setup: disc/spyro1.bin
	$(DUMPSXISO) -x rom -s spyro1.xml $<

.PHONY: clean
clean:
	$(RM) -rf build
