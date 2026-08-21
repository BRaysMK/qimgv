#pragma once
// Chinese display names for qimgv action keys (shortcut list / creator dialog).
// The raw action key must stay available (store it separately in the item data)
// because saving/editing shortcuts works with the raw key.
#include <QHash>
#include <QString>

static inline QString localizedActionName(const QString &action) {
    if(action.startsWith("s:")) // script action - show the script name as-is
        return action;
    static const QHash<QString, QString> names = {
        {"closeFullScreenOrExit", QStringLiteral("全屏或退出")},
        {"contextMenu", QStringLiteral("右键菜单")},
        {"copyFile", QStringLiteral("复制文件")},
        {"copyFileClipboard", QStringLiteral("复制文件到剪贴板")},
        {"copyPathClipboard", QStringLiteral("复制路径到剪贴板")},
        {"crop", QStringLiteral("裁剪")},
        {"discardEdits", QStringLiteral("放弃编辑")},
        {"exit", QStringLiteral("退出")},
        {"fitNormal", QStringLiteral("适合实际大小")},
        {"fitWidth", QStringLiteral("适合宽度")},
        {"fitWindow", QStringLiteral("适合窗口")},
        {"fitWindowStretch", QStringLiteral("拉伸适应窗口")},
        {"flipH", QStringLiteral("水平翻转")},
        {"flipV", QStringLiteral("垂直翻转")},
        {"folderView", QStringLiteral("文件夹面板")},
        {"frameStep", QStringLiteral("下一帧")},
        {"frameStepBack", QStringLiteral("上一帧")},
        {"goUp", QStringLiteral("上一级目录")},
        {"jumpToFirst", QStringLiteral("跳转到第一张")},
        {"jumpToLast", QStringLiteral("跳转到最后一张")},
        {"moveFile", QStringLiteral("移动文件")},
        {"moveToTrash", QStringLiteral("移到回收站")},
        {"nextDirectory", QStringLiteral("下一目录")},
        {"nextImage", QStringLiteral("下一张图片")},
        {"open", QStringLiteral("打开")},
        {"openSettings", QStringLiteral("打开设置")},
        {"pasteFile", QStringLiteral("粘贴文件")},
        {"prevDirectory", QStringLiteral("上一目录")},
        {"prevImage", QStringLiteral("上一张图片")},
        {"print", QStringLiteral("打印")},
        {"reloadImage", QStringLiteral("重新加载图像")},
        {"removeFile", QStringLiteral("删除文件")},
        {"renameFile", QStringLiteral("重命名文件")},
        {"resize", QStringLiteral("调整大小")},
        {"rotateLeft", QStringLiteral("向左旋转")},
        {"rotateRight", QStringLiteral("向右旋转")},
        {"save", QStringLiteral("保存")},
        {"saveAs", QStringLiteral("另存为")},
        {"scrollDown", QStringLiteral("向下滚动")},
        {"scrollUp", QStringLiteral("向上滚动")},
        {"seekVideoBackward", QStringLiteral("视频后退")},
        {"seekVideoForward", QStringLiteral("视频快进")},
        {"setWallpaper", QStringLiteral("设为壁纸")},
        {"showInDirectory", QStringLiteral("在文件夹中显示")},
        {"toggleFitMode", QStringLiteral("切换适应模式")},
        {"toggleFullscreen", QStringLiteral("切换全屏")},
        {"toggleFullscreenInfoBar", QStringLiteral("切换全屏信息栏")},
        {"toggleImageInfo", QStringLiteral("显示图像信息")},
        {"toggleShuffle", QStringLiteral("切换随机播放")},
        {"toggleSlideshow", QStringLiteral("切换幻灯片播放")},
        {"zoomIn", QStringLiteral("放大")},
        {"zoomInCursor", QStringLiteral("放大(光标处)")},
        {"zoomOut", QStringLiteral("缩小")},
        {"zoomOutCursor", QStringLiteral("缩小(光标处)")}
    };
    return names.value(action, action);
}
