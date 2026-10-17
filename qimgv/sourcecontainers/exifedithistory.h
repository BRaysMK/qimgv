#pragma once

#include <QString>

// One "restore point" for a file whose metadata is about to be edited.
//
// IMPORTANT: snapshots are expensive (they copy the whole image), so they are
// created ONLY when the user opens the metadata editor or confirms a save.
// Browsing, changing images, toggling the info panel and every other read-only
// path must never call into this class.
class ExifEditHistory {
public:
    // Copy `filePath` to the snapshot area and remember it as the restore point
    // for that file. If a snapshot for the same file already exists it is
    // reused (one restore point per file, so repeated Ctrl+E does not pile up
    // multi-megabyte copies).
    static bool createSnapshot(const QString &filePath, QString *error = nullptr);

    // True if a restore point exists for this file in the current session.
    static bool hasSnapshot(const QString &filePath);

    // Copy the snapshot back over the file. Consumes the snapshot.
    static bool restoreSnapshot(const QString &filePath, QString *error = nullptr);

    // Drop a restore point without restoring (used when the editor is closed
    // without changes, so no leftover copies remain).
    static void discardSnapshot(const QString &filePath);

    // Remove the whole snapshot area. Called on shutdown.
    static void clearAll();

private:
    static QString snapshotDir();
    static QString snapshotPathFor(const QString &filePath);
};
