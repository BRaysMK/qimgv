#include "imageinfooverlay.h"
#include "ui_imageinfooverlay.h"
#include <QFontMetrics>

ImageInfoOverlay::ImageInfoOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent),
    ui(new Ui::ImageInfoOverlay)
{
    ui->setupUi(this);
    ui->closeButton->setIconPath(":res/icons/common/overlay/close-dim16.png");
    ui->headerIcon->setIconPath(":res/icons/common/overlay/info16.png");
    entryStub.setFixedSize(300, 48);
    entryStub.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    connect(ui->closeButton,  &IconButton::clicked, this, &ImageInfoOverlay::hide);
    this->setPosition(FloatingWidgetPosition::RIGHT);

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
    adjustSize();
    recalculateGeometry();
}

void ImageInfoOverlay::recalculateGeometry() {
    const int headerHeight = 44; // header row
    const int entryHeight = 34;  // one metadata row

    // Adaptive height: fit as many rows as fit on screen, cap at the window
    // size, and let the scroll area handle the rest (explicit height, so the
    // entries are always laid out even before the layout's sizeHint settles).
    // with no entries leave room for the "no metadata" stub
    int contentH = entries.count() ? entries.count() * entryHeight : 50;
    int cap = qMax(120, containerSize().height() - 90);
    int scrollH = qBound(entryHeight, contentH, cap - headerHeight);
    ui->scrollArea->setFixedHeight(scrollH);

    // Adaptive width: fit the widest single-line entry, but cap it so long
    // values wrap inside the value column instead of blowing up the panel.
    int nameW = 150;
    int valueW = 160;
    QFontMetrics fm(entryStub.font());
    for(EntryInfoItem *e : entries) {
        valueW = qMax(valueW, fm.horizontalAdvance(e->value()) + 8);
    }
    valueW = qBound(160, valueW, 360);
    int width = qBound(360, nameW + valueW + 36, 560);
    setFixedWidth(width);

    OverlayWidget::recalculateGeometry();

    // clamp the final height so nothing is pushed off-screen
    int totalMax = containerSize().height() - 20;
    if(geometry().height() > totalMax) {
        QRect r = geometry();
        r.setHeight(totalMax);
        setGeometry(r);
    }
}

void ImageInfoOverlay::wheelEvent(QWheelEvent *event) {
    event->accept();
}
