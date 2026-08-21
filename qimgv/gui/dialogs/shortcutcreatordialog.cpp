#include "shortcutcreatordialog.h"
#include "ui_shortcutcreatordialog.h"
#include "shortcutnames.h"

ShortcutCreatorDialog::ShortcutCreatorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ShortcutCreatorDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Add shortcut"));
    actionList = appActions->getList();
    scriptList = scriptManager->scriptNames();

    // show localized names, keep the raw action key as item data
    for(const QString &a : actionList)
        ui->actionsComboBox->addItem(localizedActionName(a), a);
    ui->actionsComboBox->setCurrentIndex(0);

    ui->scriptsComboBox->addItems(scriptList);
    ui->scriptsComboBox->setCurrentIndex(0);
}

ShortcutCreatorDialog::~ShortcutCreatorDialog() {
    delete ui;
}

QString ShortcutCreatorDialog::selectedAction() {
    if(ui->actionsRadioButton->isChecked())
        return ui->actionsComboBox->currentData().toString();
    else
        return "s:"+ui->scriptsComboBox->currentText();
}

QString ShortcutCreatorDialog::selectedShortcut() {
    return ui->sequenceEdit->sequence();
}

void ShortcutCreatorDialog::onShortcutEdited() {
    QString action = actionManager->actionForShortcut(ui->sequenceEdit->sequence());
    if(!action.isEmpty())
        ui->warningLabel->setText(tr("This shortcut is used for action: %1. Replace?").arg(localizedActionName(action)));
    else
        ui->warningLabel->setText("");
}

void ShortcutCreatorDialog::setAction(QString action) {
    auto cbox = ui->actionsComboBox;
    if(action.startsWith("s:")) {
        action = action.remove(0,2);
        cbox = ui->scriptsComboBox;
        ui->scriptsRadioButton->setChecked(true);
    }
    // actions combo holds the raw key as item data; scripts combo is text-only
    int index = (cbox == ui->actionsComboBox) ? cbox->findData(action) : cbox->findText(action);
    if(index != -1)
       cbox->setCurrentIndex(index);
}

void ShortcutCreatorDialog::setShortcut(QString shortcut) {
    ui->sequenceEdit->setText(shortcut);
}
