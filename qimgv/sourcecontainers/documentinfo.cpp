#include "documentinfo.h"
#include <QStringList>
#include <QHash>
#include <QRegularExpression>

DocumentInfo::DocumentInfo(QString path)
    : mDocumentType(DocumentType::NONE),
      mOrientation(0),
      mFormat(""),
      exifLoaded(false)
{
    fileInfo.setFile(path);
    if(!fileInfo.isFile()) {
        qDebug() << "FileInfo: cannot open: " << path;
        return;
    }
    detectFormat();
}

DocumentInfo::~DocumentInfo() {
}

// ##############################################################
// ####################### PUBLIC METHODS #######################
// ##############################################################

QString DocumentInfo::directoryPath() const {
    return fileInfo.absolutePath();
}

QString DocumentInfo::filePath() const {
    return fileInfo.absoluteFilePath();
}

QString DocumentInfo::fileName() const {
    return fileInfo.fileName();
}

QString DocumentInfo::baseName() const {
    return fileInfo.baseName();
}

// bytes
qint64 DocumentInfo::fileSize() const {
    return fileInfo.size();
}

DocumentType DocumentInfo::type() const {
    return mDocumentType;
}

QMimeType DocumentInfo::mimeType() const {
    return mMimeType;
}

QString DocumentInfo::format() const {
    return mFormat;
}

QDateTime DocumentInfo::lastModified() const {
    return fileInfo.lastModified();
}

// For cases like orientation / even mimetype change we just reload
// Image from scratch, so don`t bother handling it here
void DocumentInfo::refresh() {
    fileInfo.refresh();
}

int DocumentInfo::exifOrientation() const {
    return mOrientation;
}

// ##############################################################
// ####################### PRIVATE METHODS ######################
// ##############################################################
void DocumentInfo::detectFormat() {
    if(mDocumentType != DocumentType::NONE)
        return;
    QMimeDatabase mimeDb;
    mMimeType = mimeDb.mimeTypeForFile(fileInfo.filePath(), QMimeDatabase::MatchContent);
    auto mimeName = mMimeType.name().toUtf8();
    auto suffix = fileInfo.suffix().toLower().toUtf8();
    if(mimeName == "image/jpeg") {
        mFormat = "jpg";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/png") {
        if(QImageReader::supportedImageFormats().contains("apng") && detectAPNG()) {
            mFormat = "apng";
            mDocumentType = DocumentType::ANIMATED;
        } else {
            mFormat = "png";
            mDocumentType = DocumentType::STATIC;
        }
    } else if(mimeName == "image/gif") {
        mFormat = "gif";
        mDocumentType = DocumentType::ANIMATED;
    } else if(mimeName == "image/webp" || (mimeName == "audio/x-riff" && suffix == "webp")) {
        mFormat = "webp";
        mDocumentType = detectAnimatedWebP() ? DocumentType::ANIMATED : DocumentType::STATIC;
    } else if(mimeName == "image/jxl") {
        mFormat = "jxl";
        mDocumentType = detectAnimatedJxl() ? DocumentType::ANIMATED : DocumentType::STATIC;
        if(mDocumentType == DocumentType::ANIMATED && !settings->jxlAnimation()) {
            mDocumentType = DocumentType::NONE;
            qDebug() << "animated jxl is off; skipping file";
        }
    } else if(mimeName == "image/avif") {
        mFormat = "avif";
        mDocumentType = detectAnimatedAvif() ? DocumentType::ANIMATED : DocumentType::STATIC;
    } else if(mimeName == "image/bmp") {
        mFormat = "bmp";
        mDocumentType = DocumentType::STATIC;
    } else if(settings->videoPlayback() && settings->videoFormats().contains(mimeName)) {
        mDocumentType = DocumentType::VIDEO;
        mFormat = settings->videoFormats().value(mimeName);
    } else {
        // just try to open via suffix if all of the above fails
        mFormat = suffix;
        if(mFormat.compare("jfif", Qt::CaseInsensitive) == 0)
            mFormat = "jpg";
        if(settings->videoPlayback() && settings->videoFormats().values().contains(suffix))
            mDocumentType = DocumentType::VIDEO;
        else
            mDocumentType = DocumentType::STATIC;
    }
    loadExifOrientation();
}

