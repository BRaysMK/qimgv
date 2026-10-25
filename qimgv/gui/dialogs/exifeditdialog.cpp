#include "exifeditdialog.h"
#include "ui_exifeditdialog.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>

#include "sourcecontainers/exifwriter.h"

namespace {

enum Column {
    COL_NAME = 0,
    COL_KEY,
    COL_TYPE,
    COL_VALUE,
    COL_COUNT
};

} // namespace

ExifEditDialog::ExifEditDialog(const QString &filePath, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExifEditDialog),
    mFilePath(filePath)
{
    ui->setupUi(this);
    applyLocalizedHeaders();

    ui->fileLabel->setText(QFileInfo(mFilePath).fileName());

    // buttons
    ui->buttonBox->button(QDialogButtonBox::Save)->setText(tr("Save"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);

    connect(ui->deleteButton, &QPushButton::clicked, this, &ExifEditDialog::markDeleteSelected);
    connect(ui->undoDeleteButton, &QPushButton::clicked, this, &ExifEditDialog::undoDeleteSelected);
    connect(ui->addButton, &QPushButton::clicked, this, &ExifEditDialog::addTag);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ExifEditDialog::onSaveClicked);
    // Without this the Cancel button emits rejected into the void and the
    // dialog never closes.
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // The delete/undo buttons depend on which row is selected, so they have to
    // react to the selection changing - not only to value edits.
    connect(ui->table, &QTableWidget::itemSelectionChanged,
            this, &ExifEditDialog::refreshButtons);
    connect(ui->table, &QTableWidget::currentCellChanged,
            this, [this](int, int, int, int) { refreshButtons(); });

    // committing an in-place edit stages a change
    connect(ui->table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if(!item || item->column() != COL_VALUE)
            return;
        const int row = item->row();
        if(row < 0 || row >= mEntries.count())
            return;
        const ExifEntryInfo &entry = mEntries.at(row);
        if(!entry.writable)
            return;
        // the row is staged for deletion; editing it must not resurrect it
        if(mDeletedKeys.contains(entry.key))
            return;
        const QString newValue = item->text();
        if(newValue == entry.value)
            return; // nothing actually changed

        ExifChange change;
        change.kind = ExifChangeKind::SetValue;
        change.key = entry.key;
        change.value = newValue;
        // last edit wins: drop any earlier staged value for the same key
        for(int i = mChanges.count() - 1; i >= 0; --i) {
            if(mChanges.at(i).kind == ExifChangeKind::SetValue && mChanges.at(i).key == entry.key)
                mChanges.removeAt(i);
        }
        mChanges.append(change);
        refreshButtons();
    });

    reload();
}

ExifEditDialog::~ExifEditDialog() {
    delete ui;
}

void ExifEditDialog::applyLocalizedHeaders() {
    ui->table->setColumnCount(COL_COUNT);
    ui->table->setHorizontalHeaderLabels({
        tr("Name"), tr("Key"), tr("Type"), tr("Value")
    });
    ui->table->horizontalHeader()->setSectionResizeMode(COL_NAME, QHeaderView::Interactive);
    ui->table->horizontalHeader()->setSectionResizeMode(COL_KEY, QHeaderView::Interactive);
    ui->table->horizontalHeader()->setSectionResizeMode(COL_TYPE, QHeaderView::ResizeToContents);
    ui->table->horizontalHeader()->setSectionResizeMode(COL_VALUE, QHeaderView::Stretch);
}

void ExifEditDialog::reload() {
    QString error;
    mEntries = ExifWriter::readAll(mFilePath, &error);
    if(!error.isEmpty() && mEntries.isEmpty()) {
        QMessageBox::warning(this, tr("Could not read metadata"), error);
        return;
    }

    // Rebuilding the table fires itemChanged for every cell; block it, we only
    // want to react to real user edits.
    const bool blocked = ui->table->blockSignals(true);
    ui->table->setRowCount(mEntries.count());
    for(int row = 0; row < mEntries.count(); ++row) {
        const ExifEntryInfo &entry = mEntries.at(row);

        auto *nameItem = new QTableWidgetItem(entry.tagName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        auto *keyItem = new QTableWidgetItem(entry.key);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        auto *typeItem = new QTableWidgetItem(entry.typeName);
        typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
        auto *valueItem = new QTableWidgetItem(entry.value);
        if(!entry.writable)
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);

        ui->table->setItem(row, COL_NAME, nameItem);
        ui->table->setItem(row, COL_KEY, keyItem);
        ui->table->setItem(row, COL_TYPE, typeItem);
        ui->table->setItem(row, COL_VALUE, valueItem);
    }
    ui->table->blockSignals(blocked);
    refreshButtons();
}

