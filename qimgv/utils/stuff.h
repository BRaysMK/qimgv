#pragma once

#include <QString>
#include <string>

#ifdef _WIN32
    #define StdString std::wstring
    #define CharType wchar_t
#else
    #define StdString std::string
    #define CharType char
#endif

int clamp(int x, int lower, int upper);
int probeOS();
StdString toStdString(QString str);
QString fromStdString(StdString str);

// Decode raw metadata bytes (exiv2 hands back EXIF "ASCII"/Undefined/XMP values
// as bytes that are NOT guaranteed to be UTF-8). Tries UTF-8 first and falls
// back to the local 8-bit code page (e.g. GBK) when the input is not valid
// UTF-8; also strips BOM/control characters. Shared by the EXIF info panel and
// the metadata editor so both always display (and round-trip) the same text.
QString decodeMetadataBytes(const std::string &raw);
