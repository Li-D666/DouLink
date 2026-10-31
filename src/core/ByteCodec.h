#pragma once

#include "core/Types.h"

#include <QByteArray>
#include <QString>

namespace doulink {
namespace Bytes {

// Parses a human friendly hex string such as:
//   "01 03 00 00 00 0A"
//   "0x01,0x03,0x00"
//   "010300000a"
// Separators are whitespace, ',', ';', ':', '-' and '_'.
bool fromHex(const QString& text, QByteArray* out, QString* error);

// Uppercase, space separated pairs: "01 03 0A".
QString toHex(const QByteArray& data, char separator = ' ');

// Escapes control / non printable characters so one frame stays on one line.
QString toDisplayText(const QByteArray& data);

QString toDisplay(const QByteArray& data, DataFormat format);

// "820 B", "1.2 KB", "3.4 MB".
QString formatSize(quint64 bytes);

} // namespace Bytes
} // namespace doulink