inline
// dumb apng detector
bool DocumentInfo::detectAPNG() {
    QFile f(fileInfo.filePath());
    if(f.open(QFile::ReadOnly)) {
        QDataStream in(&f);
        const int len = 120;
        QByteArray qbuf("\0", len);
        if (in.readRawData(qbuf.data(), len) > 0) {
            return qbuf.contains("acTL");
        }
    }
    return false;
}

bool DocumentInfo::detectAnimatedWebP() {
    QFile f(fileInfo.filePath());
    bool result = false;
    if(f.open(QFile::ReadOnly)) {
        QDataStream in(&f);
        in.skipRawData(12);
        char *buf = static_cast<char*>(malloc(5));
        buf[4] = '\0';
        in.readRawData(buf, 4);
        if(strcmp(buf, "VP8X") == 0) {
            in.skipRawData(4);
            char flags;
            in.readRawData(&flags, 1);
            if(flags & (1 << 1)) {
                result = true;
            }
        }
        free(buf);
    }
    return result;
}

// TODO avoid creating multiple QImageReader instances
bool DocumentInfo::detectAnimatedJxl() {
    QImageReader r(fileInfo.filePath(), "jxl");
    return r.supportsAnimation();
}

bool DocumentInfo::detectAnimatedAvif() {
    QFile f(fileInfo.filePath());
    bool result = false;
    if(f.open(QFile::ReadOnly)) {
        QDataStream in(&f);
        in.skipRawData(4); // skip box size
        char *buf = static_cast<char*>(malloc(9));
        buf[8] = '\0';
        in.readRawData(buf, 8);
        if(strcmp(buf, "ftypavis") == 0) {
            result = true;
        }
        free(buf);
    }
    return result;
}

