#include "stuff.h"

#include <QByteArray>

int clamp(int x, int lower, int upper) {
    return qMin(upper, qMax(x, lower));
}

// 0 - mac, 1 - linux, 2 - windows, 3 - other
int probeOS() {
#ifdef TARGET_OS_MAC
    return 0;
#elif defined(__linux__) || defined(__FreeBSD__)
    return 1;
#elif defined _WIN32 || defined _WIN64
    return 2;
#else
    return 3;
#endif
}

StdString toStdString(QString str) {
#ifdef _WIN32
    return str.toStdWString();
#else
    return str.toStdString();
#endif
}

QString fromStdString(StdString str) {
#ifdef _WIN32
    return QString::fromStdWString(str);
#else
    return QString::fromStdString(str);
#endif
}

// exiv2 hands metadata back as raw bytes. EXIF "ASCII"/"Undefined" fields are
// NOT guaranteed to be UTF-8 - they may hold the local code page (GBK on a
// Chinese system), or plain binary. Decoding those as UTF-8 unconditionally
// produces mojibake, so decode as UTF-8 only when the bytes are actually valid
// UTF-8 and fall back to the local 8-bit code page otherwise.
QString decodeMetadataBytes(const std::string &raw) {
    if(raw.empty())
        return QString();
    QByteArray bytes(raw.data(), static_cast<int>(raw.size()));
    // strip a UTF-8 BOM if present (explicit length: "\xEF\xBB\xBF" is a
    // 3-char array literal, and the hex escape could otherwise swallow a
    // following hex digit)
    static const char bom[3] = {'\xEF', '\xBB', '\xBF'};
    if(bytes.startsWith(QByteArray::fromRawData(bom, 3)))
        bytes.remove(0, 3);
    QString s = QString::fromUtf8(bytes);
    // fromUtf8() substitutes U+FFFD for invalid sequences; if that happened the
    // input was not UTF-8, so reinterpret it using the system code page.
    if(s.contains(QChar(0xFFFD)))
        s = QString::fromLocal8Bit(bytes);
    // drop control characters (except tab/newline) that binary blobs would leave
    QString out;
    out.reserve(s.size());
    for(const QChar &c : s) {
        if(c.unicode() == 0xFFFD)
            continue;
        if(c.unicode() < 0x20 && c != '\t' && c != '\n' && c != '\r')
            continue;
        out.append(c);
    }
    return out;
}
