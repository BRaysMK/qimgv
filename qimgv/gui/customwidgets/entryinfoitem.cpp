#include "entryinfoitem.h"

EntryInfoItem::EntryInfoItem(QWidget *parent) : QWidget(parent) {
    layout.setContentsMargins(9,0,9,0);
    layout.setSpacing(0);
    layout.addWidget(&nameLabel);
    layout.addWidget(&valueLabel);
    setLayout(&layout);

    // name column: fixed width, stays on one line
    nameLabel.setFixedWidth(150);
    nameLabel.setMinimumHeight(30);
    // value column: takes the remaining space and wraps long values
    valueLabel.setMinimumHeight(30);
    valueLabel.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    valueLabel.setWordWrap(true);

    // add some padding for easier text selection
    valueLabel.setContentsMargins(3,0,0,0);
    valueLabel.setTextInteractionFlags(Qt::TextSelectableByMouse);
    valueLabel.setCursor(Qt::IBeamCursor);
}

void EntryInfoItem::setInfo(QString _name, QString _value) {
    name = _name;
    value = _value;
    nameLabel.setText(name);
    valueLabel.setText(value);
    // keep the full text reachable even if the column is too narrow
    nameLabel.setToolTip(name);
    valueLabel.setToolTip(value);
};

void EntryInfoItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
