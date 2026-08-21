#include "imageinfooverlay.h"
#include "ui_imageinfooverlay.h"
#include <QFontMetrics>

ImageInfoOverlay::ImageInfoOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent),
    ui(new Ui::ImageInfoOverlay),
    sizeGrip(nullptr),
    userResized(false),
    userMoved(false),
    dragging(false)
{
    ui->setupUi(this);
    ui->closeButton->setIconPath(":res/icons/common/overlay/close-dim16.png");
    ui->headerIcon->setIconPath(":res/icons/common/overlay/info16.png");
    entryStub.setFixedSize(300, 48);
    entryStub.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    connect(ui->closeButton,  &IconButton::clicked, this, &ImageInfoOverlay::hide);
    this->setPosition(FloatingWidgetPosition::RIGHT);

    // drag handle: the header row (icon + title)
    ui->header->installEventFilter(this);
    ui->label->installEventFilter(this);
    ui->headerIcon->installEventFilter(this);

    // resize handle: bottom-right corner
    sizeGrip = new QSizeGrip(this);
    sizeGrip->setCursor(Qt::SizeFDiagCursor);
    sizeGrip->installEventFilter(this);

    if(parent)
        setContainerSize(parent->size());
}

ImageInfoOverlay::~ImageInfoOverlay() {
    delete ui;
    for(auto i = entries.count() - 1; i >= 0; i--)
        delete entries.takeAt(i);
}

void ImageInfoOverlay::setExifInfo(QMap<QString, QString> info) {
    // remove/add entries
    int entryCount = entries.count();
    if(entryCount > info.count()) {
        for(auto i = entryCount - 1; i >= info.count(); i--) {
            ui->entryLayout->removeWidget(entries.last());
            delete entries.takeLast();
        }
    } else if(entryCount < info.count()) {
        for(auto i = entryCount; i < info.count(); i++) {
            entries.append(new EntryInfoItem(this));
            ui->entryLayout->addWidget(entries.last());
        }
    }
    QMap<QString, QString>::const_iterator i = info.constBegin();
    int entryIdx = 0;
    while(i != info.constEnd()) {
        entries.at(entryIdx)->setInfo(i.key(), i.value());
        ++i;
        ++entryIdx;
    }

    // Hiding/showing entryStub causes flicker,
    // so we just remove it from layout and clear the text.
    // It's still there but basically not visible
    if(entries.count()) {
        ui->entryLayout->removeWidget(&entryStub);
        entryStub.setText("");
    } else {
        ui->entryLayout->addWidget(&entryStub);
        entryStub.setText(tr("<no metadata found>"));
    }

    // always recompute geometry so the panel adapts to the (possibly empty) content
    recalculateGeometry();
}

void ImageInfoOverlay::show() {
    OverlayWidget::show();
    recalculateGeometry();
}

