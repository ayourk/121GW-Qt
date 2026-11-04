// SPDX-License-Identifier: MIT
// Ported from Readout (Readout/Drawing/LcdDrawable.cs, SevenSegment.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#pragma once

#include "metercontroller.h"
#include "reading.h"

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <optional>

namespace readout {



// The 121GW's LCD: main and sub digits, units, the bar graph and the symbols around them, drawn
// on a 420×220 virtual canvas scaled to the item. Unlit segments are drawn faintly, like a real LCD.
class LcdItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(readout::MeterController *controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(QColor glass MEMBER m_glass NOTIFY colorsChanged)
    Q_PROPERTY(QColor ink MEMBER m_ink NOTIFY colorsChanged)
    Q_PROPERTY(QColor bezel MEMBER m_bezel NOTIFY colorsChanged)
    Q_PROPERTY(qreal ghostOpacity MEMBER m_ghostOpacity NOTIFY colorsChanged)
    Q_PROPERTY(bool backlight MEMBER m_backlight NOTIFY colorsChanged)
public:
    static constexpr qreal CanvasWidth = 420, CanvasHeight = 220;

    explicit LcdItem(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

    MeterController *controller() const { return m_controller; }
    void setController(MeterController *controller);

signals:
    void controllerChanged();
    void colorsChanged();

private:
    QPointer<MeterController> m_controller;
    QColor m_glass{0x6B, 0x72, 0x62};
    QColor m_ink{0x2E, 0x30, 0x2E};
    QColor m_bezel{0x49, 0x50, 0x4F};
    qreal m_ghostOpacity = 0.08;
    bool m_backlight = false;
};

} // namespace readout
