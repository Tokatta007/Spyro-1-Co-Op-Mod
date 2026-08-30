"""Cross-platform `rm` for the Makefile, matching cp.py / mkdir.py in style.

Restored 2026-08-28. env.mk has always defined RM = python rm.py, but the file
was missing, so `make clean` failed every time it was run — silently, because
the Makefile line is not prefixed with `-` and callers redirected output. The
visible symptom was a stale-object link error after editing a header, which
`make clean` was supposed to cure and could not.

Accepts the -r/-rf/-f flags the Makefile passes and ignores missing paths, so
`make clean` on an already-clean tree succeeds rather than erroring.

Derived from Spyromain's Spyro2x2 (MIT). See LICENSE.
"""

import argparse
import pathlib
import shutil


class ArgNamespace:
    paths: list
    recursive: bool
    force: bool


parser = argparse.ArgumentParser()
parser.add_argument("paths", type=pathlib.Path, nargs="+")
parser.add_argument("-r", "-R", "--recursive", action="store_true")
parser.add_argument("-f", "--force", action="store_true")
args = parser.parse_args(namespace=ArgNamespace())

for path in args.paths:
    if path.is_dir() and args.recursive:
        shutil.rmtree(path, ignore_errors=args.force)
    elif path.exists():
        path.unlink()
    elif not args.force:
        raise SystemExit("rm.py: no such file or directory: %s" % path)