void ImageInfoOverlay::recalculateGeometry() {
    const int headerHeight = 44; // header row
    const int entryHeight = 34;  // one metadata row
    const int bottomMargin = 4;

    QSize cs = containerSize();

    // bound the panel to the window (this also caps the size grip)
    setMaximumSize(cs);

    // compute the wanted size: adapt to the content unless the user has
    // resized the panel manually
    QSize wanted = size();
    if(!userResized) {
        // adaptive height: header + as many rows as fit on screen (scroll beyond)
        int contentH = entries.count() ? entries.count() * entryHeight : 50;
        int cap = qMax(120, cs.height() - 90);
        int scrollH = qBound(entryHeight, contentH, cap - headerHeight);

        // adaptive width: fit the widest name and value. Wider than before so
        // long tag names are no longer clipped.
        QFontMetrics fm(entryStub.font());
        int nameW = 170;
        int valueW = 180;
        for(EntryInfoItem *e : entries) {
            nameW = qMax(nameW, fm.horizontalAdvance(e->entryName()) + 10);
            valueW = qMax(valueW, fm.horizontalAdvance(e->entryValue()) + 8);
        }
        nameW = qBound(170, nameW, 260);
        valueW = qBound(180, valueW, 520);
        int width = qBound(460, nameW + valueW + 36, 800);

        // apply the name column width so names are not truncated
        for(EntryInfoItem *e : entries)
            e->setNameColumnWidth(nameW);

        wanted = QSize(width, headerHeight + scrollH + bottomMargin);
    }

    // keep a usable minimum
    setMinimumSize(360, headerHeight + entryHeight + bottomMargin);

    // clamp the size to the container
    wanted.setWidth(qMin(wanted.width(), cs.width()));
    wanted.setHeight(qMin(wanted.height(), qMax(headerHeight + entryHeight + bottomMargin, cs.height() - 20)));

    // position: default spot (right edge) or the user's dragged position
    QPoint pos;
    if(userMoved) {
        pos = QPoint(qBound(0, geometry().x(), qMax(0, cs.width() - wanted.width())),
                     qBound(0, geometry().y(), qMax(0, cs.height() - wanted.height())));
    } else {
        int hm = horizontalMargin();
        int vm = verticalMargin();
        switch(position) {
            case LEFT:          pos = QPoint(hm, (cs.height()-wanted.height())/2); break;
            case RIGHT:         pos = QPoint(cs.width()-wanted.width()-hm, (cs.height()-wanted.height())/2); break;
            case BOTTOM:        pos = QPoint((cs.width()-wanted.width())/2, cs.height()-wanted.height()-vm); break;
            case TOP:           pos = QPoint((cs.width()-wanted.width())/2, vm); break;
            case TOPLEFT:       pos = QPoint(hm, vm); break;
            case TOPRIGHT:      pos = QPoint(cs.width()-wanted.width()-hm, vm); break;
            case BOTTOMLEFT:    pos = QPoint(hm, cs.height()-wanted.height()-vm); break;
            case BOTTOMRIGHT:   pos = QPoint(cs.width()-wanted.width()-hm, cs.height()-wanted.height()-vm); break;
            case CENTER:        pos = QPoint((cs.width()-wanted.width())/2, (cs.height()-wanted.height())/2); break;
        }
    }
    setGeometry(QRect(pos, wanted));

    if(sizeGrip)
        sizeGrip->raise();
}

void ImageInfoOverlay::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if(sizeGrip) {
        sizeGrip->move(width() - sizeGrip->width() - 3, height() - sizeGrip->height() - 3);
        sizeGrip->raise();
    }
}

bool ImageInfoOverlay::eventFilter(QObject *obj, QEvent *event) {
    // dragging the panel by its header (icon / title label / header area)
    if(obj == ui->header || obj == ui->label || obj == ui->headerIcon) {
        switch(event->type()) {
        case QEvent::MouseButtonPress: {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if(me->button() == Qt::LeftButton) {
                dragging = true;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
                dragGrabPoint = mapFromGlobal(me->globalPosition().toPoint());
#else
                dragGrabPoint = mapFromGlobal(me->globalPos());
#endif
                grabMouse();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            if(dragging) {
                dragging = false;
                releaseMouse();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    // just observe the size grip: once the user grabs it, keep their size
    if(obj == sizeGrip && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if(me->button() == Qt::LeftButton)
            userResized = true;
    }
    return QWidget::eventFilter(obj, event);
}

void ImageInfoOverlay::mouseMoveEvent(QMouseEvent *event) {
    if(dragging) {
        QSize cs = containerSize();
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        QPoint p = mapFromGlobal(event->globalPosition().toPoint()) - dragGrabPoint;
#else
        QPoint p = mapFromGlobal(event->globalPos()) - dragGrabPoint;
#endif
        p.setX(qBound(0, p.x(), qMax(0, cs.width() - width())));
        p.setY(qBound(0, p.y(), qMax(0, cs.height() - height())));
        move(p);
        userMoved = true;
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ImageInfoOverlay::mouseReleaseEvent(QMouseEvent *event) {
    if(dragging) {
        dragging = false;
        releaseMouse();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ImageInfoOverlay::wheelEvent(QWheelEvent *event) {
    event->accept();
}
