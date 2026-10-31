#pragma once

#include <QStyledItemDelegate>

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
    TxBytesRole
};

class SessionListDelegate : public QStyledItemDelegate
{
public:
    explicit SessionListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace ui
} // namespace doulink
