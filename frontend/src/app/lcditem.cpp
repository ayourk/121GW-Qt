// SPDX-License-Identifier: MIT
// Ported from Readout (Readout/Drawing/LcdDrawable.cs, SevenSegment.cs), Copyright (c) 2026 Alejandro Mora, MIT.
#include "lcditem.h"

#include "metercontroller.h"
#include "metersession.h"

#include <QPainter>
#include <QPainterPath>

namespace readout {

namespace {

// Seven-segment glyphs. Bits: a=1 (top), b=2 (top right), c=4 (bottom right), d=8 (bottom),
// e=16 (bottom left), f=32 (top left), g=64 (middle).
int glyphBits(QChar c)
{
    switch (c.unicode()) {
    case '0': return 0x3F; case '1': return 0x06; case '2': return 0x5B; case '3': return 0x4F;
    case '4': return 0x66; case '5': return 0x6D; case '6': return 0x7D; case '7': return 0x07;
    case '8': return 0x7F; case '9': return 0x6F; case '-': return 0x40; case '_': return 0x08;
    case 'A': return 0x77; case 'b': return 0x7C; case 'C': return 0x39; case 'c': return 0x58;
    case 'd': return 0x5E; case 'E': return 0x79; case 'F': return 0x71; case 'H': return 0x76;
    case 'h': return 0x74; case 'L': return 0x38; case 'n': return 0x54; case 'O': return 0x3F;
    case 'o': return 0x5C; case 'P': return 0x73; case 'r': return 0x50; case 't': return 0x78;
    case 'U': return 0x3E; case 'u': return 0x1C; case 'y': return 0x6E;
    default: return 0;
    }
}

constexpr qreal Slant = 0.08;

struct Painter {
    QPainter *p;
    QColor ink, ghost, glass;

    QColor lit(bool on) const { return on ? ink : ghost; }

    QFont font(qreal size) const
    {
        QFont f(QStringLiteral("DejaVu Sans"));
        f.setBold(true);
        f.setPixelSize(qMax(1, qRound(size)));
        return f;
    }

    void label(const QString &text, qreal x, qreal y, qreal size, bool on, Qt::Alignment align = Qt::AlignLeft,
               qreal width = 60)
    {
        p->setPen(lit(on));
        p->setFont(font(size));
        p->drawText(QRectF(x, y, width, size * 1.3), align | Qt::AlignVCenter, text);
    }

    // A symbol printed light-on-dark in a box, as AUTO, MEM, A- and HOLD are on the meter.
    void boxed(const QString &text, qreal x, qreal y, qreal width, bool on)
    {
        const qreal height = 13;
        p->setPen(Qt::NoPen);
        p->setBrush(lit(on));
        p->drawRoundedRect(QRectF(x, y, width, height), 4, 4);
        p->setPen(on ? glass : ghost);
        p->setFont(font(10));
        p->drawText(QRectF(x, y, width, height), Qt::AlignCenter, text);
    }

    QPointF skew(qreal px, qreal py, qreal top, qreal height) const
    {
        return {px + (top + height - py) * Slant, py};
    }

    void digit(QChar glyph, bool point, const QRectF &cell)
    {
        const int bits = glyphBits(glyph);
        const qreal w = cell.width() * 0.68, h = cell.height();
        const qreal x = cell.x() + cell.width() * 0.08, y = cell.y();
        const qreal t = qMax(1.5, w * 0.17);   // segment thickness
        const qreal gap = t * 0.18, mid = y + h / 2, half = t / 2;
        p->setPen(Qt::NoPen);

        auto segment = [&](int bit, qreal x1, qreal y1, qreal x2, qreal y2, bool horizontal) {
            QPainterPath path;
            if (horizontal) {
                path.moveTo(skew(x1 + gap, y1, y, h));
                path.lineTo(skew(x1 + gap + half, y1 - half, y, h));
                path.lineTo(skew(x2 - gap - half, y2 - half, y, h));
                path.lineTo(skew(x2 - gap, y2, y, h));
                path.lineTo(skew(x2 - gap - half, y2 + half, y, h));
                path.lineTo(skew(x1 + gap + half, y1 + half, y, h));
            } else {
                path.moveTo(skew(x1, y1 + gap, y, h));
                path.lineTo(skew(x1 + half, y1 + gap + half, y, h));
                path.lineTo(skew(x2 + half, y2 - gap - half, y, h));
                path.lineTo(skew(x2, y2 - gap, y, h));
                path.lineTo(skew(x2 - half, y2 - gap - half, y, h));
                path.lineTo(skew(x1 - half, y1 + gap + half, y, h));
            }
            path.closeSubpath();
            p->fillPath(path, lit(bits & bit));
        };

        const qreal left = x + half, right = x + w - half, top = y + half, bottom = y + h - half;
        segment(0x01, left, top, right, top, true);        // a
        segment(0x02, right, top, right, mid, false);      // b
        segment(0x04, right, mid, right, bottom, false);   // c
        segment(0x08, left, bottom, right, bottom, true);  // d
        segment(0x10, left, mid, left, bottom, false);     // e
        segment(0x20, left, top, left, mid, false);        // f
        segment(0x40, left, mid, right, mid, true);        // g

        const qreal dot = t * 1.1;
        const QPointF dp = skew(x + w + cell.width() * 0.07, bottom, y, h);
        p->setBrush(lit(point));
        p->drawEllipse(QRectF(dp.x() - dot / 2, dp.y() - dot / 2, dot, dot));
    }