// Localize the most common EXIF tag names ("group.tag" -> Chinese).
// Unknown tags keep their original (English) name.
static QString localizedTagName(const QString &raw) {
    static const QHash<QString, QString> names = {
        // --- Exif.Image ---
        {"Image.ImageWidth", QStringLiteral("图像宽度")},
        {"Image.ImageLength", QStringLiteral("图像高度")},
        {"Image.BitsPerSample", QStringLiteral("每样本位数")},
        {"Image.Compression", QStringLiteral("压缩方式")},
        {"Image.PhotometricInterpretation", QStringLiteral("光度解释")},
        {"Image.Orientation", QStringLiteral("方向")},
        {"Image.SamplesPerPixel", QStringLiteral("每像素样本数")},
        {"Image.XResolution", QStringLiteral("水平分辨率")},
        {"Image.YResolution", QStringLiteral("垂直分辨率")},
        {"Image.ResolutionUnit", QStringLiteral("分辨率单位")},
        {"Image.NewSubfileType", QStringLiteral("子文件类型")},
        {"Image.StripOffsets", QStringLiteral("数据偏移")},
        {"Image.StripByteCounts", QStringLiteral("数据长度")},
        {"Image.Software", QStringLiteral("软件")},
        {"Image.DateTime", QStringLiteral("日期时间")},
        {"Image.Artist", QStringLiteral("作者")},
        {"Image.Copyright", QStringLiteral("版权")},
        {"Image.ImageDescription", QStringLiteral("图像描述")},
        {"Image.Make", QStringLiteral("相机制造商")},
        {"Image.Model", QStringLiteral("相机型号")},
        {"Image.ExifTag", QStringLiteral("Exif指针")},
        {"Image.GPSTag", QStringLiteral("GPS指针")},
        {"Image.XPTitle", QStringLiteral("XP标题")},
        {"Image.XPComment", QStringLiteral("XP注释")},
        {"Image.XPAuthor", QStringLiteral("XP作者")},
        {"Image.XPKeywords", QStringLiteral("XP关键字")},
        {"Image.XPSubject", QStringLiteral("XP主题")},
        {"Image.YCbCrPositioning", QStringLiteral("YCbCr分量位置")},
        {"Image.YCbCrSubSampling", QStringLiteral("YCbCr子采样")},
        {"Image.TransferFunction", QStringLiteral("传输函数")},
        {"Image.WhitePoint", QStringLiteral("白点")},
        {"Image.PrimaryChromaticities", QStringLiteral("原色色度")},
        {"Image.ReferenceBlackWhite", QStringLiteral("黑白参考值")},
        {"Image.DocumentName", QStringLiteral("文档名称")},
        {"Image.HostComputer", QStringLiteral("主机名")},
        {"Image.ProfileType", QStringLiteral("配置文件类型")},
        {"Image.PlanarConfiguration", QStringLiteral("平面配置")},
        // --- Exif.Photo ---
        {"Photo.ExposureTime", QStringLiteral("曝光时间")},
        {"Photo.FNumber", QStringLiteral("光圈数")},
        {"Photo.ExposureProgram", QStringLiteral("曝光程序")},
        {"Photo.ISOSpeedRatings", QStringLiteral("ISO感光度")},
        {"Photo.ExifVersion", QStringLiteral("Exif版本")},
        {"Photo.DateTimeOriginal", QStringLiteral("原始拍摄时间")},
        {"Photo.DateTimeDigitized", QStringLiteral("数字化时间")},
        {"Photo.ComponentsConfiguration", QStringLiteral("分量配置")},
        {"Photo.CompressedBitsPerPixel", QStringLiteral("压缩位/像素")},
        {"Photo.ShutterSpeedValue", QStringLiteral("快门速度")},
        {"Photo.ApertureValue", QStringLiteral("光圈值")},
        {"Photo.BrightnessValue", QStringLiteral("亮度值")},
        {"Photo.ExposureBiasValue", QStringLiteral("曝光补偿")},
        {"Photo.MaxApertureValue", QStringLiteral("最大光圈")},
        {"Photo.SubjectDistance", QStringLiteral("拍摄距离")},
        {"Photo.MeteringMode", QStringLiteral("测光模式")},
        {"Photo.LightSource", QStringLiteral("光源")},
        {"Photo.Flash", QStringLiteral("闪光灯")},
        {"Photo.FocalLength", QStringLiteral("焦距")},
        {"Photo.SubjectArea", QStringLiteral("主体区域")},
        {"Photo.MakerNote", QStringLiteral("厂商备注")},
        {"Photo.UserComment", QStringLiteral("用户注释")},
        {"Photo.SubSecTime", QStringLiteral("秒以下时间")},
        {"Photo.SubSecTimeOriginal", QStringLiteral("原始拍摄时间(亚秒)")},
        {"Photo.SubSecTimeDigitized", QStringLiteral("数字化时间(亚秒)")},
        {"Photo.FlashpixVersion", QStringLiteral("FlashPix版本")},
        {"Photo.ColorSpace", QStringLiteral("色彩空间")},
        {"Photo.PixelXDimension", QStringLiteral("像素宽度")},
        {"Photo.PixelYDimension", QStringLiteral("像素高度")},
        {"Photo.RelatedSoundFile", QStringLiteral("相关音频文件")},
        {"Photo.FocalPlaneXResolution", QStringLiteral("焦平面水平分辨率")},
        {"Photo.FocalPlaneYResolution", QStringLiteral("焦平面垂直分辨率")},
        {"Photo.FocalPlaneResolutionUnit", QStringLiteral("焦平面分辨率单位")},
        {"Photo.SensingMethod", QStringLiteral("感光方式")},
        {"Photo.FileSource", QStringLiteral("文件来源")},
        {"Photo.SceneType", QStringLiteral("场景类型")},
        {"Photo.CustomRendered", QStringLiteral("自定义渲染")},
        {"Photo.ExposureMode", QStringLiteral("曝光模式")},
        {"Photo.WhiteBalance", QStringLiteral("白平衡")},
        {"Photo.DigitalZoomRatio", QStringLiteral("数码变焦比率")},
        {"Photo.FocalLengthIn35mmFilm", QStringLiteral("35mm等效焦距")},
        {"Photo.SceneCaptureType", QStringLiteral("场景拍摄类型")},
        {"Photo.GainControl", QStringLiteral("增益控制")},
        {"Photo.Contrast", QStringLiteral("对比度")},
        {"Photo.Saturation", QStringLiteral("饱和度")},
        {"Photo.Sharpness", QStringLiteral("锐度")},
        {"Photo.SubjectDistanceRange", QStringLiteral("主体距离范围")},
        {"Photo.ImageUniqueID", QStringLiteral("图像唯一ID")},
        {"Photo.LensSpecification", QStringLiteral("镜头规格")},
        {"Photo.LensModel", QStringLiteral("镜头型号")},
        {"Photo.LensMake", QStringLiteral("镜头制造商")},
        {"Photo.PhotographicSensitivity", QStringLiteral("感光度")},
        {"Photo.SensitivityType", QStringLiteral("感光度类型")},
        {"Photo.OffsetTime", QStringLiteral("时区偏移")},
        {"Photo.OffsetTimeOriginal", QStringLiteral("拍摄时区偏移")},
        {"Photo.OffsetTimeDigitized", QStringLiteral("数字化时区偏移")},
        {"Photo.CameraOwnerName", QStringLiteral("相机所有者")},
        {"Photo.BodySerialNumber", QStringLiteral("机身序列号")},
        {"Photo.LensSerialNumber", QStringLiteral("镜头序列号")},
        {"Photo.InteroperabilityTag", QStringLiteral("互操作性指针")},
        {"Photo.DeviceSettingDescription", QStringLiteral("设备设置描述")},
        {"Photo.OECF", QStringLiteral("光电转换函数(OECF)")},
        {"Photo.SpectralSensitivity", QStringLiteral("光谱灵敏度")},
        {"Photo.SpatialFrequencyResponse", QStringLiteral("空间频率响应")},
        // --- Exif.GPSInfo ---
        {"GPSInfo.GPSVersionID", QStringLiteral("GPS版本")},
        {"GPSInfo.GPSLatitudeRef", QStringLiteral("纬度方向")},
        {"GPSInfo.GPSLatitude", QStringLiteral("纬度")},
        {"GPSInfo.GPSLongitudeRef", QStringLiteral("经度方向")},
        {"GPSInfo.GPSLongitude", QStringLiteral("经度")},
        {"GPSInfo.GPSAltitudeRef", QStringLiteral("海拔参考")},
        {"GPSInfo.GPSAltitude", QStringLiteral("海拔")},
        {"GPSInfo.GPSTimeStamp", QStringLiteral("GPS时间")},
        {"GPSInfo.GPSSatellites", QStringLiteral("GPS卫星")},
        {"GPSInfo.GPSStatus", QStringLiteral("GPS状态")},
        {"GPSInfo.GPSMeasureMode", QStringLiteral("GPS测量模式")},
        {"GPSInfo.GPSDOP", QStringLiteral("GPS精度因子")},
        {"GPSInfo.GPSSpeedRef", QStringLiteral("速度单位")},
        {"GPSInfo.GPSSpeed", QStringLiteral("速度")},
        {"GPSInfo.GPSTrackRef", QStringLiteral("方向参考")},
        {"GPSInfo.GPSTrack", QStringLiteral("方向")},
        {"GPSInfo.GPSImgDirectionRef", QStringLiteral("图像方向参考")},
        {"GPSInfo.GPSImgDirection", QStringLiteral("图像方向")},
        {"GPSInfo.GPSMapDatum", QStringLiteral("地图基准")},
        {"GPSInfo.GPSDestLatitudeRef", QStringLiteral("目标纬度方向")},
        {"GPSInfo.GPSDestLatitude", QStringLiteral("目标纬度")},
        {"GPSInfo.GPSDestLongitudeRef", QStringLiteral("目标经度方向")},
        {"GPSInfo.GPSDestLongitude", QStringLiteral("目标经度")},
        {"GPSInfo.GPSDestBearingRef", QStringLiteral("目标方位参考")},
        {"GPSInfo.GPSDestBearing", QStringLiteral("目标方位")},
        {"GPSInfo.GPSDestDistanceRef", QStringLiteral("目标距离单位")},
        {"GPSInfo.GPSDestDistance", QStringLiteral("目标距离")},
        {"GPSInfo.GPSProcessingMethod", QStringLiteral("GPS处理方法")},
        {"GPSInfo.GPSAreaInformation", QStringLiteral("GPS区域信息")},
        {"GPSInfo.GPSDateStamp", QStringLiteral("GPS日期")},
        {"GPSInfo.GPSHPositioningError", QStringLiteral("GPS定位误差")},
        // --- Exif.Iop (Interoperability) ---
        {"Iop.InteroperabilityIndex", QStringLiteral("互操作性索引")},
        {"Iop.InteroperabilityVersion", QStringLiteral("互操作性版本")},
        {"Iop.RelatedImageFileFormat", QStringLiteral("相关图像文件格式")},
        {"Iop.RelatedImageWidth", QStringLiteral("相关图像宽度")},
        {"Iop.RelatedImageLength", QStringLiteral("相关图像高度")},
        // --- Exif.Thumbnail ---
        {"Thumbnail.ImageWidth", QStringLiteral("缩略图宽度")},
        {"Thumbnail.ImageLength", QStringLiteral("缩略图高度")},
        {"Thumbnail.Compression", QStringLiteral("缩略图压缩")},
        {"Thumbnail.XResolution", QStringLiteral("缩略图水平分辨率")},
        {"Thumbnail.YResolution", QStringLiteral("缩略图垂直分辨率")},
        {"Thumbnail.JPEGInterchangeFormat", QStringLiteral("缩略图偏移")},
        {"Thumbnail.JPEGInterchangeFormatLength", QStringLiteral("缩略图长度")},
        {"Thumbnail.ResolutionUnit", QStringLiteral("缩略图分辨率单位")},
        {"Thumbnail.Orientation", QStringLiteral("缩略图方向")},
        {"Thumbnail.BitsPerSample", QStringLiteral("缩略图每样本位数")},
        {"Thumbnail.SamplesPerPixel", QStringLiteral("缩略图每像素样本数")}
    };
    return names.value(raw, raw);
}

