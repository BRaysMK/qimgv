#include "exifedithistory.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QDebug>

#include "settings.h"

namespace {

// In-memory map filePath -> snapshot file, for the current session only.
QHash<QString, QString> &snapshots() {
    static QHash<QString, QString> map;
    return map;
}

} // namespace

QString ExifEditHistory::snapshotDir() {
    return settings->tmpDir() + QStringLiteral("/qimsvg-exif-snap");
}

QString ExifEditHistory::snapshotPathFor(const QString &filePath) {
    // Keep the original suffix so the copy stays a valid image and so a manual
    // inspection of the snapshot folder is meaningful.
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Md5).toHex());
    QFileInfo fi(filePath);
    return snapshotDir() + "/" + hash + "." + fi.suffix().toLower();
}

bool ExifEditHistory::createSnapshot(const QString &filePath, QString *error) {
    auto fail = [error](const QString &msg) {
        if(error)
            *error = msg;
        qDebug() << "[ExifEditHistory] createSnapshot failed:" << msg;
        return false;
    };

    QFileInfo fi(filePath);
    if(!fi.exists() || !fi.isFile())
        return fail(QStringLiteral("File does not exist"));

    // Already have a restore point for this file: keep it. It captures the
    // state from before the FIRST edit, which is what the user expects to
    // return to.
    if(snapshots().contains(filePath) && QFile::exists(snapshots().value(filePath)))
        return true;

    QDir dir;
    if(!dir.mkpath(snapshotDir()))
        return fail(QStringLiteral("Could not create snapshot directory"));

    const QString target = snapshotPathFor(filePath);
    QFile::remove(target);
    if(!QFile::copy(filePath, target))
        return fail(QStringLiteral("Could not copy file for snapshot"));

    snapshots().insert(filePath, target);
    return true;
}

bool ExifEditHistory::hasSnapshot(const QString &filePath) {
    const auto it = snapshots().constFind(filePath);
    return it != snapshots().constEnd() && QFile::exists(it.value());
}

bool ExifEditHistory::restoreSnapshot(const QString &filePath, QString *error) {
    auto fail = [error](const QString &msg) {
        if(error)
            *error = msg;
        qDebug() << "[ExifEditHistory] restoreSnapshot failed:" << msg;
        return false;
    };

    const auto it = snapshots().constFind(filePath);
    if(it == snapshots().constEnd())
        return fail(QStringLiteral("No restore point for this file"));

    const QString snap = it.value();
    if(!QFile::exists(snap))
        return fail(QStringLiteral("Snapshot file is missing"));

    // Copy back through a temporary file so a failure cannot destroy the
    // current file (which is still a valid image, just edited).
    const QString tmp = filePath + QStringLiteral(".qimsvg-restore.tmp");
    QFile::remove(tmp);
    if(!QFile::copy(snap, tmp))
        return fail(QStringLiteral("Could not stage restore"));

    QFile::remove(filePath);
    if(!QFile::rename(tmp, filePath)) {
        // put the snapshot in place directly as a last resort
        QFile::remove(tmp);
        if(!QFile::copy(snap, filePath))
            return fail(QStringLiteral("Could not restore file from snapshot"));
    }

    // one-shot: the restore point is consumed
    QFile::remove(snap);
    snapshots().remove(filePath);
    QFileInfo(filePath).refresh();
    return true;
}

void ExifEditHistory::discardSnapshot(const QString &filePath) {
    const auto it = snapshots().find(filePath);
    if(it == snapshots().end())
        return;
    QFile::remove(it.value());
    snapshots().erase(it);
}

void ExifEditHistory::clearAll() {
    for(const QString &snap : snapshots())
        QFile::remove(snap);
    snapshots().clear();
    QDir(snapshotDir()).removeRecursively();
}