    // Right-aligned into count digit cells; a point never takes a cell of its own.
    void text(const QString &s, const QRectF &area, int count)
    {
        QList<QPair<QChar, bool>> cells;
        for (QChar ch : s) {
            if (ch == u'.' && !cells.isEmpty())
                cells.last().second = true;
            else if (ch == u'.')
                cells.append({u' ', true});
            else
                cells.append({ch, false});
        }
        while (cells.size() < count)
            cells.prepend({u' ', false});
        if (cells.size() > count)
            cells = cells.mid(cells.size() - count);
        const qreal cw = area.width() / count;
        for (int i = 0; i < count; ++i)
            digit(cells[i].first, cells[i].second, QRectF(area.x() + i * cw, area.y(), cw, area.height()));
    }
};

} // namespace

LcdItem::LcdItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &LcdItem::colorsChanged, this, [this] { update(); });
}

void LcdItem::setController(MeterController *controller)
{
    if (m_controller == controller)
        return;
    if (m_controller)
        m_controller->disconnect(this);
    m_controller = controller;
    if (m_controller)
        connect(m_controller, &MeterController::readingChanged, this, [this] { update(); });
    emit controllerChanged();
    update();
}

void LcdItem::paint(QPainter *painter)
{
    const qreal scale = qMin(width() / CanvasWidth, height() / CanvasHeight);
    painter->save();
    painter->translate((width() - CanvasWidth * scale) / 2, (height() - CanvasHeight * scale) / 2);
    painter->scale(scale, scale);
    painter->setRenderHint(QPainter::Antialiasing);

    // The backlight (MODE held) brightens the glass a little, as the meter's white LEDs do.
    QColor glass = m_backlight ? m_glass.lighter(135) : m_glass;
    QColor ghost = m_ink;
    ghost.setAlphaF(m_ghostOpacity);
    Painter d{painter, m_ink, ghost, glass};

    painter->setPen(Qt::NoPen);
    painter->setBrush(m_bezel);
    painter->drawRoundedRect(QRectF(0, 0, CanvasWidth, CanvasHeight), 14, 14);
    painter->setBrush(glass);
    painter->drawRoundedRect(QRectF(8, 8, CanvasWidth - 16, CanvasHeight - 16), 9, 9);

    const MeterReading *r = nullptr;
    std::optional<MeterReading> latest;
    if (m_controller && m_controller->session()) {
        latest = m_controller->session()->latest();
        if (latest)
            r = &*latest;
    }
    const Annunciators i = r ? r->icons : Annunciators{};

    // Top row: MIN MAX AVG LowZ (separate symbols, so "all three" lights all three).
    d.label("MIN", 18, 18, 10, i.min, Qt::AlignLeft, 26);
    d.label("MAX", 44, 18, 10, i.max, Qt::AlignLeft, 28);
    d.label("AVG", 72, 18, 10, i.average, Qt::AlignLeft, 28);
    d.label("LowZ", 100, 18, 10, i.lowZ, Qt::AlignLeft, 34);

    // Diode, beep, the 1 kHz low-pass filter, Bluetooth.
    {
        const QColor c = d.lit(i.diode);
        const QRectF rect(18, 38, 16, 9);
        painter->setPen(QPen(c, 1.4));
        painter->drawLine(QPointF(rect.left(), rect.center().y()), QPointF(rect.right(), rect.center().y()));
        QPainterPath tri;
        tri.moveTo(rect.left() + 3, rect.top());
        tri.lineTo(rect.left() + 3, rect.bottom());
        tri.lineTo(rect.right() - 4, rect.center().y());
        tri.closeSubpath();
        painter->fillPath(tri, c);
        painter->drawLine(QPointF(rect.right() - 4, rect.top()), QPointF(rect.right() - 4, rect.bottom()));
    }
    d.label("·))", 38, 36, 10, i.beep, Qt::AlignLeft, 20);
    {
        const QColor c = d.lit(i.lowPassFilter);
        painter->setPen(c);
        painter->setFont(d.font(9));
        painter->drawText(QRectF(60, 38, 30, 12), Qt::AlignLeft | Qt::AlignVCenter, "1kHz");
        painter->setPen(QPen(c, 1.4));
        painter->drawLine(QPointF(61, 36), QPointF(84, 36));
        painter->drawLine(QPointF(84, 36), QPointF(90, 49));
    }
    d.label("BT", 96, 34, 13, i.bluetooth, Qt::AlignLeft, 24);

    // AC over a bar over DC, beside the sub display.
    d.label("AC", 156, 17, 9, i.ac, Qt::AlignHCenter, 22);
    painter->setPen(Qt::NoPen);
    painter->setBrush(d.lit(i.ac || i.dc));
    painter->drawRect(QRectF(160, 31, 14, 2));
    d.label("DC", 156, 35, 9, i.dc, Qt::AlignHCenter, 22);

    // Sub display.
    {
        const QRectF area(196, 16, 132, 34);
        painter->setPen(Qt::NoPen);
        painter->setBrush(d.lit(r && r->sub.negative));
        painter->drawRect(QRectF(area.x() - 12, area.y() + area.height() / 2 - 1.5, 9, 3));
        d.text(r ? r->sub.text : QString(), area, 5);
        d.label(r ? r->sub.unit : QString(), area.right() + 4, area.y() + 12, 13, true, Qt::AlignLeft, 64);
    }

    // Status row.
    {
        const qreal y = 57;
        d.boxed("AUTO", 18, y, 34, i.autoRange);
        d.label("DC", 56, y - 1, 10, i.acPlusDc, Qt::AlignLeft, 18);
        d.label("+", 72, y - 1, 10, i.acPlusDc, Qt::AlignLeft, 10);
        d.label("AC", 81, y - 1, 10, i.acPlusDc, Qt::AlignLeft, 18);
        // What a SETUP page is about ("Battery"), which the meter only hints at in the digits.
        if (r && !r->sub.label.isEmpty())
            d.label(r->sub.label.toUpper(), 104, y - 1, 9, true, Qt::AlignRight, 120);
        d.boxed("MEM", 230, y, 30, i.memory);
        d.boxed("A-", 264, y, 18, i.autoHold);
        d.boxed("HOLD", 284, y, 34, i.hold);
        d.label("1ms", 322, y - 1, 11, i.oneMsPeak, Qt::AlignLeft, 30);
    }

    // Left column: battery, Apo, REL, TEST, the arrow.
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(d.lit(i.lowBattery));
        painter->drawRoundedRect(QRectF(18, 78, 24, 12), 2, 2);
        painter->drawRect(QRectF(42, 81, 3, 6));
        painter->setPen(i.lowBattery ? glass : ghost);
        painter->setFont(d.font(10));
        painter->drawText(QRectF(18, 78, 24, 12), Qt::AlignCenter, "+ -");
        d.label("Apo", 16, 94, 11, i.autoPowerOff, Qt::AlignLeft, 34);
        d.label("REL", 16, 110, 10, i.relative, Qt::AlignLeft, 34);
        d.label("TEST", 16, 124, 9, i.test, Qt::AlignLeft, 34);
        QPainterPath arrow;
        arrow.moveTo(22, 154);
        arrow.lineTo(28, 144);
        arrow.lineTo(34, 154);
        arrow.closeSubpath();
        painter->fillPath(arrow, d.lit(i.arrow));
    }

    // Main digits, minus sign and unit.
    {
        const QRectF digits(78, 78, 250, 80);
        painter->setPen(Qt::NoPen);
        painter->setBrush(d.lit(r && r->mainNegative && !r->mainOverload));
        painter->drawRoundedRect(QRectF(56, digits.y() + digits.height() / 2 - 4, 20, 8), 2, 2);
        d.text(r ? r->mainText : QString(), digits, 5);
        d.label(r ? r->mainUnit : QString(), 334, 120, 24, true, Qt::AlignLeft, 80);
        if (i.decibels)
            d.label("dBm", 334, 92, 14, true);
    }

    // The chevron bar graph with its sign and scale.
    {
        const int total = BarGraph::TotalSegments;
        const qreal left = 50, top = 188, w = 340, h = 14, segment = w / total;
        const bool on = r && r->bar.on;
        const int lit = r ? r->bar.segments : 0;
        painter->setPen(Qt::NoPen);
        painter->setBrush(d.lit(on));
        painter->drawRect(QRectF(24, top + 5, 14, 4));
        painter->setBrush(d.lit(on && !r->bar.negative));
        painter->drawRect(QRectF(29, top, 4, 14));
        for (int s = 0; s < total; ++s) {
            const qreal x = left + s * segment, sw = segment - 1.5, notch = sw * 0.4;
            QPainterPath chevron;
            chevron.moveTo(x, top);
            chevron.lineTo(x + sw - notch, top);
            chevron.lineTo(x + sw, top + h / 2);
            chevron.lineTo(x + sw - notch, top + h);
            chevron.lineTo(x, top + h);
            if (s > 0)
                chevron.lineTo(x + notch, top + h / 2);
            chevron.closeSubpath();
            painter->fillPath(chevron, d.lit(s < lit));
        }
        if (on) {
            const int full = r->bar.fullScale();
            for (int k = 0; k <= 5; ++k) {
                const qreal x = left + w * k / 5.0;
                painter->setPen(Qt::NoPen);
                painter->setBrush(m_ink);
                painter->drawRect(QRectF(x - 0.5, top - 6, 1, 4));
                d.label(QString::number(full * k / 5), x - 20, top - 19, 9, true, Qt::AlignHCenter, 40);
            }
        }
    }
    painter->restore();
}

} // namespace readout
