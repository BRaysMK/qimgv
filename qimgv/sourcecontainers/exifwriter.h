#pragma once

#include <QList>
#include <QString>

#include "sourcecontainers/exifedittypes.h"

// Reads and writes image metadata through exiv2.
//
// Writing is the dangerous part: Exiv2::Image::writeMetadata() replaces every
// metadata section, and any type without assigned values is REMOVED from the
// image. Only assigning ExifData would therefore silently wipe XMP (which is
// where DJI flight data lives) and IPTC. apply() takes a snapshot of all
// sections first and writes them back, so untouched metadata survives.
class ExifWriter {
public:
    // True if this file type can be written back (extension whitelist).
    static bool supportsWriting(const QString &path);

    // Read every tag of the file, for display in the editor.
    static QList<ExifEntryInfo> readAll(const QString &path, QString *error = nullptr);

    // Apply the requested changes in place. Atomic: the file is only replaced
    // if the new metadata was written successfully.
    static ExifEditResult apply(const QString &path, const QList<ExifChange> &changes);
};
