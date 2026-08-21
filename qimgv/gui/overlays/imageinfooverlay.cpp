#include "imageinfooverlay.h"
#include "ui_imageinfooverlay.h"

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
        entryStub.setText("<no metadata found>");
    }

    if(!isHidden() && entryCount != info.count()) {
        // wait for layout change
        qApp->processEvents();
        // reposition
        recalculateGeometry();
    }
}

void ImageInfoOverlay::show() {
    OverlayWidget::show();
    adjustSize();
    recalculateGeometry();
}

void ImageInfoOverlay::recalculateGeometry() {
    // Keep the panel inside the window: cap the scrollable area's height
    // (a full EXIF dump can easily have several dozen entries).
    int cap = containerSize().height() - 90; // header + margins
    cap = qBound(120, cap, 500);
    ui->scrollArea->setMaximumHeight(cap);

    OverlayWidget::recalculateGeometry();

    // The layout may still ask for more space than the window provides;
    // clamp the final geometry so nothing is pushed off-screen.
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
