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
    setMaximumBlockCount(kMaxRenderedEntries);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    QFont console(QStringLiteral("Consolas"));
    console.setStyleHint(QFont::Monospace);
    console.setPointSize(10);
    setFont(console);
    document()->setDefaultFont(console);
    setTabStopDistance(QFontMetricsF(console).horizontalAdvance(QLatin1Char(' ')) * 4.0);

    setPlaceholderText(QStringLiteral("暂无报文 — 建立连接后，收发的数据会显示在这里"));
    setToolTip(QStringLiteral("为保证流畅，界面最多显示最近 %1 条报文；保存日志会导出本会话全部历史。")
                   .arg(kMaxRenderedEntries));
}

void LogView::setTheme(Theme theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    renderAll();
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

        const int overflow = m_entries.size() - kMaxEntries;
        if (overflow > 0)
            m_entries.remove(0, overflow);

        if (matches(entry)) {
            appendRendered(entry, followTail);
            m_visibleCount = qMin(m_visibleCount + 1, kMaxRenderedEntries);
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

    // Find the start of the most recent kMaxRenderedEntries matching entries.
    int start = m_entries.size();
    int found = 0;
    while (start > 0 && found < kMaxRenderedEntries) {
        --start;
        if (matches(m_entries.at(start)))
            ++found;
    }

    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();
    for (int i = start; i < m_entries.size(); ++i) {
        const LogEntry& entry = m_entries.at(i);
        if (!matches(entry))
            continue;
        appendRenderedTo(cursor, entry);
        ++m_visibleCount;
    }
    cursor.endEditBlock();

    setUpdatesEnabled(true);
    scrollToEnd();
    updatePlaceholder();
    emit countChanged(m_visibleCount, m_entries.size());
}

void LogView::appendRendered(const LogEntry& entry, bool followTail)
{
    QTextCursor cursor(document());
    cursor.movePosition(QTextCursor::End);
    appendRenderedTo(cursor, entry);
    if (followTail)
        scrollToEnd();
}

void LogView::appendRenderedTo(QTextCursor& cursor, const LogEntry& entry)
{
    const ThemePalette& palette = themePalette(m_theme);

    QTextBlockFormat blockFormat;
    blockFormat.setBottomMargin(2.0);
    blockFormat.setLineHeight(112.0, QTextBlockFormat::ProportionalHeight);
    if (document()->isEmpty())
        cursor.setBlockFormat(blockFormat);
    else
        cursor.insertBlock(blockFormat);

    const QTextCharFormat timestampFormat = coloredText(palette.logTimestamp);
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
        tagColor = palette.logRxTag;
        tagBackground = palette.logRxTagBg;
        break;
    case Direction::Tx:
        tag = QStringLiteral("TX");
        tagColor = palette.logTxTag;
        tagBackground = palette.logTxTagBg;
        break;
    case Direction::System:
        tag = QStringLiteral("SYS");
        tagColor = palette.logSysTag;
        tagBackground = palette.logSysTagBg;
        break;
    case Direction::Error:
        tag = QStringLiteral("ERR");
        tagColor = palette.logErrTag;
        tagBackground = palette.logErrTagBg;
        break;
    }

    QTextCharFormat badgeFormat;
    badgeFormat.setForeground(tagColor);
    badgeFormat.setBackground(tagBackground);
    badgeFormat.setFontWeight(QFont::Bold);
    cursor.insertText(QStringLiteral(" %1 ").arg(tag), badgeFormat);
    cursor.insertText(QStringLiteral(" "), coloredText(palette.logTimestamp));

    if (!entry.peer.isEmpty()) {
        cursor.insertText(QStringLiteral("[%1] ").arg(entry.peer),
                          coloredText(palette.logPeer));
    }

    if (entry.direction == Direction::System) {
        cursor.insertText(entry.message,
                          coloredText(palette.logSysText, true));
    } else if (entry.direction == Direction::Error) {
        cursor.insertText(entry.message,
                          coloredText(palette.logErrText, true));
    } else {
        const QColor payloadColor = entry.direction == Direction::Rx
            ? palette.logRxText
            : palette.logTxText;
        cursor.insertText(Bytes::toDisplay(entry.payload, m_format),
                          coloredText(payloadColor));
    }
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
