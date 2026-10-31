#include "ui/SessionListDelegate.h"

#include "core/ByteCodec.h"
#include "core/Types.h"

#include <QAbstractItemModel>
#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>

namespace doulink {
namespace ui {
namespace {

QRectF checkboxRect(const QRect& card)
{
    return QRectF(card.left() + 12, card.top() + 10, 16, 16);
}

QColor stateColor(SessionState state, const ThemePalette& palette)
{
    switch (state) {
    case SessionState::Connected:
    case SessionState::Listening:
        return palette.stateOnline;
    case SessionState::Connecting:
        return palette.stateConnecting;
    case SessionState::Error:
        return palette.stateError;
    case SessionState::Disconnected:
        return palette.stateOffline;
    }
    return palette.stateOffline;
}

} // namespace

SessionListDelegate::SessionListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void SessionListDelegate::setTheme(Theme theme)
{
    m_theme = theme;
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

    const ThemePalette& palette = themePalette(m_theme);
    const QRect card = option.rect.adjusted(3, 2, -3, -2);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    const QColor background = selected
        ? palette.cardSelectedBg
        : (hovered ? palette.cardHoverBg : palette.cardBg);
    const QColor border = selected ? palette.cardSelectedBorder : palette.cardBorder;

    painter->setPen(QPen(border, 1.0));
    painter->setBrush(background);
    painter->drawRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(palette.accent);
        painter->drawRoundedRect(
            QRectF(card.left() + 1, card.top() + 11, 3, card.height() - 22), 1.5, 1.5);
    }

    const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
    const QRectF checkBox = checkboxRect(card);
    painter->setPen(QPen(checked ? palette.checkboxCheckedBorder
                                 : palette.checkboxBorder,
                         1.2));
    painter->setBrush(checked ? palette.checkboxCheckedBg : palette.checkboxBg);
    painter->drawRoundedRect(checkBox, 4, 4);
    if (checked) {
        QPen checkPen(palette.checkMark, 2.0);
        checkPen.setCapStyle(Qt::RoundCap);
        checkPen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(checkPen);
        painter->drawLine(QPointF(checkBox.left() + 4, checkBox.center().y()),
                          QPointF(checkBox.center().x() - 1, checkBox.bottom() - 4));
        painter->drawLine(QPointF(checkBox.center().x() - 1, checkBox.bottom() - 4),
                          QPointF(checkBox.right() - 4, checkBox.top() + 4));
    }

    const auto state = static_cast<SessionState>(index.data(StateRole).toInt());
    const QColor dotColor = stateColor(state, palette);
    const QPointF dotCenter(card.left() + 38, card.top() + 18);

    QColor glow = dotColor;
    glow.setAlpha(45);
    painter->setPen(Qt::NoPen);
    painter->setBrush(glow);
    painter->drawEllipse(dotCenter, 5.5, 5.5);
    painter->setBrush(dotColor);
    painter->drawEllipse(dotCenter, 3.2, 3.2);

    QFont nameFont = option.font;
    nameFont.setPointSizeF(10.0);
    nameFont.setWeight(QFont::DemiBold);
    painter->setFont(nameFont);
    painter->setPen(palette.sessionName);

    const QString name = index.data(NameRole).toString();
    const QFontMetrics nameMetrics(nameFont);
    const QRect nameRect(card.left() + 50, card.top() + 6, card.width() - 50 - 68, 23);
    painter->drawText(nameRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(name, Qt::ElideRight, nameRect.width()));

    QFont smallFont = option.font;
    smallFont.setPointSizeF(8.5);
    painter->setFont(smallFont);
    painter->setPen(dotColor);
    const QString stateText = index.data(StateTextRole).toString();
    const QRect stateRect(card.right() - 66, card.top() + 7, 58, 20);
    painter->drawText(stateRect, Qt::AlignRight | Qt::AlignVCenter, stateText);

    const QFontMetrics smallMetrics(smallFont);
    painter->setPen(palette.sessionEndpoint);
    const QString endpoint = index.data(EndpointRole).toString();
    const QRect endpointRect(card.left() + 50, card.top() + 28, card.width() - 62, 18);
    painter->drawText(endpointRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      smallMetrics.elidedText(endpoint, Qt::ElideRight, endpointRect.width()));

    const quint64 rxBytes = index.data(RxBytesRole).toULongLong();
    const quint64 txBytes = index.data(TxBytesRole).toULongLong();
    const QRect metricRect(card.left() + 50, card.top() + 49, card.width() - 62, 17);

    const QString rxText = QStringLiteral("RX  %1").arg(Bytes::formatSize(rxBytes));
    const QString txText = QStringLiteral("TX  %1").arg(Bytes::formatSize(txBytes));

    painter->setPen(palette.metricRx);
    painter->drawText(metricRect, Qt::AlignLeft | Qt::AlignVCenter, rxText);

    const int rxWidth = smallMetrics.horizontalAdvance(rxText) + 14;
    painter->setPen(palette.metricTx);
    painter->drawText(metricRect.adjusted(rxWidth, 0, 0, 0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      txText);

    painter->restore();
}

bool SessionListDelegate::editorEvent(QEvent* event,
                                      QAbstractItemModel* model,
                                      const QStyleOptionViewItem& option,
                                      const QModelIndex& index)
{
    if (!(index.flags() & Qt::ItemIsUserCheckable))
        return false;

    const auto toggle = [&]() -> bool {
        const auto state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
        const Qt::CheckState next = state == Qt::Checked ? Qt::Unchecked : Qt::Checked;
        return model->setData(index, next, Qt::CheckStateRole);
    };

    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            const QRect card = option.rect.adjusted(3, 2, -3, -2);
            if (checkboxRect(card).contains(mouse->pos()))
                return toggle();
        }
    } else if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Space || key->key() == Qt::Key_Select)
            return toggle();
    }

    return false;
}

} // namespace ui
} // namespace doulink
