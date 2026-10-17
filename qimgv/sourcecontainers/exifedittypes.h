#pragma once

#include <QString>
#include <QStringList>

// A single metadata modification requested by the user.
//
// Keys use the full exiv2 form, e.g. "Exif.Image.Artist", "Xmp.dc.source" or
// "Iptc.Application2.Caption". Adding tags is intentionally restricted to
// already-registered namespaces: exiv2 rejects unknown ones ("Invalid key"),
// and such tags would be invisible to other software anyway.
enum class ExifChangeKind {
    SetValue,   // change the value of an existing tag
    DeleteTag,  // remove the tag entirely
    AddTag      // create a new tag inside an existing (registered) namespace
};

struct ExifChange {
    ExifChangeKind kind = ExifChangeKind::SetValue;
    QString key;
    QString value;
};

struct ExifEditResult {
    bool ok = false;
    QString error;

    static ExifEditResult success() {
        return {true, QString()};
    }
    static ExifEditResult failure(const QString &msg) {
        return {false, msg};
    }
};

// One row of the tag list shown in the editor.
struct ExifEntryInfo {
    QString key;        // "Exif.Image.Artist"
    QString group;      // "Exif.Image"
    QString tagName;    // "Artist"
    QString typeName;   // "Ascii", "Short", ...
    QString value;      // human readable value
    bool writable = false;  // true if the value may be edited in place
};

// Namespaces that exiv2 accepts for newly created tags. "Exif.*" groups are
// handled separately in the writer (they are not XMP).
inline QStringList editableXmpNamespaces() {
    return {
        QStringLiteral("dc"),
        QStringLiteral("xmp"),
        QStringLiteral("tiff"),
        QStringLiteral("exif"),
        QStringLiteral("photoshop"),
        QStringLiteral("Iptc4xmpCore"),
        QStringLiteral("crs"),
        QStringLiteral("aux"),
        QStringLiteral("lr")
    };
}