// Friendly formatting for a few raw exiv2 values:
// - DateTime fields use "YYYY:MM:DD HH:MM:SS" (colons are the EXIF standard);
//   show them as "YYYY-MM-DD HH:MM:SS" instead.
// - GPS coordinates are rational arrays in exiv2 0.27 ("23/1 14/1 3080/100",
//   slash-separated); show "23°14'30.80"" (degrees/minutes/seconds) instead.
static QString formatExifValue(const QString &name, const QString &value) {
    if(name == "Image.DateTime" || name == "Photo.DateTimeOriginal" || name == "Photo.DateTimeDigitized") {
        static const QRegularExpression dateRe("^(\d{4}):(\d{2}):(\d{2})(.*)$");
        QRegularExpressionMatch m = dateRe.match(value);
        if(m.hasMatch())
            return m.captured(1) + "-" + m.captured(2) + "-" + m.captured(3) + m.captured(4);
    } else if(name == "GPSInfo.GPSLatitude" || name == "GPSInfo.GPSLongitude") {
        // exiv2 0.27 rational-array form: "23/1 14/1 3080/100"
        static const QRegularExpression fracRe("^(\d+)/(\d+)\s+(\d+)/(\d+)\s+([\d.]+)/(\d+)$");
        QRegularExpressionMatch m = fracRe.match(value);
        if(m.hasMatch()) {
            double deg = m.captured(1).toDouble() / m.captured(2).toDouble();
            double min = m.captured(3).toDouble() / m.captured(4).toDouble();
            double sec = m.captured(5).toDouble() / m.captured(6).toDouble();
            return QString::number(deg, 'f', 0) + "°" + QString::number(min, 'f', 0)
                   + "'" + QString::number(sec, 'f', 2) + "\"";
        }
        // exiv2 0.28 degree form: "23 deg 14' 30.80""
        static const QRegularExpression degRe("^(\d+(?:\.\d+)?)\s*(?:deg|d)\s*(\d+(?:\.\d+)?)'\s*([\d.]+)\"?$");
        m = degRe.match(value);
        if(m.hasMatch())
            return m.captured(1) + "°" + m.captured(2) + "'" + m.captured(3) + "\"";
    }
    return value;
}

