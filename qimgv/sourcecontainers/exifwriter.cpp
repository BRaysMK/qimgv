#include "exifwriter.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QDebug>

#include "utils/stuff.h"

#ifdef USE_EXIV2
#include <exiv2/exiv2.hpp>
#endif

namespace {

// Extensions we know exiv2 can read AND write back in place.
const QSet<QString> &writableExtensions() {
    static const QSet<QString> exts = {
        "jpg", "jpeg", "jpe", "jfif",
        "tif", "tiff",
        "webp",
        "png",
        "heic", "heif",
        "avif"
    };
    return exts;
}

#ifdef USE_EXIV2

// Expensive but rare, so a plain split is fine.
bool isExifKey(const QString &key) {
    return key.startsWith("Exif.", Qt::CaseInsensitive);
}

bool isXmpKey(const QString &key) {
    return key.startsWith("Xmp.", Qt::CaseInsensitive);
}

bool isIptcKey(const QString &key) {
    return key.startsWith("Iptc.", Qt::CaseInsensitive);
}

#endif // USE_EXIV2

} // namespace

bool ExifWriter::supportsWriting(const QString &path) {
    QFileInfo fi(path);
    return writableExtensions().contains(fi.suffix().toLower());
}

#ifdef USE_EXIV2

QList<ExifEntryInfo> ExifWriter::readAll(const QString &path, QString *error) {
    QList<ExifEntryInfo> out;
    auto setError = [error](const QString &msg) {
        if(error)
            *error = msg;
        qDebug() << "[ExifWriter] readAll:" << msg;
    };

    if(!supportsWriting(path)) {
        setError(QStringLiteral("Unsupported file type for metadata editing"));
        return out;
    }

    try {
        std::unique_ptr<Exiv2::Image> image = Exiv2::ImageFactory::open(toStdString(path));
        if(!image) {
            setError(QStringLiteral("Could not open file"));
            return out;
        }
        image->readMetadata();

        auto appendFrom = [&out](const auto &md) {
            for(const auto &datum : md) {
                ExifEntryInfo info;
                // decodeMetadataBytes: metadata bytes are not guaranteed UTF-8;
                // without it GBK/ANSI values would display (and get written
                // back) as mojibake.
                info.key = decodeMetadataBytes(datum.key());
                info.group = decodeMetadataBytes(std::string(datum.groupName()));
                info.tagName = decodeMetadataBytes(std::string(datum.tagName()));
                info.typeName = decodeMetadataBytes(std::string(datum.typeName()));
                // binary payloads would render as a wall of noise
                if(datum.typeId() == Exiv2::undefined)
                    info.value = QStringLiteral("(binary data %1 bytes)")
                                     .arg(static_cast<qlonglong>(datum.size()));
                else
                    info.value = decodeMetadataBytes(datum.value().toString());
                // Ascii/XmpText and numbers can be edited as text; binary can not
                info.writable = (datum.typeId() != Exiv2::undefined);
                out.append(info);
            }
        };

        appendFrom(image->exifData());
        appendFrom(image->xmpData());
        appendFrom(image->iptcData());
    } catch(const Exiv2::Error &e) {
        setError(QString::fromStdString(e.what()));
    } catch(const std::exception &e) {
        setError(QString::fromStdString(e.what()));
    } catch(...) {
        setError(QStringLiteral("Unexpected error while reading metadata"));
    }
    return out;
}

ExifEditResult ExifWriter::apply(const QString &path, const QList<ExifChange> &changes) {
    if(changes.isEmpty())
        return ExifEditResult::success();

    if(!supportsWriting(path))
        return ExifEditResult::failure(QStringLiteral("Unsupported file type for metadata editing"));

    QFileInfo fi(path);
    if(!fi.exists())
        return ExifEditResult::failure(QStringLiteral("File does not exist"));
    if(!fi.isWritable())
        return ExifEditResult::failure(QStringLiteral("File is read-only"));

    // exiv2 can only rewrite the file in place. Safety against a damaged file
    // comes from ExifEditHistory, which snapshots the whole original before any
    // of this is called (see Core::showMetadataEditor).
    try {
        std::unique_ptr<Exiv2::Image> image = Exiv2::ImageFactory::open(toStdString(path));
        if(!image)
            return ExifEditResult::failure(QStringLiteral("Could not open file"));
        image->readMetadata();

        // readMetadata() above has already populated ALL sections (exif, xmp,
        // iptc) inside `image`. writeMetadata() only drops a section when it
        // holds no values, so editing these containers in place keeps every
        // untouched tag - including the DJI `Xmp.drone-dji.*` flight data.
        // (Do NOT re-assign a snapshot over a section here: that would discard
        // the user's own changes to that same section.)
        Exiv2::ExifData &exifData = image->exifData();
        Exiv2::XmpData &xmpData = image->xmpData();
        Exiv2::IptcData &iptcData = image->iptcData();

        // 2. Apply the requested changes.
        for(const ExifChange &change : changes) {
            const std::string key = change.key.toStdString();
            const bool isExif = isExifKey(change.key);
            const bool isXmp = isXmpKey(change.key);
            const bool isIptc = isIptcKey(change.key);

            if(!isExif && !isXmp && !isIptc) {
                return ExifEditResult::failure(
                    QStringLiteral("Unsupported metadata key: %1").arg(change.key));
            }

            switch(change.kind) {
            case ExifChangeKind::SetValue:
            case ExifChangeKind::AddTag: {
                const std::string value = change.value.toStdString();
                if(isExif) {
                    exifData[key] = value;
                } else if(isXmp) {
                    xmpData[key] = value;
                } else {
                    iptcData[key] = value;
                }
                break;
            }
            case ExifChangeKind::DeleteTag: {
                if(isExif) {
                    auto it = exifData.findKey(Exiv2::ExifKey(key));
                    if(it != exifData.end())
                        exifData.erase(it);
                } else if(isXmp) {
                    auto it = xmpData.findKey(Exiv2::XmpKey(key));
                    if(it != xmpData.end())
                        xmpData.erase(it);
                } else {
                    auto it = iptcData.findKey(Exiv2::IptcKey(key));
                    if(it != iptcData.end())
                        iptcData.erase(it);
                }
                break;
            }
            }
        }

        // 3. Persist. writeMetadata() rewrites the original file in place.
        image->writeMetadata();
    } catch(const Exiv2::Error &e) {
        return ExifEditResult::failure(QString::fromStdString(e.what()));
    } catch(const std::exception &e) {
        return ExifEditResult::failure(QString::fromStdString(e.what()));
    } catch(...) {
        return ExifEditResult::failure(QStringLiteral("Unexpected error while writing metadata"));
    }

    // pick up the new size/mtime so the model notices the change
    fi.refresh();
    return ExifEditResult::success();
}

#else // !USE_EXIV2

QList<ExifEntryInfo> ExifWriter::readAll(const QString &path, QString *error) {
    Q_UNUSED(path)
    if(error)
        *error = QStringLiteral("Metadata editing is not available (built without exiv2)");
    return {};
}

ExifEditResult ExifWriter::apply(const QString &path, const QList<ExifChange> &changes) {
    Q_UNUSED(path)
    Q_UNUSED(changes)
    return ExifEditResult::failure(
        QStringLiteral("Metadata editing is not available (built without exiv2)"));
}

#endif // USE_EXIV2
