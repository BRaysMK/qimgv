#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""One-shot batch update for zh_CN.ts (audit fixes: unfinished->finished,
fill empty translations, insert missing entries incl. the new About HTML).

Text-level edits to keep the diff minimal. Idempotent-ish: every op reports
FOUND/FILLED/MISSING so re-runs are safe.
"""
import re
import sys
import io

TS = "qimgv/res/translations/zh_CN.ts"
UI = "qimgv/gui/dialogs/settingsdialog.ui"

def esc(s: str) -> str:
    # lupdate escapes & < > " (and ' as &apos;) in text nodes
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
             .replace('"', "&quot;").replace("'", "&apos;"))

def nl(s: str) -> str:
    # literal newline in source -> XML numeric entity (decodes back to \n)
    return s.replace("\n", "&#10;")

data = io.open(TS, encoding="utf-8").read()
ops = {"finished": [], "filled": [], "inserted": [], "source_changed": [], "missed": []}

def context_span(ctx: str):
    m = re.search(r"<name>%s</name>(.*?)</context>" % re.escape(ctx), data, re.S)
    return m

def set_tr(ctx: str, source: str, zh: str, change_to: str = None):
    """Fill/re-tag one message. source = raw text (will be escaped).
    change_to: optionally rewrite the <source> too (typo fixes)."""
    global data
    m = context_span(ctx)
    if not m:
        ops["missed"].append((ctx, source, "no context"))
        return
    body = m.group(1)
    src_esc = esc(source)
    pat = re.compile(
        r"<source>(%s)</source>((?:\s*<translatorcomment>.*?</translatorcomment>)?\s*)<translation([^>]*)>(.*?)</translation>" % re.escape(src_esc),
        re.S)
    pm = pat.search(body)
    if not pm:
        ops["missed"].append((ctx, source, "no source entry"))
        return
    old_trans = pm.group(4)
    mid = pm.group(2)  # whitespace / translatorcomment between source and translation
    new_src_esc = esc(change_to) if change_to else src_esc
    new_trans = esc(zh)
    new_body = pat.sub(
        lambda mm: "<source>%s</source>%s<translation>%s</translation>" % (new_src_esc, mm.group(2), new_trans),
        body, count=1)
    data = data[:m.start(1)] + new_body + data[m.end(1):]
    if change_to:
        ops["source_changed"].append((ctx, source, "->", change_to, zh))
    elif old_trans.strip():
        ops["finished"].append((ctx, source, "->", zh))
    else:
        ops["filled"].append((ctx, source, "->", zh))

def add_tr(ctx: str, source: str, zh: str):
    """Insert a new message if the source is not already present."""
    global data
    src_esc = esc(source)
    m = context_span(ctx)
    msg = (
        "    <message>\n"
        "        <source>%s</source>\n"
        "        <translation>%s</translation>\n"
        "    </message>\n" % (src_esc, esc(zh))
    )
    if m:
        if re.search(r"<source>%s</source>" % re.escape(src_esc), m.group(1), re.S):
            ops["missed"].append((ctx, source, "already present"))
            return
        insert_at = m.end(1)  # before </context> (group ends right there)
        data = data[:insert_at] + msg + data[insert_at:]
        ops["inserted"].append((ctx, source))
    else:
        block = "    <context>\n    <name>%s</name>\n%s    </context>\n" % (ctx, msg)
        data = data.replace("</TS>", block + "</TS>")
        ops["inserted"].append((ctx, source, "new context"))

def about_html():
    """Extract the (escaped) html string from settingsdialog.ui and build the
    translated copy with identical structure."""
    global data
    ui = io.open(UI, encoding="utf-8").read()
    m = re.search(r'<property name="html">\s*<string>(.*?)</string>', ui, re.S)
    if not m:
        raise SystemExit("about html not found in ui file")
    src = m.group(1)  # already xml-escaped in the ui file
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
            print("WARN: about-html text not found:", a)
        zh = zh.replace(a, b)
    return src, zh

# ---------------- C++ entries ----------------
set_tr("ContextMenu", "Print", "打印")
set_tr("ContextMenu", "Open with...", "打开方式...")
set_tr("ContextMenu", "Back", "返回")
set_tr("ContextMenu", "Configure menu", "配置菜单")
set_tr("ContextMenu", "Edit", "编辑")
set_tr("SSideBar", "General", "常规")
set_tr("SSideBar", "View", "查看")
set_tr("SSideBar", "Theme", "主题")
set_tr("SSideBar", "Controls", "控制中心")
set_tr("SSideBar", "Scripts", "脚本")
set_tr("SSideBar", "Advanced", "高级")
set_tr("SSideBar", "About", "关于")
set_tr("ScriptEditorDialog", "Edit", "编辑")
set_tr("ScriptEditorDialog", "New application/script", "新建应用程序/脚本")
set_tr("ScriptEditorDialog", "Select an executable/script", "选择可执行文件/脚本")
set_tr("QObject", "Operation completed succesfully.", "操作完成。", change_to="Operation completed successfully.")
set_tr("FileReplaceDialog", "src", "源")
set_tr("FileReplaceDialog", "dst", "目标")

# ---------------- new tr() entries ----------------
add_tr("ExifEditDialog", 'Marked for deletion. Use "Undo delete" to restore.', "已标记为待删除。使用“撤销删除”可恢复。")
add_tr("Core", "Metadata editing is only available for static images.", "仅静态图像支持元数据编辑。")
add_tr("Core", "Unsaved changes", "未保存的更改")
add_tr("Core", "This image has unsaved changes.\nDiscard them and continue?", "此图像有未保存的更改。\n放弃并继续？")
add_tr("SettingsDialog", "Language changed", "语言已更改")
add_tr("SettingsDialog", "The new language will be applied after you restart the application.", "新语言将在重启应用后生效。")
add_tr("FileReplaceDialog", "File already exists", "文件已存在")
add_tr("FileReplaceDialog", "Replace destination file?", "替换目标文件吗？")
add_tr("FileReplaceDialog", "Directory already exists", "目录已存在")
add_tr("FileReplaceDialog", "Merge directories?", "合并目录吗？")
add_tr("FileReplaceDialog", "Destination already exists", "目标已存在")
add_tr("FileReplaceDialog", "There is a file with that name. Replace?", "已存在同名文件，是否替换？")
add_tr("FileReplaceDialog", "There is a folder with that name. Replace?", "已存在同名文件夹，是否替换？")
add_tr("MW", "All Files (*)", "所有文件 (*)")
add_tr("MW", "Open image", "打开图像")
add_tr("Settings", "Supported files (", "支持的文件 (")
add_tr("PathSelectorMenuItem", "Select directory", "选择目录")
add_tr("FolderView", "Select directory", "选择目录")
add_tr("FullscreenInfoOverlay", "No file opened", "未打开文件")
add_tr("InfoBar", "No file opened.", "未打开文件。")
add_tr("PrintDialog", "PDF (*.pdf)", "PDF 文件 (*.pdf)")

# ---------------- settingsdialog.ui entries ----------------
UI_ZH = {
    "Language:": "语言：",
    "Requires application restart": "需要重启应用",
    "User interface": "用户界面",
    "On": "开",
    "Off": "关",
    "Automatic window resize": "自动调整窗口大小",
    "Match displayed content": "适应显示内容",
    "Screen area limit for auto resize:": "自动缩放的屏幕区域上限：",
    "Crop previews": "裁剪预览",
    "Pinned": "固定",
    "Disable in windowed mode": "窗口模式下禁用",
    "Center selected image": "居中选中图像",
    "Previews only": "仅预览",
    "Show filename and resolution": "显示文件名和分辨率",
    "Preview size:": "预览大小：",
    "Left": "左侧",
    "Right": "右侧",
    "Stop": "停止",
    "Loop folder": "循环文件夹",
    "Go to the next folder": "进入下一个文件夹",
    "After reaching the end:": "到达末尾后：",
    "View": "查看",
    "Display options": "显示选项",
    "Keep fit mode when switching images": "切换图像时保持适应模式",
    "Part of image that's focused after switching to 1:1": "切换到 1:1 后聚焦的图像部分",
    "Grid background on images with transparency": "透明图像显示网格背景",
    "Expand images, up to:": "放大小图，上限：",
    "Images smaller than window will be zoomed in": "小于窗口的图像将被放大",
    "Unlock minimum zoom": "解锁最小缩放",
    "Always allow zooming below 100%": "允许缩放到 100% 以下",
    "Use fixed zoom levels:": "使用固定缩放级别：",
    "Load defaults": "恢复默认",
    "Theme": "主题",
    "Background (fullscreen mode)": "背景（全屏模式）",
    "Other window tweaks": "其他窗口调整",
    "Scroll image with:": "滚动图像方式：",
    "Note: you can also zoom by holding RMB and moving the mouse": "注：按住鼠标右键拖动也可缩放",
    'Note: these will appear in "Open with" menu.': "注：这些将出现在“打开方式”菜单中。",
    'Also, you can assign shortcuts to scripts (in "Controls" section).': "另外，可在“控制中心”分区为脚本分配快捷键。",
    "Load adjacent images in background": "在后台加载相邻图像",
    "Dynamically unload items to save memory": "动态卸载条目以节省内存",
    "Confirm file delete (!)": "删除文件前确认 (!)",
    "Memory allocation limit per image, MB:": "每张图像内存上限（MB）：",
}
for src, zh in UI_ZH.items():
    set_tr("SettingsDialog", src, zh)

UI_ADD = {
    "Apply sorting to folders": "将排序应用到文件夹",
    "Show hidden files": "显示隐藏文件",
    "Switch image by clicking window edges": "点击窗口边缘切换图像",
    "Visible edges": "可见边缘",
    "Mouse scrolling speed:": "鼠标滚动速度：",
    "Trackpad detection": "触摸板检测",
    "Disable if you have issues with mouse scrolling": "鼠标滚动异常时请禁用",
}
for src, zh in UI_ADD.items():
    add_tr("SettingsDialog", src, zh)

# About page rich text: replace the stale source with the current ui string
src, zh = about_html()
m = re.search(r"<source>(%s)</source>(\s*)<translation([^>]*)>(.*?)</translation>" % re.escape(src), data, re.S)
if m:
    pm = re.compile(r"<source>(.*?)</source>(\s*)<translation[^>]*>(.*?)</translation>", re.S)
    # find the message whose source starts with <!DOCTYPE HTML
    pat = re.compile(r"<source>(&lt;!DOCTYPE HTML PUBLIC.*?)</source>(\s*)<translation[^>]*>(.*?)</translation>", re.S)
    pm = pat.search(data)
    if pm:
        data = data[:pm.start()] + "<source>%s</source>%s<translation>%s</translation>" % (src, pm.group(2), esc(zh)) + data[pm.end():]
        ops["filled"].append(("SettingsDialog", "About HTML (rewritten to current source)"))
    else:
        add_tr("SettingsDialog", src, zh)
else:
    add_tr("SettingsDialog", src, zh)

io.open(TS, "w", encoding="utf-8", newline="\n").write(data)

# ---------------- report ----------------
print("== re-tagged finished (had translation) ==")
for t in ops["finished"]:
    print("  ", t)
print("== filled empty translations ==")
for t in ops["filled"]:
    print("  ", t)
print("== source changed ==")
for t in ops["source_changed"]:
    print("  ", t)
print("== inserted ==")
for t in ops["inserted"]:
    print("  ", t)
print("== MISSED (need attention) ==")
for t in ops["missed"]:
    print("  !!", t)

# well-formedness check
import xml.etree.ElementTree as ET
ET.parse(TS)
print("XML OK")
