#!/bin/bash
# Generate the fork-local msys2 dll list (reachable set only) from an extracted package.
# Usage: OUT=<path> gen-fork-dll-list.sh <extracted-package-dir>
PKG="$1"
OUT="${OUT:-msys2-dll-deps-fork.txt}"
# resolve OUT to an absolute path BEFORE entering the package dir
mkdir -p "$(dirname "$OUT")"
OUT_ABS="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")" || exit 9
cd "$PKG" || exit 9
export AUDIT_OUT="$OUT_ABS"
python - <<'PYEOF'
import os, subprocess, re

pkg = os.getcwd()
files = []
for root, dirs, fnames in os.walk(pkg):
    dirs[:] = [d for d in dirs if d not in ('cache','conf','data','translations','thumbnails')]
    for f in fnames:
        if f.lower().endswith(('.dll','.exe')):
            files.append(os.path.join(root, f))

by_name = {os.path.basename(f).lower(): f for f in files}

def imports(path):
    for attempt in range(3):
        try:
            out = subprocess.run(['ldd', path], capture_output=True, text=True, timeout=60).stdout
            refs = set()
            for line in out.splitlines():
                m = re.match(r'\s*(\S+)\s+=>', line)
                if m:
                    refs.add(os.path.basename(m.group(1)).lower())
            return refs
        except Exception:
            continue
    return None

edges = {}
for f in files:
    refs = imports(f)
    edges[f] = {by_name[r] for r in (refs or set()) if r in by_name and by_name[r] != f} if refs is not None else set()

def is_root(f):
    rel = os.path.relpath(f, pkg).replace(chr(92),'/').lower()
    if rel in ('qimgv.exe','mpv.exe'): return True
    if rel.startswith(('imageformats/','platforms/','styles/','printsupport/','iconengines/')) and rel.endswith('.dll'): return True
    if rel == 'plugins/player_mpv.dll': return True
    return False

reach = set()
queue = [f for f in files if is_root(f)]
reach.update(queue)
while queue:
    cur = queue.pop()
    for dep in edges[cur]:
        if dep not in reach:
            reach.add(dep); queue.append(dep)

markers = ('qt5','libopencv','mpv.exe','libmpv','libexiv2','qimgv.exe','player_mpv','imageformats','platforms','styles','printsupport','iconengines')
msys = []
for f in sorted(reach):
    rel = os.path.relpath(f, pkg).replace(chr(92),'/').lower()
    if any(m in rel for m in markers): continue
    msys.append(os.path.basename(rel))
out = os.environ.get('AUDIT_OUT','msys2-dll-deps-fork.txt')
with open(out,'w',newline='\n') as fh:
    fh.write("\n".join(msys) + "\n")
print(f"written {len(msys)} entries -> {out}")
PYEOF
