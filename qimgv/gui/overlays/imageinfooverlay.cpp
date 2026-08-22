#include "imageinfooverlay.h"
#include "ui_imageinfooverlay.h"
#include <QFontMetrics>

// QSizeGrip resizes the *top-level* window by default. This overlay lives
// inside the viewer, so a plain QSizeGrip would resize the whole app window
// when the user drags the corner. This subclass only resizes the overlay it
// lives in (and clamps to its minimum size).
class PanelSizeGrip : public QSizeGrip {
public:
    explicit PanelSizeGrip(QWidget *parent) : QSizeGrip(parent) {}
protected:
    void mousePressEvent(QMouseEvent *e) override {
        QSizeGrip::mousePressEvent(e);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        mLastGlobal = e->globalPosition().toPoint();
#else
        mLastGlobal = e->globalPos();
#endif
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        QWidget *p = parentWidget();
        if(!p)
            return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint g = e->globalPosition().toPoint();
#else
        const QPoint g = e->globalPos();
#endif
        const QPoint delta = g - mLastGlobal;
        mLastGlobal = g;
        int w = qMax(p->minimumWidth(), p->width() + delta.x());
        int h = qMax(p->minimumHeight(), p->height() + delta.y());
        p->resize(w, h);
    }
private:
    QPoint mLastGlobal;
};

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

    // drag handle: the header row (icon + title)
    ui->header->installEventFilter(this);
    ui->label->installEventFilter(this);
    ui->headerIcon->installEventFilter(this);

    // resize handle: bottom-right corner (resizes only this overlay)
    sizeGrip = new PanelSizeGrip(this);
    sizeGrip->setCursor(Qt::SizeFDiagCursor);
    sizeGrip->installEventFilter(this);

    // XMP section: collapsible, collapsed by default
    xmpToggle = new QToolButton(this);
    xmpToggle->setText(QStringLiteral("XMP 信息"));
    xmpToggle->setCheckable(true);
    xmpToggle->setChecked(false);
    xmpToggle->setArrowType(Qt::RightArrow);
    xmpToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    xmpToggle->setStyleSheet("QToolButton { border: none; padding: 4px 6px; color: #999999; background: transparent; }");
    xmpToggle->setVisible(false);

    xmpContainer = new QWidget(this);
    xmpLayout = new QVBoxLayout(xmpContainer);
    xmpLayout->setContentsMargins(0, 0, 0, 0);
    xmpLayout->setSpacing(0);
    xmpContainer->setVisible(false);

    connect(xmpToggle, &QToolButton::toggled, this, [this](bool on) {
        xmpContainer->setVisible(on);
        xmpToggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        recalculateGeometry();
    });

    // setPosition() triggers recalculateGeometry(), which reads the XMP
    // members, so it must run after they exist (not in the init list / early).
    this->setPosition(FloatingWidgetPosition::RIGHT);

    if(parent)
        setContainerSize(parent->size());
}

ImageInfoOverlay::~ImageInfoOverlay() {
    delete ui;
    for(auto i = entries.count() - 1; i >= 0; i--)
        delete entries.takeAt(i);
    for(auto i = xmpEntries.count() - 1; i >= 0; i--)
        delete xmpEntries.takeAt(i);
}

void ImageInfoOverlay::setExifInfo(QMap<QString, QString> info, QMap<QString, QString> xmpInfo) {
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

    // XMP section (collapsible, collapsed by default): the XMP rows live in
    // their own container under a toggle button.
    while(xmpEntries.count() > xmpInfo.count()) {
        xmpLayout->removeWidget(xmpEntries.last());
        delete xmpEntries.takeLast();
    }
    while(xmpEntries.count() < xmpInfo.count()) {
        xmpEntries.append(new EntryInfoItem(this));
        xmpLayout->addWidget(xmpEntries.last());
    }
    QMap<QString, QString>::const_iterator xi = xmpInfo.constBegin();
    int xidx = 0;
    while(xi != xmpInfo.constEnd()) {
        xmpEntries.at(xidx)->setInfo(xi.key(), xi.value());
        ++xi;
        ++xidx;
    }
    bool hasXmp = !xmpInfo.isEmpty();
    if(xmpToggle) {
        xmpToggle->setVisible(hasXmp);
        if(xmpContainer)
            xmpContainer->setVisible(hasXmp && xmpToggle->isChecked());
        // keep the XMP section below the EXIF rows (moves it to the layout end)
        ui->entryLayout->addWidget(xmpToggle);
        if(xmpContainer)
            ui->entryLayout->addWidget(xmpContainer);
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
        // adaptive height: EXIF rows + the XMP toggle row + expanded XMP rows
        int contentH = entries.count() * entryHeight;
        if(xmpToggle && xmpToggle->isVisible())
            contentH += entryHeight;                     // the XMP toggle row
        if(xmpContainer && xmpContainer->isVisible())
            contentH += xmpEntries.count() * entryHeight;
        if(!contentH)
            contentH = 50;
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
        if(xmpContainer && xmpContainer->isVisible()) {
            for(EntryInfoItem *e : xmpEntries) {
                nameW = qMax(nameW, fm.horizontalAdvance(e->entryName()) + 10);
                valueW = qMax(valueW, fm.horizontalAdvance(e->entryValue()) + 8);
            }
        }
        nameW = qBound(170, nameW, 260);
        valueW = qBound(180, valueW, 520);
        int width = qBound(460, nameW + valueW + 36, 800);

        // apply the name column width so names are not truncated
        for(EntryInfoItem *e : entries)
            e->setNameColumnWidth(nameW);
        if(xmpContainer && xmpContainer->isVisible()) {
            for(EntryInfoItem *e : xmpEntries)
                e->setNameColumnWidth(nameW);
        }

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
                QPoint g = me->globalPosition().toPoint();
#else
                QPoint g = me->globalPos();
#endif
                // constant: where the grab point sits relative to the panel's
                // top-left corner (in global coordinates)
                dragOffset = g - geometry().topLeft();
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
        QPoint p = event->globalPosition().toPoint() - dragOffset;
#else
        QPoint p = event->globalPos() - dragOffset;
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
