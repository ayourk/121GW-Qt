// SPDX-License-Identifier: MIT
// Where the front panel's elements go: the QML defaults, nudged in the app's layout edit mode
// (offsets saved to ~/.config/121gw-qt/layout.json), or taken from an optional SVG face whose
// element ids name them (~/.config/121gw-qt/face.svg).
#pragma once

#include <QHash>
#include <QObject>
#include <QQuickImageProvider>
#include <QPointer>
#include <QRectF>
#include <QUrl>
#include <QVariantMap>
#include <memory>

class QQuickItem;
class QSvgRenderer;

namespace readout {

class FaceLayout : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool editMode READ editMode WRITE setEditMode NOTIFY editModeChanged)
    Q_PROPERTY(QString selected READ selected WRITE setSelected NOTIFY selectedChanged)
    // name -> [dx, dy] in face units (the face is 500×1000)
    Q_PROPERTY(QVariantMap offsets READ offsets NOTIFY offsetsChanged)
    Q_PROPERTY(bool hasSvg READ hasSvg CONSTANT)
    // The SVG supplies the base under everything (its "artwork" group): QML's boot and face plate hide.
    Q_PROPERTY(bool hasBase READ hasBase CONSTANT)
    // The SVG has an "overlay" group, drawn above the live parts (glare on the LCD glass…).
    Q_PROPERTY(bool hasOverlay READ hasOverlay CONSTANT)
    Q_PROPERTY(QUrl svgUrl READ svgUrl CONSTANT)
    // True while the face is grabbed for --export-svg: live parts hide so only the artwork is captured.
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)
public:
    static constexpr qreal FaceWidth = 500, FaceHeight = 1000;

    FaceLayout(QString layoutFile, const QString &svgFile, QObject *parent = nullptr);
    ~FaceLayout() override;

    bool editMode() const { return m_editMode; }
    void setEditMode(bool on);
    QString selected() const { return m_selected; }
    void setSelected(const QString &name);
    QVariantMap offsets() const { return m_offsets; }
    bool hasSvg() const;
    bool hasBase() const;
    bool hasOverlay() const { return svgHas(QStringLiteral("overlay")); }
    // Draws part of the SVG face into an image of size: the "artwork" base or the "overlay" group,
    // both at their place on the face; or one element's own artwork ("art-btn-mode"), filling the
    // image. An empty art-<name> group just stops QML drawing that element's artwork.
    QImage renderLayer(const QString &layer, const QSize &size) const;
    QUrl svgUrl() const { return m_svgUrl; }
    bool exporting() const { return m_exporting; }

    Q_INVOKABLE void nudge(const QString &name, qreal dx, qreal dy);
    Q_INVOKABLE void resetOffset(const QString &name);
    // Whether the SVG face has an element with this id, and its bounds in face units.
    Q_INVOKABLE bool svgHas(const QString &name) const;
    Q_INVOKABLE QRectF svgRect(const QString &name) const;
    // Every placed element reports where it is, for --export-svg.
    Q_INVOKABLE void report(const QString &name, qreal x, qreal y, qreal w, qreal h);

    // Writes a starter SVG: the face's artwork as a background image, and one named, outlined
    // rect per element, to move in a vector editor. Calls done(ok, message) when written.
    void exportSvg(QQuickItem *face, const QString &file, std::function<void(bool, QString)> done);

signals:
    void editModeChanged();
    void selectedChanged();
    void offsetsChanged();
    void exportingChanged();

private:
    void save();
    friend class FaceImageProvider;

    QString m_layoutFile;
    QUrl m_svgUrl;
    std::unique_ptr<QSvgRenderer> m_svg;
    qreal m_svgScale = 1;
    QPointF m_svgOrigin;
    bool m_editMode = false;
    QString m_selected;
    QVariantMap m_offsets;
    QHash<QString, QRectF> m_reported;
    bool m_exporting = false;
};

// image://face/artwork and image://face/overlay for QML.
class FaceImageProvider : public QQuickImageProvider
{
public:
    explicit FaceImageProvider(FaceLayout *layout)
        : QQuickImageProvider(QQuickImageProvider::Image), m_layout(layout) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    FaceLayout *m_layout;
};

} // namespace readout
