"""Post-build verification for the Spyro 1 co-op mod.

Run automatically by `make`. Everything here exists because something once
went wrong in a way the previous checks could not see:

  WHERE, not just how many.  The old gate counted changed bytes and compared
    the total against EXPECTED_HOOKS * 4. That bounds how MANY bytes moved,
    never WHERE — so two hooks placed four bytes off (landing in the delay
    slots of the jals they meant to replace) passed cleanly and produced a
    soft-lock that took a full debugging session to find. Check 2 now requires
    every changed byte to fall inside a declared hook's own four bytes.

  Hook targets.  A hook can sit at the right address and still jump to the
    wrong place. Check 3 decodes each patched instruction and confirms it
    reaches the symbol main.S names.

  Our own code.  The byte gate only ever inspected the GAME's body, so faults
    in the mod's own code were invisible to it. A scripted edit once made a
    helper call itself; it looked like a 752-byte saving because the real work
    had vanished, and it would have stack-overflowed on the first frame.
    Check 5 catches direct self-recursion.

  Real free space.  Summing symbol sizes UNDERSTATES usage: it misses the
    padding the linker inserts between object files. BIOS2 was reported as
    "128 free" when the true figure was 4. Check 4 scans the built payload for
    trailing zeros, which cannot be fooled that way.

  Memory map.  Two allocations once claimed the same address (a retired
    instrument's buffer sat exactly on player 2's camera). It was dead code so
    it never fired, but nothing would have told us. Check 6 tests the map for
    overlaps.

Exit status is non-zero if any check fails, so `make` stops.

Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
"""

import re
import struct
import subprocess
import sys

ORIG = "rom/SCUS_942.28"
BUILT = "build/rom/SCUS_942.28"
ELF = "build/spyro1x2.elf"
MAIN_S = "src/asm/main.S"

LOAD_ADDR = 0x80010000
FILE_BASE = 0x800

# Code regions: name -> (payload file offset, RAM base, length)
REGIONS = [
    ("BIOS2", 0x66000, 0x8000C000, 0x2000),
    ("BIOS2B", 0x68000, 0x8000E400, 0x0400),
]

# Runtime data the mod owns, as (start, length, description). Keep this in step
# with CHANGES.md section 3 — it is what check 6 tests for collisions, and the
# whole point is that a new allocation cannot silently land on an old one.
DATA_MAP = [
    (0x8000E800, 0x444, "player 2 Spyro shadow"),
    (0x8000ED00, 0x004, "player 2 seeded flag"),
    (0x8000ED04, 0x004, "level id last seeded in"),
    (0x8000ED08, 0x004, "handover pending flag"),
    (0x8000ED10, 0x004, "player 2 Sparx pointer"),
    (0x8000ED14, 0x004, "g_Sparx as last seen"),
    (0x8000ED18, 0x004, "last non-zero gamestate"),
    (0x8000ED20, 0x020, "arrival + true-start capture"),
    (0x8000ED40, 0x00C, "fairy mute: owner+1, respawn x,y"),
    (0x8000ED50, 0x004, "which player's tick is running"),
    (0x8000ED70, 0x010, "focus-repair counters"),
    (0x8000EE00, 0x110, "player 2 camera shadow"),
    (0x8000EF10, 0x010, "player 2 camera extra globals"),
    (0x8000F100, 0x100, "flame matrix chains"),
    (0x8000F200, 0x14D, "player 2 pad state"),
    (0x8000F3F0, 0x010, "region-table valid bits"),
    (0x8000F400, 0x200, "per-viewport region tables"),
    (0x8000F600, 0x200, "particle snapshot"),
    (0x8000F800, 0x200, "moby m_WasDrawn flags"),
    (0x8000FA00, 0x400, "moby mask stash"),
    (0x8000FE00, 0x200, "moby owner table"),
]

fails = []
notes = []


def fail(msg):
    fails.append(msg)


def read(path):
    with open(path, "rb") as handle:
        return handle.read()


def symbols():
    out = subprocess.run(["mipsel-none-elf-nm", ELF],
                         capture_output=True, text=True).stdout
    table = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3:
            table[parts[2]] = int(parts[0], 16)
    return table


def hooks():
    """Every patched instruction declared in main.S, as (addr, kind, target)."""
    text = read(MAIN_S).decode("utf-8", "replace")
    found = []
    for addr, kind, target in re.findall(
            r"/\* (0x[0-9a-fA-F]+) \*/\s*(jal|j|\.word)\s+(\S+)", text):
        found.append((int(addr, 16), kind, target.rstrip(",")))
    return found


orig = read(ORIG)
built = read(BUILT)
syms = symbols()
hook_list = hooks()

# ---- 1. size -----------------------------------------------------------
payload = len(built) - len(orig)
if payload < 0 or payload % 2048:
    fail("payload is %d bytes; t_size must stay a multiple of 2048 or the "
         "BIOS hangs at the PlayStation logo" % payload)
