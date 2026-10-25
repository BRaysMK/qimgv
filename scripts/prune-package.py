#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Prune top-level DLLs unreachable from the package entry points.

BFS over the REAL PE import tables (regular + delay imports) starting from
qimgv.exe, mpv.exe and every runtime-loaded plugin dll. Anything not reached
is deleted. This replaces the ldd-based sweep, which is unreliable in both
directions: it misses deps when they are not on PATH, and on CI it reported
phantom deps (styles/qwindowsvistastyle.dll "importing" libicuin78 which its
import table does not contain).

Standard library only (msys2 python friendly).

Usage: prune-package.py <package_dir> [--dry-run]
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pe_imports import imports  # noqa: E402

PLUGIN_SUBDIRS = ["platforms", "printsupport", "styles", "iconengines", "imageformats", "plugins"]

def main():
    pkg = Path(sys.argv[1]).resolve()
    dry = "--dry-run" in sys.argv[2:]

    roots = [pkg / "qimgv.exe", pkg / "mpv.exe"]
    for sub in PLUGIN_SUBDIRS:
        d = pkg / sub
        if d.is_dir():
            roots += sorted(d.glob("*.dll"))

    seen = set()
    queue = [r for r in roots if r.is_file()]
    while queue:
        f = queue.pop(0)
        key = f.name.lower()
        if key in seen:
            continue
        seen.add(key)
        for kind, dep in imports(f):
            cand = pkg / dep
            if cand.is_file() and cand.name.lower() not in seen:
                queue.append(cand)

    removed = 0
    total = 0
    for p in sorted(pkg.glob("*.dll")):
        total += 1
        if p.name.lower() not in seen:
            removed += 1
            if dry:
                print(f"would prune: {p.name}")
            else:
                print(f"prune unreachable: {p.name}")
                p.unlink()
    print(f"prune: {removed} of {total} top-level dlls removed")

if __name__ == "__main__":
    main()