void ExifEditDialog::markDeleteSelected() {
    const int row = ui->table->currentRow();
    if(row < 0 || row >= mEntries.count())
        return;
    const ExifEntryInfo &entry = mEntries.at(row);
    if(mDeletedKeys.contains(entry.key))
        return;

    ExifChange change;
    change.kind = ExifChangeKind::DeleteTag;
    change.key = entry.key;
    mChanges.append(change);
    mDeletedKeys.append(entry.key);

    // visual marker
    QTableWidgetItem *nameItem = ui->table->item(row, COL_NAME);
    if(nameItem) {
        QFont f = nameItem->font();
        f.setStrikeOut(true);
        nameItem->setFont(f);
        nameItem->setText(tr("%1 (deleted)").arg(entry.tagName));
    }
    // a deleted row must not be editable until the deletion is undone
    if(QTableWidgetItem *valueItem = ui->table->item(row, COL_VALUE)) {
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setToolTip(tr("Marked for deletion. Use \"Undo delete\" to restore."));
    }
    refreshButtons();
}

void ExifEditDialog::undoDeleteSelected() {
    const int row = ui->table->currentRow();
    if(row < 0 || row >= mEntries.count())
        return;
    const ExifEntryInfo &entry = mEntries.at(row);
    if(!mDeletedKeys.contains(entry.key))
        return;

    mDeletedKeys.removeAll(entry.key);
    // drop the matching DeleteTag change
    for(int i = mChanges.count() - 1; i >= 0; --i) {
        const ExifChange &c = mChanges.at(i);
        if(c.kind == ExifChangeKind::DeleteTag && c.key == entry.key)
            mChanges.removeAt(i);
    }

    QTableWidgetItem *nameItem = ui->table->item(row, COL_NAME);
    if(nameItem) {
        QFont f = nameItem->font();
        f.setStrikeOut(false);
        nameItem->setFont(f);
        nameItem->setText(entry.tagName);
    }
    if(QTableWidgetItem *valueItem = ui->table->item(row, COL_VALUE)) {
        if(entry.writable)
            valueItem->setFlags(valueItem->flags() | Qt::ItemIsEditable);
        valueItem->setToolTip(QString());
    }
    refreshButtons();
}

void ExifEditDialog::addTag() {
    bool ok = false;
    const QString namespaces = editableXmpNamespaces().join(QStringLiteral(", "));
    const QString ns = QInputDialog::getText(
        this, tr("Add tag"),
        tr("Namespace (one of: %1)").arg(namespaces),
        QLineEdit::Normal, QStringLiteral("dc"), &ok).trimmed();
    if(!ok || ns.isEmpty())
        return;
    if(!editableXmpNamespaces().contains(ns)) {
        QMessageBox::warning(this, tr("Add tag"),
                             tr("Unknown namespace.\nSupported: %1").arg(namespaces));
        return;
    }

    const QString tag = QInputDialog::getText(
        this, tr("Add tag"), tr("Tag name"), QLineEdit::Normal, QString(), &ok).trimmed();
    if(!ok || tag.isEmpty())
        return;

    const QString value = QInputDialog::getText(
        this, tr("Add tag"), tr("Value"), QLineEdit::Normal, QString(), &ok);
    if(!ok)
        return;

    ExifChange change;
    change.kind = ExifChangeKind::AddTag;
    change.key = QStringLiteral("Xmp.%1.%2").arg(ns, tag);
    change.value = value;

    // if an identical add is already staged, replace it
    for(int i = mChanges.count() - 1; i >= 0; --i) {
        if(mChanges.at(i).kind == ExifChangeKind::AddTag && mChanges.at(i).key == change.key)
            mChanges.removeAt(i);
    }
    mChanges.append(change);

    QMessageBox::information(this, tr("Add tag"),
                             tr("Tag queued for writing:\n%1 = %2").arg(change.key, value));
    refreshButtons();
}

void ExifEditDialog::refreshButtons() {
    const bool hasChanges = !mChanges.isEmpty();
    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(hasChanges);

    const int row = ui->table->currentRow();
    const bool validRow = row >= 0 && row < mEntries.count();
    const bool selectedIsDeleted = validRow && mDeletedKeys.contains(mEntries.at(row).key);

    // "Delete" only makes sense on a row that is not already staged for
    // deletion, and "Undo delete" only on one that is.
    ui->deleteButton->setEnabled(validRow && !selectedIsDeleted);
    ui->undoDeleteButton->setEnabled(validRow && selectedIsDeleted);
}

void ExifEditDialog::onSaveClicked() {
    if(mChanges.isEmpty()) {
        reject();
        return;
    }
    accept();
}

bool ExifEditDialog::hasChanges() const {
    return !mChanges.isEmpty();
}

QList<ExifChange> ExifEditDialog::changes() const {
    return mChanges;
}

int ExifEditDialog::exec() {
    return QDialog::exec();
}
