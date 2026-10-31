#include "ui/LogView.h"

#include "core/ByteCodec.h"

#include <QColor>
#include <QDateTime>
#include <QFontMetricsF>
#include <QScrollBar>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>

namespace doulink {
namespace ui {
namespace {

QTextCharFormat coloredText(const QColor& color, bool italic = false)
{
    QTextCharFormat format;
    format.setForeground(color);
    if (italic)
        format.setFontItalic(true);
    return format;
}

} // namespace

LogView::LogView(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("LogView"));
    setReadOnly(true);
    setUndoRedoEnabled(false);
    setFrameShape(QFrame::NoFrame);
    setMaximumBlockCount(kMaxEntries);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    QFont console(QStringLiteral("Consolas"));
    console.setStyleHint(QFont::Monospace);
    console.setPointSize(10);
    setFont(console);
    document()->setDefaultFont(console);
    setTabStopDistance(QFontMetricsF(console).horizontalAdvance(QLatin1Char(' ')) * 4.0);

    setPlaceholderText(QStringLiteral("暂无报文 — 建立连接后，收发的数据会显示在这里"));
}

void LogView::setEntries(const QVector<LogEntry>& entries)
{
    m_entries = entries;
    const int overflow = m_entries.size() - kMaxEntries;
    if (overflow > 0)
        m_entries.remove(0, overflow);
    renderAll();
}

void LogView::appendEntry(const LogEntry& entry)
{
    if (!m_paused) {
        const bool followTail = atBottom();
        m_entries.append(entry);

        int overflow = m_entries.size() - kMaxEntries;
        if (overflow > 0) {
            for (int i = 0; i < overflow; ++i) {
                if (matches(m_entries.at(i)))
                    --m_visibleCount;
            }
            m_entries.remove(0, overflow);
        }

        if (matches(entry)) {
            ++m_visibleCount;
            appendRendered(entry, followTail);
        }
        updatePlaceholder();
        emit countChanged(m_visibleCount, m_entries.size());
        return;
    }

    // Paused: keep buffering so resuming does not lose anything.
    m_entries.append(entry);
    const int overflow = m_entries.size() - kMaxEntries;
    if (overflow > 0)
        m_entries.remove(0, overflow);
    emit countChanged(m_visibleCount, m_entries.size());
}

void LogView::clearEntries()
{
    m_entries.clear();
    m_visibleCount = 0;
    document()->clear();
    updatePlaceholder();
    emit countChanged(0, 0);
}

void LogView::setDataFormat(DataFormat format)
{
    if (m_format == format)
        return;
    m_format = format;
    renderAll();
}

void LogView::setShowTimestamp(bool show)
{
    if (m_showTimestamp == show)
        return;
    m_showTimestamp = show;
    renderAll();
}

void LogView::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    if (!paused)
        renderAll();
}

void LogView::setFilter(const QString& text, int directionFilter)
{
    const QString trimmed = text.trimmed();
    if (m_filter == trimmed && m_directionFilter == directionFilter)
        return;
    m_filter = trimmed;
    m_directionFilter = directionFilter;
    renderAll();
}

QString LogView::exportText() const
{
    QString out;
    out.reserve(m_visibleCount * 64);
    for (const LogEntry& entry : m_entries) {
        if (!matches(entry))
            continue;
        out += entryAsPlainText(entry);
        out += QLatin1Char('\n');
    }
    return out;
}

bool LogView::matches(const LogEntry& entry) const
{
    switch (m_directionFilter) {
    case RxOnly:
        if (entry.direction != Direction::Rx)
            return false;
        break;
    case TxOnly:
        if (entry.direction != Direction::Tx)
            return false;
        break;
    case SystemOnly:
        if (entry.direction != Direction::System && entry.direction != Direction::Error)
            return false;
        break;
    default:
        break;
    }

    if (m_filter.isEmpty())
        return true;

    return entryAsPlainText(entry).contains(m_filter, Qt::CaseInsensitive);
}

QString LogView::entryAsPlainText(const LogEntry& entry) const
{
    QString line;
    if (m_showTimestamp) {
        line += QLatin1Char('[');
        line += entry.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
        line += QStringLiteral("] ");
    }

    line += directionTag(entry.direction);
    line += QLatin1Char(' ');

    if (!entry.peer.isEmpty())
        line += QStringLiteral("[%1] ").arg(entry.peer);

    if (entry.direction == Direction::System || entry.direction == Direction::Error)
        line += entry.message;
    else
        line += Bytes::toDisplay(entry.payload, m_format);

    return line;
}

