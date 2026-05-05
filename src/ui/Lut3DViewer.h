#pragma once

#include "core/Lut3D.h"

#include <QMatrix4x4>
#include <QWidget>

namespace ikclut {

enum class Lut3DViewMode {
    Overlay,
    DeltaLines,
    Heatmap
};

class Lut3DViewer : public QWidget {
    Q_OBJECT

public:
    explicit Lut3DViewer(QWidget* parent = nullptr);

    void setLut(const Lut3D& lut);
    void setDisplaySize(int size);
    void setViewMode(Lut3DViewMode mode);
    void resetView();

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QPointF project(const QVector3D& point, const QMatrix4x4& transform) const;
    void updateMetrics();

    Lut3D m_lut = Lut3D::identity(16);
    Lut3DViewMode m_viewMode = Lut3DViewMode::Overlay;
    int m_displaySize = 17;
    float m_averageDelta = 0.0f;
    float m_maxDelta = 0.0f;
    float m_yaw = -28.0f;
    float m_pitch = 18.0f;
    float m_zoom = 1.0f;
    QPoint m_lastMouse;
};

} // namespace ikclut
