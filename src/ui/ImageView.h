#pragma once

#include "model/ColorTypes.h"

#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QWidget>

namespace ikclut {

enum class ImageWarningOverlay {
    None,
    ShadowClip,
    HighlightClip,
    Gamut,
    FalseColor
};

class ImageView : public QWidget {
    Q_OBJECT

public:
    explicit ImageView(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setLabel(const QString& label);
    void setOverlayInfo(const QString& info);
    void setWarningOverlay(ImageWarningOverlay overlay);
    void setMaskOverlay(const QImage& overlay);
    void setMaskGuide(MaskKind kind, const QJsonObject& params, bool visible);
    void setMaskEditMode(bool enabled, const QString& hint = QString());
    void setPickMode(bool enabled, const QString& hint = QString());
    bool pickMode() const;
    QSize sizeHint() const override;

signals:
    void imageSampled(const QPoint& pixel, const QColor& color);
    void maskPointSampled(const QPointF& uv, const QColor& color);
    void maskDragCompleted(const QPointF& startUv, const QPointF& endUv);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QRectF imageRect() const;
    QPoint imagePixelAt(const QPoint& pos, bool* ok = nullptr) const;
    QPointF imageUvAt(const QPoint& pos, bool* ok = nullptr) const;

    QImage m_image;
    QString m_label;
    QString m_overlayInfo;
    QString m_pickHint;
    QString m_maskEditHint;
    ImageWarningOverlay m_warningOverlay = ImageWarningOverlay::None;
    QImage m_maskOverlay;
    MaskKind m_maskGuideKind = MaskKind::Full;
    QJsonObject m_maskGuideParams;
    bool m_maskGuideVisible = false;
    float m_zoom = 1.0f;
    QPointF m_pan;
    QPoint m_lastMouse;
    bool m_pickMode = false;
    bool m_maskEditMode = false;
    QPoint m_maskDragStart;
};

} // namespace ikclut
