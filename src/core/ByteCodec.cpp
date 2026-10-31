#include "core/ByteCodec.h"

#include <QChar>
#include <QStringList>

namespace doulink {
namespace Bytes {
namespace {

bool isHexDigit(QChar ch)
{
    const ushort c = ch.unicode();
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int nibble(QChar ch)
{
    const ushort c = ch.unicode();
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}

bool isSeparator(QChar ch)
{
    return ch.isSpace() || ch == QLatin1Char(',') || ch == QLatin1Char(';')
        || ch == QLatin1Char(':') || ch == QLatin1Char('-') || ch == QLatin1Char('_');
}

QString hexEscape(ushort value, int width)
{
    return QStringLiteral("\\%1%2")
        .arg(width == 2 ? QStringLiteral("x") : QStringLiteral("u"),
             QString::number(value, 16).toUpper().rightJustified(width, QLatin1Char('0')));
}

} // namespace

bool fromHex(const QString& text, QByteArray* out, QString* error)
{
    if (!out)
        return false;
    out->clear();
    if (error)
        error->clear();

    int i = 0;
    const int size = text.size();
    while (i < size) {
        const QChar ch = text.at(i);

        if (isSeparator(ch)) {
            ++i;
            continue;
        }

        // Tolerate 0x / 0X prefixes.
        if (ch == QLatin1Char('0') && i + 1 < size
            && (text.at(i + 1) == QLatin1Char('x') || text.at(i + 1) == QLatin1Char('X'))) {
            i += 2;
            continue;
        }

        if (!isHexDigit(ch)) {
            if (error) {
                *error = QStringLiteral("第 %1 个字符“%2”不是合法的十六进制字符")
                             .arg(i + 1)
                             .arg(ch);
            }
            return false;
        }

        const int start = i;
        while (i < size && isHexDigit(text.at(i)))
            ++i;

        const int count = i - start;
        if (count % 2 != 0) {
            if (error) {
                *error = QStringLiteral("从第 %1 个字符开始的十六进制位数是奇数（%2 位），请补足两位")
                             .arg(start + 1)
                             .arg(count);
            }
            out->clear();
            return false;
        }

        out->reserve(out->size() + count / 2);
        for (int p = start; p < i; p += 2) {
            const int value = nibble(text.at(p)) * 16 + nibble(text.at(p + 1));
            out->append(static_cast<char>(value));
        }
    }

    if (out->isEmpty() && !text.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("没有解析到有效的十六进制数据");
        return false;
    }
    return true;
}

QString toHex(const QByteArray& data, char separator)
{
    if (data.isEmpty())
        return {};
    return QString::fromLatin1(data.toHex(separator).toUpper());
}

QString toDisplayText(const QByteArray& data)
{
    const QString decoded = QString::fromUtf8(data);
    QString out;
    out.reserve(decoded.size() + 8);

    for (const QChar ch : decoded) {
        const ushort code = ch.unicode();
        switch (code) {
        case '\\':
            out += QStringLiteral("\\\\");
            break;
        case '\r':
            out += QStringLiteral("\\r");
            break;
        case '\n':
            out += QStringLiteral("\\n");
            break;
        case '\t':
            out += QStringLiteral("\\t");
            break;
        default:
            if (code < 0x20 || code == 0x7F) {
                out += hexEscape(code, 2);
            } else if (!ch.isPrint()) {
                out += hexEscape(code, 4);
            } else {
                out += ch;
            }
            break;
        }
    }
    return out;
}

QString toDisplay(const QByteArray& data, DataFormat format)
{
    return format == DataFormat::Hex ? toHex(data) : toDisplayText(data);
}

QString formatSize(quint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);

    const double kb = static_cast<double>(bytes) / 1024.0;
    if (kb < 1024.0)
        return QStringLiteral("%1 KB").arg(QString::number(kb, 'f', kb < 10.0 ? 2 : 1));

    const double mb = kb / 1024.0;
    if (mb < 1024.0)
        return QStringLiteral("%1 MB").arg(QString::number(mb, 'f', mb < 10.0 ? 2 : 1));

    const double gb = mb / 1024.0;
    return QStringLiteral("%1 GB").arg(QString::number(gb, 'f', 2));
}

} // namespace Bytes
} // namespace doulink
