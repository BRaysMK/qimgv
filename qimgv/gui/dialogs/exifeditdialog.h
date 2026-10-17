#pragma once

#include <QDialog>
#include <QList>

#include "sourcecontainers/exifedittypes.h"

namespace Ui {
class ExifEditDialog;
}

// Lets the user view every metadata tag of an image and stage three kinds of
// edits: change a value, delete a tag, add a tag in an existing namespace.
// Nothing is written here - the dialog only collects ExifChange items, which
// the caller applies after exec() returns Accepted.
class ExifEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExifEditDialog(const QString &filePath, QWidget *parent = nullptr);
    ~ExifEditDialog();

    // True if the user staged at least one change.
    bool hasChanges() const;
    // Changes to apply, in the order they were staged.
    QList<ExifChange> changes() const;

public slots:
    int exec() override;

private slots:
    void markDeleteSelected();
    void undoDeleteSelected();
    void addTag();
    void onSaveClicked();

private:
    void reload();
    void refreshButtons();
    void applyLocalizedHeaders();

    Ui::ExifEditDialog *ui;
    QString mFilePath;
    QList<ExifEntryInfo> mEntries;
    QList<ExifChange> mChanges;
    // keys staged for deletion, so the row can be struck through
    QList<QString> mDeletedKeys;
};
