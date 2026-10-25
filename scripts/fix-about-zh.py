#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Repair the About-page entries in zh_CN.ts after the double-escaping mess:
- remove the two double-escaped inserted <message> blocks (&amp;lt;...)
- rewrite the original (location-carrying) entry with the correct single-escaped
  source and a single-escaped zh translation (HTML tags stay as &lt; entities,
  visible text is Chinese).
Idempotent.
"""
import io, re, sys

TS = "qimgv/res/translations/zh_CN.ts"
UI = "qimgv/gui/dialogs/settingsdialog.ui"

ui = io.open(UI, encoding="utf-8").read()
m = re.search(r'<property name="html">\s*<string>(.*?)</string>', ui, re.S)
if not m:
    raise SystemExit("about html not found in ui")
src = m.group(1)  # already xml-escaped; DO NOT escape again

zh = src
pairs = [
    ("This is a fast and easy to use image viewer", "这是一款快速易用的图像查看器"),
    ("Features video support via libmpv", "通过 libmpv 支持视频播放"),
    ("This is a fork of qimgv, maintained by BRaysMK", "本程序是 qimgv 的分支（fork），由 BRaysMK 维护"),
    ("Github page:", "GitHub 页面："),
    ("Main developer:", "主开发者："),
    ("Contributors", "贡献者"),
    ("OpenCV enabled builds use ", "启用 OpenCV 的构建使用 "),
    (" by dbzhang800", "，作者 dbzhang800"),
    ("qimgv is licensed under ", "qimgv 基于 "),
    ("GNU GPL Version 3", "GNU GPL 第 3 版"),
    ("Report any issues / request features ", "报告问题 / 提出功能建议 "),
    ("&gt;here&lt;", "&gt;请点这里&lt;"),
]
for a, b in pairs:
    if a not in zh:
        print("WARN: text not found:", a)
    zh = zh.replace(a, b)

data = io.open(TS, encoding="utf-8").read()
orig_len = len(data)

# 1) drop double-escaped message blocks entirely (they can only be broken inserts)
dbl = re.compile(r"[ \t]*<message>\s*<source>&amp;lt;!DOCTYPE HTML PUBLIC.*?</translation>\s*</message>\s*", re.S)
data, n = dbl.subn("", data)
print("removed double-escaped blocks:", n)

# 2) fix the real (single-escaped, location-carrying) entry
real = re.compile(
    r"(<source>)((?:(?!amp;)&lt;!DOCTYPE HTML PUBLIC).*?)(</source>\s*)<translation[^>]*>.*?</translation>",
    re.S)
mm = real.search(data)
if not mm:
    raise SystemExit("real about entry not found")
new_block = "%s%s%s<translation>%s</translation>" % (mm.group(1), src, mm.group(3), zh)
data = data[:mm.start()] + new_block + data[mm.end():]
print("rewrote real entry; source ok:", src in data, "| zh ok:", "请点这里" in data)

io.open(TS, "w", encoding="utf-8", newline="\n").write(data)
print("len:", orig_len, "->", len(data))

import xml.etree.ElementTree as ET
ET.parse(TS)
print("XML OK")
