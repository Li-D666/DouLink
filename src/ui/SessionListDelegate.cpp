#include "ui/SessionListDelegate.h"

#include "core/ByteCodec.h"
#include "core/Types.h"

#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>

namespace doulink {
namespace ui {
namespace {

QColor stateColor(SessionState state)
{
    switch (state) {
    case SessionState::Connected:
    case SessionState::Listening:
        return QColor(QStringLiteral("#4ade80"));
    case SessionState::Connecting:
        return QColor(QStringLiteral("#fbbf24"));
    case SessionState::Error:
        return QColor(QStringLiteral("#f87171"));
    case SessionState::Disconnected:
        return QColor(QStringLiteral("#68798a"));
    }
    return QColor(QStringLiteral("#68798a"));
}

} // namespace

SessionListDelegate::SessionListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QSize SessionListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(260, 78);
}

void SessionListDelegate::paint(QPainter* painter,
                                const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRect card = option.rect.adjusted(3, 2, -3, -2);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    const QColor background = selected
        ? QColor(QStringLiteral("#162431"))
        : (hovered ? QColor(QStringLiteral("#111c26")) : QColor(QStringLiteral("#0e1720")));
    const QColor border = selected
        ? QColor(QStringLiteral("#2b5a70"))
        : QColor(QStringLiteral("#172531"));

    painter->setPen(QPen(border, 1.0));
    painter->setBrush(background);
    painter->drawRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(QStringLiteral("#22d3ee")));
        painter->drawRoundedRect(
            QRectF(card.left() + 1, card.top() + 11, 3, card.height() - 22), 1.5, 1.5);
    }

    const auto state = static_cast<SessionState>(index.data(StateRole).toInt());
    const QColor dotColor = stateColor(state);
    const QPointF dotCenter(card.left() + 18, card.top() + 18);

    QColor glow = dotColor;
    glow.setAlpha(45);
    painter->setPen(Qt::NoPen);
    painter->setBrush(glow);
    painter->drawEllipse(dotCenter, 6.5, 6.5);
    painter->setBrush(dotColor);
    painter->drawEllipse(dotCenter, 3.5, 3.5);

    QFont nameFont = option.font;
    nameFont.setPointSizeF(10.0);
    nameFont.setWeight(QFont::DemiBold);
    painter->setFont(nameFont);
    painter->setPen(QColor(QStringLiteral("#e8f1f8")));

    const QString name = index.data(NameRole).toString();
    const QFontMetrics nameMetrics(nameFont);
    const QRect nameRect(card.left() + 30, card.top() + 6, card.width() - 30 - 78, 23);
    painter->drawText(nameRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(name, Qt::ElideRight, nameRect.width()));

    QFont smallFont = option.font;
    smallFont.setPointSizeF(8.5);
    painter->setFont(smallFont);
    painter->setPen(dotColor);
    const QString stateText = index.data(StateTextRole).toString();
    const QRect stateRect(card.right() - 74, card.top() + 7, 64, 20);
    painter->drawText(stateRect, Qt::AlignRight | Qt::AlignVCenter, stateText);

    const QFontMetrics smallMetrics(smallFont);
    painter->setPen(QColor(QStringLiteral("#7c93a7")));
    const QString endpoint = index.data(EndpointRole).toString();
    const QRect endpointRect(card.left() + 30, card.top() + 28, card.width() - 42, 18);
    painter->drawText(endpointRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      smallMetrics.elidedText(endpoint, Qt::ElideRight, endpointRect.width()));

    const quint64 rxBytes = index.data(RxBytesRole).toULongLong();
    const quint64 txBytes = index.data(TxBytesRole).toULongLong();
    const QRect metricRect(card.left() + 30, card.top() + 49, card.width() - 42, 17);

    const QString rxText = QStringLiteral("RX  %1").arg(Bytes::formatSize(rxBytes));
    const QString txText = QStringLiteral("TX  %1").arg(Bytes::formatSize(txBytes));

    painter->setPen(QColor(QStringLiteral("#4ade80")));
    painter->drawText(metricRect, Qt::AlignLeft | Qt::AlignVCenter, rxText);

    const int rxWidth = smallMetrics.horizontalAdvance(rxText) + 14;
    painter->setPen(QColor(QStringLiteral("#38bdf8")));
    painter->drawText(metricRect.adjusted(rxWidth, 0, 0, 0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      txText);

    painter->restore();
}

} // namespace ui
} // namespace doulink
