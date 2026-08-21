#pragma once

#include "gui/customwidgets/overlaywidget.h"
#include "gui/customwidgets/entryinfoitem.h"
#include <QWheelEvent>
#include <QSizeGrip>
#include <QMouseEvent>
#include <QPoint>

namespace Ui {
class ImageInfoOverlay;
}

class ImageInfoOverlay : public OverlayWidget
{
    Q_OBJECT

public:
    explicit ImageInfoOverlay(FloatingWidgetContainer *parent = nullptr);
    ~ImageInfoOverlay();
    void setExifInfo(QMap<QString, QString>);

public slots:
    void show();

protected:
    void wheelEvent(QWheelEvent *event);
    void recalculateGeometry() override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Ui::ImageInfoOverlay *ui;
    QList<EntryInfoItem*> entries;
    QLabel entryStub;
    QSizeGrip *sizeGrip;
    bool userResized;   // user resized the panel -> stop re-adapting its size
    bool userMoved;     // user dragged the panel -> keep its position
    bool dragging;    // header drag in progress
    // constant offset between the global mouse position and the panel's
    // top-left corner, recorded on press (standard drag algorithm)
    QPoint dragOffset;
};
