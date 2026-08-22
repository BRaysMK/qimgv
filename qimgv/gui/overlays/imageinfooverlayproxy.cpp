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
    init();
    overlay->show();
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