else:
    notes.append("size: %d = %d original + %d payload"
                 % (len(built), len(orig), payload))

# ---- 2. every changed byte sits inside a declared hook ------------------
windows = [(a, a + 4) for a, _, _ in hook_list]
stray = []
for i in range(FILE_BASE, len(orig)):
    if orig[i] != built[i]:
        addr = LOAD_ADDR + i - FILE_BASE
        if not any(lo <= addr < hi for lo, hi in windows):
            stray.append(addr)
if stray:
    fail("%d changed byte(s) are NOT inside any hook declared in main.S — "
         "the first few: %s. A .incbin length is wrong, or a hook is "
         "misplaced." % (len(stray), ", ".join("0x%08X" % a for a in stray[:6])))
else:
    notes.append("all changed bytes lie inside the %d declared hooks"
                 % len(hook_list))

# ---- 3. each hook reaches the symbol main.S names -----------------------
bad_target = 0
checked = 0
for addr, kind, target in hook_list:
    off = addr - LOAD_ADDR + FILE_BASE
    word = struct.unpack("<I", built[off:off + 4])[0]
    if kind in ("jal", "j"):
        want = syms.get(target)
        if want is None:
            fail("hook at 0x%08X names %s, which is not in the binary"
                 % (addr, target))
            continue
        opcode = word >> 26
        if opcode not in (2, 3):
            fail("hook at 0x%08X should be a jump but the built word is "
                 "%08X" % (addr, word))
            bad_target += 1
            continue
        got = ((word & 0x03FFFFFF) << 2) | 0x80000000
        if got != want:
            fail("hook at 0x%08X jumps to 0x%08X, but %s is at 0x%08X"
                 % (addr, got, target, want))
            bad_target += 1
        checked += 1
if not bad_target and checked:
    notes.append("all %d jump hooks reach their named function" % checked)

# ---- 4. real free space in each code region ----------------------------
for name, off, base, length in REGIONS:
    chunk = built[off:off + length]
    used = 0
    for i in range(len(chunk) - 1, -1, -1):
        if chunk[i]:
            used = i + 1
            break
    if used > length:
        fail("%s overflows: %d of %d bytes" % (name, used, length))
    else:
        notes.append("%-6s %5d of %d bytes used (%d free)"
                     % (name, used, length, length - used))

# ---- 5. no function in our own code calls itself ------------------------
dis = subprocess.run(["mipsel-none-elf-objdump", "-d", ELF],
                     capture_output=True, text=True).stdout
current = None
start = 0
for line in dis.splitlines():
    head = re.match(r"^([0-9a-f]{8}) <(\S+)>:", line)
    if head:
        start = int(head.group(1), 16)
        current = head.group(2)
        continue
    if current and current.startswith("Sp1x2"):
        call = re.search(r"\bjal\s+([0-9a-f]{8})", line)
        if call and int(call.group(1), 16) == start:
            fail("%s calls itself — infinite recursion. (A scripted edit "
                 "caused exactly this once; the byte gate cannot see it.)"
                 % current)

# ---- 6. runtime memory map has no collisions ---------------------------
ordered = sorted(DATA_MAP)
for (a1, n1, d1), (a2, _, d2) in zip(ordered, ordered[1:]):
    if a1 + n1 > a2:
        fail("memory map collision: %s (0x%08X+0x%X) overlaps %s (0x%08X)"
             % (d1, a1, n1, d2, a2))
for name, _, base, length in REGIONS:
    for a, n, d in DATA_MAP:
        if a < base + length and base < a + n:
            fail("%s is CODE (0x%08X..0x%08X) but %s claims 0x%08X"
                 % (name, base, base + length, d, a))
last = ordered[-1]
if last[0] + last[1] > 0x80010000:
    fail("%s runs past the end of BIOS RAM at 0x80010000" % last[2])
if not any("collision" in f or "CODE" in f for f in fails):
    notes.append("memory map: %d allocations, no overlaps" % len(DATA_MAP))

# ---- 7. every hook is documented in CHANGES.md -------------------------
# CHANGES.md is the canonical record: "if it is not in this file it is not
# finished". This makes that rule enforceable rather than aspirational — a
# hook added without a matching row fails the build.
try:
    doc = read("../../../CHANGES.md").decode("utf-8", "replace").lower()
except OSError:
    notes.append("CHANGES.md not found - documentation check skipped")
else:
    missing = [a for a, _, _ in hook_list if ("%08x" % a) not in doc]
    if missing:
        fail("%d hook(s) are not documented in CHANGES.md: %s. Add a row to "
             "Table 1 in the same pass that adds the hook."
             % (len(missing), ", ".join("0x%08X" % a for a in missing)))
    else:
        notes.append("all %d hooks are documented in CHANGES.md"
                     % len(hook_list))

# ---- report ------------------------------------------------------------
for line in notes:
    print("  " + line)
if fails:
    print()
    for line in fails:
        print("  FAIL - " + line)
    sys.exit(1)
print("  PASS - build verified")
