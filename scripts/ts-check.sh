#!/bin/bash
cd '/e/something/AI Agent/qimgv/qimgv/res/translations' || exit 9
for f in *.ts; do
  python -c "import xml.etree.ElementTree as ET; ET.parse('$f')" 2>/dev/null && echo "OK   $f" || echo "FAIL $f"
done
echo '--- zh sample ---'
python - <<'PYEOF'
import xml.etree.ElementTree as ET
t = ET.parse('zh_CN.ts')
for c in t.getroot():
    if c.get('name') == 'ExifEditDialog':
        n = 0
        for m in c:
            s = m.findtext('source'); tr = m.findtext('translation')
            n += 1
            if n <= 6: print(f'  {s!r} -> {tr!r}')
        print(f'  total messages: {n}')
PYEOF
