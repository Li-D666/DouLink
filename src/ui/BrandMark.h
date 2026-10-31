#pragma once

#include <QWidget>

// Small painted brand mark (no external image assets required).
class BrandMark : public QWidget
{
public:
    explicit BrandMark(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
};