// Localize DJI drone flight-data XMP tags (tag name only).
static QString localizedXmpTagName(const QString &tag) {
    static const QHash<QString, QString> names = {
        {"AbsoluteAltitude", QStringLiteral("绝对海拔")},
        {"RelativeAltitude", QStringLiteral("相对海拔")},
        {"GimbalRollDegree", QStringLiteral("云台横滚角")},
        {"GimbalYawDegree", QStringLiteral("云台偏航角")},
        {"GimbalPitchDegree", QStringLiteral("云台俯仰角")},
        {"FlightRollDegree", QStringLiteral("飞行横滚角")},
        {"FlightYawDegree", QStringLiteral("飞行偏航角")},
        {"FlightPitchDegree", QStringLiteral("飞行俯仰角")},
        {"FlightXSpeed", QStringLiteral("飞行X速度")},
        {"FlightYSpeed", QStringLiteral("飞行Y速度")},
        {"FlightZSpeed", QStringLiteral("飞行Z速度")},
        {"DroneModel", QStringLiteral("无人机型号")},
        {"DroneSerialNumber", QStringLiteral("无人机序列号")},
        {"SurveyingMode", QStringLiteral("测绘模式")},
        {"CalibratedFocalLength", QStringLiteral("校准焦距")},
        {"CalibratedOpticalCenterX", QStringLiteral("校准光心X")},
        {"CalibratedOpticalCenterY", QStringLiteral("校准光心Y")},
        {"CamReverse", QStringLiteral("相机反转")},
        {"GimbalReverse", QStringLiteral("云台反转")},
        {"PictureQuality", QStringLiteral("图片质量")},
        {"ShutterType", QStringLiteral("快门类型")},
        {"Version", QStringLiteral("版本")},
        {"ImageSource", QStringLiteral("图像源")},
        {"GpsStatus", QStringLiteral("GPS状态")},
        {"AltitudeType", QStringLiteral("海拔类型")},
        {"GpsLatitude", QStringLiteral("GPS纬度")},
        {"GpsLongitude", QStringLiteral("GPS经度")},
        {"CaptureUUID", QStringLiteral("拍摄UUID")},
        {"SelfData", QStringLiteral("自身数据")}
    };
    return names.value(tag, tag);
}

