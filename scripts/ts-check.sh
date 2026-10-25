#!/bin/bash
# Translation sanity check:
#  1) XML well-formedness for every .ts
#  2) per-language coverage report (unfinished/empty entries are dropped by
#     lrelease at runtime, so they count as missing)
#  3) zh_CN.ts must have no unfinished entries (CI fails otherwise)
cd "$(dirname "$0")/../qimgv/res/translations" || exit 9

for f in *.ts; do
  python -c "import xml.etree.ElementTree as ET; ET.parse('$f')" 2>/dev/null && echo "XML  OK   $f" || echo "XML  FAIL $f"
done
echo
echo '--- coverage (finished / total, unfinished+empty listed separately) ---'
python - <<'PYEOF'
import glob
import xml.etree.ElementTree as ET

fail = False
for path in sorted(glob.glob('*.ts')):
    root = ET.parse(path).getroot()
    total = unfinished = empty_finished = finished = 0
    for msg in root.iter('message'):
        total += 1
        tr = msg.find('translation')
        if tr is None:
            continue
        if tr.get('type') == 'unfinished':
            unfinished += 1
        elif not (tr.text or '').strip():
            empty_finished += 1
        else:
            finished += 1
    eff = finished + unfinished + empty_finished  # messages that exist at all
    flag = ''
    if path == 'zh_CN.ts' and unfinished:
        flag = '  <-- FAIL: unfinished entries are dropped at runtime'
        fail = True
    if path == 'en_US.ts':
        flag = '  (baseline file, all unfinished - expected)'
    print(f'{path:12} finished {finished:4}/{eff:4}  unfinished {unfinished:3}  empty-finished {empty_finished:3}{flag}')
raise SystemExit(1 if fail else 0)
PYEOF
rc=$?
echo
[ $rc -eq 0 ] && echo 'ts-check: PASS' || echo 'ts-check: FAIL'
exit $rc
