#pragma once

#include "core/Types.h"

#include <QPlainTextEdit>
#include <QVector>

class QTextCursor;

namespace doulink {
namespace ui {

// Rich, bounded, per-session serial console. Keeps the full recent history for
// filtering/export, but only renders a recent window so switching sessions or
// toggling text/HEX/timestamps stays responsive.
class LogView : public QPlainTextEdit
{
    Q_OBJECT

public:
    enum DirectionFilter {
        AllDirections = 0,
        RxOnly = 1,
        TxOnly = 2,
        SystemOnly = 3
    };

    explicit LogView(QWidget* parent = nullptr);

    void setEntries(const QVector<LogEntry>& entries);
    void appendEntry(const LogEntry& entry);
    void clearEntries();

    void setDataFormat(DataFormat format);
    DataFormat dataFormat() const { return m_format; }

    void setShowTimestamp(bool show);
    bool showTimestamp() const { return m_showTimestamp; }

    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

    void setFilter(const QString& text, int directionFilter);
    QString exportText() const;

    // Rendered into the document (bounded by kMaxRenderedEntries).
    int visibleCount() const { return m_visibleCount; }
    // Retained history size (used by exportText()).
    int totalCount() const { return m_entries.size(); }

signals:
    void countChanged(int visible, int total);

private:
    bool matches(const LogEntry& entry) const;
    QString entryAsPlainText(const LogEntry& entry) const;
    void renderAll();
    void appendRendered(const LogEntry& entry, bool followTail);
    void appendRenderedTo(QTextCursor& cursor, const LogEntry& entry);
    void scrollToEnd();
    void updatePlaceholder();
    bool atBottom() const;

    static QString directionTag(Direction direction);

    // Full history retained by the view (and used by exportText()).
    static constexpr int kMaxEntries = 5000;
    // Only this many matching entries are rendered into the document.
    static constexpr int kMaxRenderedEntries = 1000;

    QVector<LogEntry> m_entries;
    DataFormat m_format = DataFormat::Text;
    bool m_showTimestamp = true;
    bool m_paused = false;
    QString m_filter;
    int m_directionFilter = AllDirections;
    int m_visibleCount = 0;
};

} // namespace ui
} // namespace doulink
