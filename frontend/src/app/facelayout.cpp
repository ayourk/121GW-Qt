// SPDX-License-Identifier: MIT
#include "facelayout.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QPainter>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QSaveFile>
#include <QSvgRenderer>
#include <QTimer>
#include <algorithm>

Q_LOGGING_CATEGORY(lcLayout, "121gw.layout")

namespace readout {

FaceLayout::FaceLayout(QString layoutFile, const QString &svgFile, QObject *parent)
    : QObject(parent), m_layoutFile(std::move(layoutFile))
{
    QFile f(m_layoutFile);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject offsets = QJsonDocument::fromJson(f.readAll()).object().value("offsets").toObject();
        for (auto it = offsets.begin(); it != offsets.end(); ++it) {
            const QJsonArray a = it.value().toArray();
            if (a.size() == 2)
                m_offsets.insert(it.key(), QVariantList{a[0].toDouble(), a[1].toDouble()});
        }
    }
    if (!svgFile.isEmpty() && QFileInfo::exists(svgFile)) {
        m_svg = std::make_unique<QSvgRenderer>(svgFile);
        if (m_svg->isValid()) {
            // The SVG's viewBox maps onto the 500×1000 face.
            const QRectF box = m_svg->viewBoxF();
            m_svgScale = box.width() > 0 ? FaceWidth / box.width() : 1;
            m_svgOrigin = box.topLeft();
            m_svgUrl = QUrl::fromLocalFile(QFileInfo(svgFile).absoluteFilePath());
            qCInfo(lcLayout) << "face from" << svgFile;
        } else {
            qCWarning(lcLayout) << "can't read" << svgFile << "- using the built-in face";
            m_svg.reset();
        }
    }
}

FaceLayout::~FaceLayout() = default;

void FaceLayout::setEditMode(bool on)
{
    if (m_editMode == on)
        return;
    m_editMode = on;
    if (!on)
        setSelected({});
    emit editModeChanged();
}

void FaceLayout::setSelected(const QString &name)
{
    if (m_selected == name)
        return;
    m_selected = name;
    emit selectedChanged();
}

bool FaceLayout::hasSvg() const
{
    return m_svg != nullptr;
}

bool FaceLayout::hasBase() const
{
    return m_svg && m_svg->elementExists(QStringLiteral("artwork"));
}

void FaceLayout::nudge(const QString &name, qreal dx, qreal dy)
{
    if (name.isEmpty())
        return;
    const QVariantList cur = m_offsets.value(name).toList();
    const qreal x = (cur.size() == 2 ? cur[0].toReal() : 0) + dx;
    const qreal y = (cur.size() == 2 ? cur[1].toReal() : 0) + dy;
    if (x == 0 && y == 0)
        m_offsets.remove(name);
    else
        m_offsets.insert(name, QVariantList{x, y});
    emit offsetsChanged();
    save();
}

void FaceLayout::resetOffset(const QString &name)
{
    if (m_offsets.remove(name) > 0) {
        emit offsetsChanged();
        save();
    }
}

bool FaceLayout::svgHas(const QString &name) const
{
    return m_svg && m_svg->elementExists(name);
}

QRectF FaceLayout::svgRect(const QString &name) const
{
    if (!svgHas(name))
        return {};
    const QRectF r = m_svg->transformForElement(name).mapRect(m_svg->boundsOnElement(name));
    return {(r.x() - m_svgOrigin.x()) * m_svgScale, (r.y() - m_svgOrigin.y()) * m_svgScale,
            r.width() * m_svgScale, r.height() * m_svgScale};
}

QImage FaceLayout::renderLayer(const QString &layer, const QSize &size) const
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!m_svg)
        return image;
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal sx = size.width() / FaceWidth, sy = size.height() / FaceHeight;
    if (layer.startsWith(QLatin1String("art-"))) {
        // One element's own artwork, filling the image (QML sizes and places it).
        if (m_svg->elementExists(layer))
            m_svg->render(&painter, layer, QRectF(QPointF(0, 0), QSizeF(size)));
    } else if (m_svg->elementExists(layer)) {
        const QRectF r = svgRect(layer);
        m_svg->render(&painter, layer, QRectF(r.x() * sx, r.y() * sy, r.width() * sx, r.height() * sy));
    }
    return image;
}

