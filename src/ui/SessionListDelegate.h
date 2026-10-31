#pragma once

#include "ui/Theme.h"

#include <QStyledItemDelegate>

class QAbstractItemModel;
class QEvent;

namespace doulink {
namespace ui {

// Data roles stored on each session list item.
enum SessionItemRole {
    SessionIdRole = Qt::UserRole + 1,
    NameRole,
    EndpointRole,
    StateRole,
    StateTextRole,
    RxBytesRole,
    TxBytesRole,
    KindRole
};

class SessionListDelegate : public QStyledItemDelegate
{
public:
    explicit SessionListDelegate(QObject* parent = nullptr);

    void setTheme(Theme theme);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event,
                     QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

private:
    Theme m_theme = Theme::Dark;
};

} // namespace ui
} // namespace doulink
