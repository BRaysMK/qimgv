#pragma once

#include "gui/overlays/imageinfooverlay.h"
#include <functional>

struct ImageInfoOverlayStateBuffer {
    QMap<QString, QString> info;
    QMap<QString, QString> xmpInfo;
};

class ImageInfoOverlayProxy {
public:
    explicit ImageInfoOverlayProxy(FloatingWidgetContainer *parent = nullptr);
    ~ImageInfoOverlayProxy();
    void init();
    void show();
    void hide();

    // Metadata is only needed while the panel is actually visible. Reading it is
    // expensive (a full EXIF+XMP parse per image), so instead of pushing it on
    // every image load the panel pulls it through this provider when it opens.
    void setMetadataProvider(std::function<QMap<QString, QString>()> exifProvider,
                             std::function<QMap<QString, QString>()> xmpProvider);
    void refreshMetadata();

    void setExifInfo(QMap<QString, QString> info, QMap<QString, QString> xmpInfo);
    bool isHidden();
private:
    FloatingWidgetContainer *container;
    ImageInfoOverlay *overlay;
    ImageInfoOverlayStateBuffer stateBuf;
    std::function<QMap<QString, QString>()> exifProvider;
    std::function<QMap<QString, QString>()> xmpProvider;
};