QImage FaceImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QSize out = requestedSize.isValid() && !requestedSize.isEmpty()
                          ? requestedSize : QSize(int(FaceLayout::FaceWidth), int(FaceLayout::FaceHeight));
    if (size)
        *size = out;
    return m_layout->renderLayer(id.section(u'?', 0, 0), out);
}

void FaceLayout::report(const QString &name, qreal x, qreal y, qreal w, qreal h)
{
    m_reported.insert(name, QRectF(x, y, w, h));
}

void FaceLayout::save()
{
    QJsonObject offsets;
    for (auto it = m_offsets.begin(); it != m_offsets.end(); ++it) {
        const QVariantList v = it.value().toList();
        offsets.insert(it.key(), QJsonArray{v[0].toDouble(), v[1].toDouble()});
    }
    QSaveFile f(m_layoutFile);
    if (!f.open(QIODevice::WriteOnly)) {
        qCWarning(lcLayout) << "can't save" << m_layoutFile << f.errorString();
        return;
    }
    f.write(QJsonDocument(QJsonObject{{"offsets", offsets}}).toJson(QJsonDocument::Indented));
    f.commit();
}

void FaceLayout::exportSvg(QQuickItem *face, const QString &file, std::function<void(bool, QString)> done)
{
    m_exporting = true;
    emit exportingChanged();
    // Let the scene hide the live parts before grabbing.
    QTimer::singleShot(300, this, [this, face, file, done] {
        const auto grab = face->grabToImage(QSize(int(FaceWidth * 2), int(FaceHeight * 2)));
        if (!grab) {
            m_exporting = false;
            emit exportingChanged();
            done(false, QStringLiteral("couldn't grab the face"));
            return;
        }
        connect(grab.data(), &QQuickItemGrabResult::ready, this, [this, grab, file, done] {
            m_exporting = false;
            emit exportingChanged();
            QByteArray png;
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            grab->image().save(&buffer, "PNG");

            QStringList names = m_reported.keys();
            std::sort(names.begin(), names.end());
            QString svg;
            svg += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                  "<!-- 121gw-qt face template. The app draws the \"artwork\" group under its live parts\n"
                                  "     (LCD, knob, highlights) and an optional \"overlay\" group above them (glare, bezel\n"
                                  "     lips). The rects in \"elements\" are never drawn: each id places that front panel\n"
                                  "     element (centered on the rect). Move them, and replace the artwork with your own. -->\n"
                                  "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
                                  "     width=\"500\" height=\"1000\" viewBox=\"0 0 500 1000\" version=\"1.2\" baseProfile=\"tiny\">\n");
            svg += QStringLiteral("  <g id=\"artwork\">\n    <image x=\"0\" y=\"0\" width=\"500\" height=\"1000\" "
                                  "xlink:href=\"data:image/png;base64,%1\"/>\n  </g>\n")
                       .arg(QString::fromLatin1(png.toBase64()));
            svg += QStringLiteral("  <g id=\"elements\" fill=\"none\" stroke=\"#FF00FF\" stroke-width=\"0.5\">\n");
            for (const QString &n : names) {
                const QRectF r = m_reported.value(n);
                svg += QStringLiteral("    <rect id=\"%1\" x=\"%2\" y=\"%3\" width=\"%4\" height=\"%5\"/>\n")
                           .arg(n)
                           .arg(r.x(), 0, 'f', 1)
                           .arg(r.y(), 0, 'f', 1)
                           .arg(r.width(), 0, 'f', 1)
                           .arg(r.height(), 0, 'f', 1);
            }
            svg += QStringLiteral("  </g>\n</svg>\n");

            QSaveFile out(file);
            if (!out.open(QIODevice::WriteOnly)) {
                done(false, out.errorString());
                return;
            }
            out.write(svg.toUtf8());
            if (!out.commit()) {
                done(false, out.errorString());
                return;
            }
            done(true, QStringLiteral("%1 elements").arg(names.size()));
        });
    });
}

} // namespace readout