void LogView::renderAll()
{
    setUpdatesEnabled(false);
    document()->clear();

    m_visibleCount = 0;
    for (const LogEntry& entry : qAsConst(m_entries)) {
        if (!matches(entry))
            continue;
        appendRendered(entry, false);
        ++m_visibleCount;
    }

    setUpdatesEnabled(true);
    scrollToEnd();
    updatePlaceholder();
    emit countChanged(m_visibleCount, m_entries.size());
}

void LogView::appendRendered(const LogEntry& entry, bool followTail)
{
    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::End);

    QTextBlockFormat blockFormat;
    blockFormat.setBottomMargin(2.0);
    blockFormat.setLineHeight(112.0, QTextBlockFormat::ProportionalHeight);
    if (document()->isEmpty())
        cursor.setBlockFormat(blockFormat);
    else
        cursor.insertBlock(blockFormat);

    const QTextCharFormat timestampFormat = coloredText(QColor(QStringLiteral("#5a7186")));
    if (m_showTimestamp) {
        cursor.insertText(QStringLiteral("[%1] ")
                              .arg(entry.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))),
                          timestampFormat);
    }

    QString tag;
    QColor tagColor;
    QColor tagBackground;
    switch (entry.direction) {
    case Direction::Rx:
        tag = QStringLiteral("RX");
        tagColor = QColor(QStringLiteral("#4ade80"));
        tagBackground = QColor(QStringLiteral("#10352a"));
        break;
    case Direction::Tx:
        tag = QStringLiteral("TX");
        tagColor = QColor(QStringLiteral("#38bdf8"));
        tagBackground = QColor(QStringLiteral("#0c2d40"));
        break;
    case Direction::System:
        tag = QStringLiteral("SYS");
        tagColor = QColor(QStringLiteral("#94a3b8"));
        tagBackground = QColor(QStringLiteral("#1a2632"));
        break;
    case Direction::Error:
        tag = QStringLiteral("ERR");
        tagColor = QColor(QStringLiteral("#f87171"));
        tagBackground = QColor(QStringLiteral("#3a171b"));
        break;
    }

    QTextCharFormat badgeFormat;
    badgeFormat.setForeground(tagColor);
    badgeFormat.setBackground(tagBackground);
    badgeFormat.setFontWeight(QFont::Bold);
    cursor.insertText(QStringLiteral(" %1 ").arg(tag), badgeFormat);
    cursor.insertText(QStringLiteral(" "), coloredText(QColor(QStringLiteral("#5a7186"))));

    if (!entry.peer.isEmpty()) {
        cursor.insertText(QStringLiteral("[%1] ").arg(entry.peer),
                          coloredText(QColor(QStringLiteral("#c4b5fd"))));
    }

    if (entry.direction == Direction::System) {
        cursor.insertText(entry.message,
                          coloredText(QColor(QStringLiteral("#8fa2b3")), true));
    } else if (entry.direction == Direction::Error) {
        cursor.insertText(entry.message,
                          coloredText(QColor(QStringLiteral("#fca5a5")), true));
    } else {
        const QColor payloadColor = entry.direction == Direction::Rx
            ? QColor(QStringLiteral("#c3f7d5"))
            : QColor(QStringLiteral("#c7e9ff"));
        cursor.insertText(Bytes::toDisplay(entry.payload, m_format),
                          coloredText(payloadColor));
    }

    if (followTail)
        scrollToEnd();
}

void LogView::scrollToEnd()
{
    QScrollBar* bar = verticalScrollBar();
    if (bar)
        bar->setValue(bar->maximum());
}

void LogView::updatePlaceholder()
{
    if (m_entries.isEmpty()) {
        setPlaceholderText(QStringLiteral("暂无报文 — 建立连接后，收发的数据会显示在这里"));
    } else if (m_visibleCount == 0) {
        setPlaceholderText(QStringLiteral("没有匹配的报文，请调整筛选条件"));
    } else {
        setPlaceholderText(QString());
    }
}

bool LogView::atBottom() const
{
    const QScrollBar* bar = verticalScrollBar();
    return !bar || bar->maximum() - bar->value() <= 4;
}

QString LogView::directionTag(Direction direction)
{
    switch (direction) {
    case Direction::Rx:
        return QStringLiteral("RX");
    case Direction::Tx:
        return QStringLiteral("TX");
    case Direction::System:
        return QStringLiteral("SYS");
    case Direction::Error:
        return QStringLiteral("ERR");
    }
    return QStringLiteral("???");
}

} // namespace ui
} // namespace doulink
