#include "ui/BrandMark.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>

BrandMark::BrandMark(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(38, 38);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

QSize BrandMark::sizeHint() const
{
    return QSize(38, 38);
}

QSize BrandMark::minimumSizeHint() const
{
    return QSize(38, 38);
}

void BrandMark::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF background = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QLinearGradient gradient(background.topLeft(), background.bottomRight());
    gradient.setColorAt(0.0, QColor(QStringLiteral("#22d3ee")));
    gradient.setColorAt(1.0, QColor(QStringLiteral("#2563eb")));

    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(background, 10.5, 10.5);

    QPen linkPen(QColor(QStringLiteral("#07151f")), 2.4);
    linkPen.setCapStyle(Qt::RoundCap);
    linkPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(linkPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(9.5, 12.5, 11.0, 12.0), 4.5, 4.5);
    painter.drawRoundedRect(QRectF(17.5, 13.5, 11.0, 12.0), 4.5, 4.5);
    painter.drawLine(QPointF(16.5, 19.0), QPointF(21.5, 19.0));
}
