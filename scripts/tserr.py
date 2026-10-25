import xml.etree.ElementTree as ET
import sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
for f in ['zh_CN.ts', 'en_US.ts']:
    try:
        ET.parse(f)
        print(f, 'OK')
    except Exception as e:
        print(f, 'ERROR:', e)
