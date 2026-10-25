#!/bin/bash
# Dead-DLL audit v2: retry ldd per file, list failures explicitly.
# A file is DEAD only if no other package file's successful ldd references it.
PKG="$1"
cd "$PKG" || exit 9
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
    return None  # failed after retries

edges = {}
failures = []
for f in files:
    refs = imports(f)
    if refs is None:
        failures.append(os.path.basename(f))
        edges[f] = set()
    else:
        edges[f] = {by_name[r] for r in refs if r in by_name and by_name[r] != f}

def is_root(f):
    rel = os.path.relpath(f, pkg).replace('\\','/').lower()
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
            reach.add(dep)
            queue.append(dep)

dead = [f for f in files if f not in reach]
total = sum(os.path.getsize(f) for f in files)
dead_size = sum(os.path.getsize(f) for f in dead)
print(f"files: {len(files)}, total {total/1e6:.1f} MB, reachable {len(files)-len(dead)}, dead {len(dead)} ({dead_size/1e6:.1f} MB)")
if failures:
    print("LDD FAILURES (excluded from edge analysis):", failures)
for f in sorted(dead):
    print(f"DEAD {os.path.relpath(f,pkg)} {os.path.getsize(f)}")
PYEOF
