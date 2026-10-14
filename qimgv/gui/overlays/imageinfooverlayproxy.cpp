#include "imageinfooverlayproxy.h"

ImageInfoOverlayProxy::ImageInfoOverlayProxy(FloatingWidgetContainer *parent)
    : container(parent),
      overlay(nullptr)
{
}

ImageInfoOverlayProxy::~ImageInfoOverlayProxy() {
    if(overlay)
        overlay->deleteLater();
}

void ImageInfoOverlayProxy::show() {
    // pull the metadata only now that the panel is actually being opened
    refreshMetadata();
    init();
    overlay->show();
}

void ImageInfoOverlayProxy::setMetadataProvider(std::function<QMap<QString, QString>()> _exifProvider,
                                                std::function<QMap<QString, QString>()> _xmpProvider) {
    exifProvider = _exifProvider;
    xmpProvider = _xmpProvider;
}

void ImageInfoOverlayProxy::refreshMetadata() {
    if(!exifProvider)
        return;
    stateBuf.info = exifProvider();
    stateBuf.xmpInfo = xmpProvider ? xmpProvider() : QMap<QString, QString>();
    // if the panel is already built, update it in place as well
    if(overlay)
        overlay->setExifInfo(stateBuf.info, stateBuf.xmpInfo);
}

void ImageInfoOverlayProxy::hide() {
    if(overlay)
        overlay->hide();
}

void ImageInfoOverlayProxy::init() {
    if(overlay)
        return;
    overlay = new ImageInfoOverlay(container);
    overlay->setExifInfo(stateBuf.info, stateBuf.xmpInfo);
}

bool ImageInfoOverlayProxy::isHidden() {
    return overlay ? overlay->isHidden() : true;
}

void ImageInfoOverlayProxy::setExifInfo(QMap<QString, QString> _info, QMap<QString, QString> _xmpInfo) {
    if(overlay)
        overlay->setExifInfo(_info, _xmpInfo);
    else {
        stateBuf.info = _info;
        stateBuf.xmpInfo = _xmpInfo;
    }
}
