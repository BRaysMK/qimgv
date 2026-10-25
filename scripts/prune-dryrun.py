#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Verify the prune algorithm against a shipped package: BFS the import
closure from the exe + plugin dirs (via ldd), report top-level DLLs that are
unreachable. Dry-run: prints what build-qimgv.sh would delete.
"""
import subprocess
import sys
from pathlib import Path

PKG = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
PLUGIN_SUBDIRS = ["platforms", "printsupport", "styles", "iconengines", "imageformats", "plugins"]

def ldd_deps(path):
    try:
        out = subprocess.run(["ldd", str(path)], capture_output=True, text=True, timeout=60).stdout
    except Exception as e:
        print(f"  ldd FAIL {path}: {e}", file=sys.stderr)
        return set()
    deps = set()
    for line in out.splitlines():
        if "=>" not in line:
            continue
        name = line.split("=>")[0].strip()
        if name:
            deps.add(name)
    return deps

# roots: exes + every dll inside plugin subdirs (runtime dlopen'd, ldd-unreachable)
roots = [PKG / "qimgv.exe", PKG / "mpv.exe"]
for sub in PLUGIN_SUBDIRS:
    d = PKG / sub
    if d.is_dir():
        roots += sorted(d.glob("*.dll"))

seen = set()
queue = [r for r in roots if r.is_file()]
while queue:
    f = queue.pop(0)
    base = f.name
    if base in seen:
        continue
    seen.add(base)
    for dep in ldd_deps(f):
        cand = PKG / dep
        if cand.is_file() and dep not in seen:
            queue.append(cand)

top_dlls = sorted(p for p in PKG.glob("*.dll"))
unreachable = [p for p in top_dlls if p.name not in seen]

reach = len(top_dlls) - len(unreachable)
total_mb = sum(p.stat().st_size for p in unreachable) / 1e6
print(f"top-level DLLs: {len(top_dlls)}, reachable: {reach}, unreachable: {len(unreachable)} ({total_mb:.1f} MB)")
print("\n-- would prune --")
for p in unreachable:
    print(f"  {p.name:32} {p.stat().st_size/1e6:8.2f} MB")