void DocumentInfo::loadExifTags() {
    if(exifLoaded)
        return;
    exifLoaded = true;
    exifTags.clear();
    xmpTags.clear();
#ifdef USE_EXIV2
    try {
        std::unique_ptr<Exiv2::Image> image;

        image = Exiv2::ImageFactory::open(toStdString(fileInfo.filePath()));
        if(!image) {
            qDebug() << "Could not open file for metadata reading:" << fileInfo.filePath();
            return;
        }
        image->readMetadata();
        Exiv2::ExifData &exifData = image->exifData();

        // Show every EXIF tag exactly as exiv2 reads it (raw value, localized
        // display name) - no hand-written formatting of our own. Two exceptions:
        // 1) binary blobs (XML XMP packet, raw maker note bytes) would render as
        //    a wall of numbers, so they are dropped;
        // 2) over-long values are truncated defensively.
        for(const auto &datum : exifData) {
            QString name = QString::fromStdString(std::string(datum.groupName()))
                           + "." + QString::fromStdString(std::string(datum.tagName()));
            if(name == "Image.XMLPacket" || name == "Photo.MakerNote")
                continue;
            QString value = QString::fromStdString(datum.value().toString());
            if(value.isEmpty())
                continue;
            if(value.size() > 400)
                value = value.left(400) + "…";
            exifTags.insert(localizedTagName(name), formatExifValue(name, value));
        }

        // XMP: DJI drone flight data lives in the drone-dji namespace
        // (requires exiv2 to be built with XMP support).
        Exiv2::XmpData &xmpData = image->xmpData();
        for(const auto &datum : xmpData) {
            QString key = QString::fromStdString(datum.key());
            if(!key.startsWith("Xmp.drone-dji."))
                continue;
            QString value = QString::fromStdString(datum.value().toString());
            if(value.isEmpty())
                continue;
            // datum.tagName() is unreliable for XMP keys, take the last segment
            QString tag = key.mid(key.lastIndexOf('.') + 1);
            xmpTags.insert(localizedXmpTagName(tag), value);
        }
    }

// this should work with both 0.28 and <0.28
#if not EXIV2_TEST_VERSION(0, 28, 0)
#ifdef __WIN32
    catch (Exiv2::BasicError<wchar_t>& e) {
        qDebug() << "Caught Exiv2::BasicError exception:\n" << e.what() << "\n";
        return;
    }
#else
    catch (Exiv2::BasicError<char>& e) {
        qDebug() << "Caught Exiv2::BasicError exception:\n" << e.what() << "\n";
        return;
    }
#endif
#endif

    catch (Exiv2::Error& e) {
        qDebug() << "Caught Exiv2 exception:\n" << e.what() << "\n";
        return;
    }
    catch(...) {
        qDebug() << "Caught unexpected exception while reading metadata";
        return;
    }
#endif
}

QMap<QString, QString> DocumentInfo::getExifTags() {
    if(!exifLoaded)
        loadExifTags();
    return exifTags;
}

QMap<QString, QString> DocumentInfo::getXmpTags() {
    if(!exifLoaded)
        loadExifTags();
    return xmpTags;
}

void DocumentInfo::loadExifOrientation() {
    if(mDocumentType == DocumentType::VIDEO || mDocumentType == DocumentType::NONE)
        return;

    QString path = filePath();
    QImageReader *reader = nullptr;
    if(!mFormat.isEmpty())
        reader = new QImageReader(path, mFormat.toStdString().c_str());
    else
        reader = new QImageReader(path);

    if(reader->canRead())
        mOrientation = static_cast<int>(reader->transformation());
    delete reader;
}
