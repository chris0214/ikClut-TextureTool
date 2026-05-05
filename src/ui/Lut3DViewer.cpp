#include "ui/Lut3DViewer.h"

#include <algorithm>
#include <vector>

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace ikclut {

Lut3DViewer::Lut3DViewer(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 280);
}

void Lut3DViewer::setLut(const Lut3D& lut)
{
    if (lut.isValid()) {
        m_lut = lut.resampled(m_displaySize);
        updateMetrics();
        update();
    }
}

void Lut3DViewer::setDisplaySize(int size)
{
    m_displaySize = std::clamp(size, 9, 33);
    m_lut = m_lut.resampled(m_displaySize);
    updateMetrics();
    update();
}

void Lut3DViewer::setViewMode(Lut3DViewMode mode)
{
    m_viewMode = mode;
    update();
}

void Lut3DViewer::resetView()
{
    m_yaw = -28.0f;
    m_pitch = 18.0f;
    m_zoom = 1.0f;
    update();
}

void Lut3DViewer::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(8, 9, 12));

    QLinearGradient background(rect().topLeft(), rect().bottomRight());
    background.setColorAt(0.0, QColor(18, 20, 25));
    background.setColorAt(0.55, QColor(11, 12, 15));
    background.setColorAt(1.0, QColor(6, 7, 10));
    painter.fillRect(rect(), background);

    QMatrix4x4 transform;
    transform.rotate(m_pitch, 1, 0, 0);
    transform.rotate(m_yaw, 0, 1, 0);

    const int size = m_lut.size();
    if (size < 2) {
        return;
    }

    struct ProjectedPoint {
        QPointF screen;
        float depth = 0.0f;
    };
    struct DrawablePoint {
        QPointF screen;
        QColor color;
        float depth = 0.0f;
        float delta = 0.0f;
    };
    struct DrawableLine {
        QPointF from;
        QPointF to;
        QColor color;
        float depth = 0.0f;
        float width = 1.0f;
    };

    auto projectPoint = [this, &transform](const QVector3D& point) {
        const QVector3D centered = point - QVector3D(0.5f, 0.5f, 0.5f);
        const QVector3D rotated = transform.map(centered);
        const float scale = std::min(width(), height()) * 0.58f * m_zoom;
        return ProjectedPoint{
            QPointF(width() * 0.5f + rotated.x() * scale + rotated.z() * scale * 0.12f,
                    height() * 0.55f - rotated.y() * scale + rotated.z() * scale * 0.28f),
            rotated.z()
        };
    };

    auto heatColor = [](float value) {
        const float heat = std::clamp(value, 0.0f, 1.0f);
        return QColor::fromRgbF(0.16f + heat * 0.84f,
                                0.68f - heat * 0.42f,
                                1.0f - heat * 0.88f,
                                0.94f);
    };

    auto drawLabel = [&painter](const QPointF& p, const QString& label, const QColor& color) {
        painter.setPen(color);
        painter.setBrush(QColor(9, 10, 13, 180));
        const QRectF box(p.x() + 6, p.y() - 12, 20, 18);
        painter.drawRoundedRect(box, 3, 3);
        painter.drawText(box, Qt::AlignCenter, label);
    };

    const float denom = static_cast<float>(size - 1);
    std::vector<DrawableLine> lines;
    std::vector<DrawablePoint> points;
    lines.reserve(4000);
    points.reserve(static_cast<size_t>(size * size * size));

    const int gridStep = std::max(1, (size - 1) / 4);
    for (int i = 0; i < size; i += gridStep) {
        const float t = i / denom;
        const QVector<QPair<QVector3D, QVector3D>> grid = {
            {QVector3D(t, 0, 0), QVector3D(t, 1, 0)},
            {QVector3D(t, 0, 1), QVector3D(t, 1, 1)},
            {QVector3D(0, t, 0), QVector3D(1, t, 0)},
            {QVector3D(0, t, 1), QVector3D(1, t, 1)},
            {QVector3D(0, 0, t), QVector3D(1, 0, t)},
            {QVector3D(0, 1, t), QVector3D(1, 1, t)}
        };
        for (const auto& segment : grid) {
            const ProjectedPoint a = projectPoint(segment.first);
            const ProjectedPoint b = projectPoint(segment.second);
            lines.push_back({a.screen, b.screen, QColor(104, 110, 121, 56), (a.depth + b.depth) * 0.5f, 1.0f});
        }
    }

    const QVector<QPair<QVector3D, QVector3D>> cubeEdges = {
        {QVector3D(0, 0, 0), QVector3D(1, 0, 0)}, {QVector3D(0, 1, 0), QVector3D(1, 1, 0)},
        {QVector3D(0, 0, 1), QVector3D(1, 0, 1)}, {QVector3D(0, 1, 1), QVector3D(1, 1, 1)},
        {QVector3D(0, 0, 0), QVector3D(0, 1, 0)}, {QVector3D(1, 0, 0), QVector3D(1, 1, 0)},
        {QVector3D(0, 0, 1), QVector3D(0, 1, 1)}, {QVector3D(1, 0, 1), QVector3D(1, 1, 1)},
        {QVector3D(0, 0, 0), QVector3D(0, 0, 1)}, {QVector3D(1, 0, 0), QVector3D(1, 0, 1)},
        {QVector3D(0, 1, 0), QVector3D(0, 1, 1)}, {QVector3D(1, 1, 0), QVector3D(1, 1, 1)}
    };
    for (const auto& segment : cubeEdges) {
        const ProjectedPoint a = projectPoint(segment.first);
        const ProjectedPoint b = projectPoint(segment.second);
        lines.push_back({a.screen, b.screen, QColor(180, 186, 198, 115), (a.depth + b.depth) * 0.5f, 1.2f});
    }

    const int lineStride = m_viewMode == Lut3DViewMode::DeltaLines ? 2 : 5;
    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                const QVector3D identity(r / denom, g / denom, b / denom);
                const QVector3D mapped = m_lut.value(r, g, b);
                const ProjectedPoint p0 = projectPoint(identity);
                const ProjectedPoint p1 = projectPoint(mapped);
                const float delta = (mapped - identity).length();
                if (m_viewMode == Lut3DViewMode::DeltaLines && (r + g + b) % lineStride == 0) {
                    const int alpha = std::clamp(static_cast<int>(60 + delta * 380), 60, 230);
                    lines.push_back({p0.screen, p1.screen, QColor(255, 194, 92, alpha), (p0.depth + p1.depth) * 0.5f, 1.0f + std::min(delta * 2.0f, 1.4f)});
                }
                if (m_viewMode == Lut3DViewMode::Overlay && (r + g + b) % 5 == 0) {
                    lines.push_back({p0.screen, p1.screen, QColor(145, 152, 165, 42), (p0.depth + p1.depth) * 0.5f, 0.8f});
                }

                QColor pointColor;
                if (m_viewMode == Lut3DViewMode::Heatmap) {
                    const float heat = std::clamp(delta / std::max(0.001f, m_maxDelta), 0.0f, 1.0f);
                    pointColor = heatColor(heat);
                } else {
                    pointColor = QColor::fromRgbF(std::clamp(mapped.x(), 0.0f, 1.0f),
                                                  std::clamp(mapped.y(), 0.0f, 1.0f),
                                                  std::clamp(mapped.z(), 0.0f, 1.0f),
                                                  0.9f);
                }
                points.push_back({m_viewMode == Lut3DViewMode::DeltaLines ? p0.screen : p1.screen,
                                  pointColor,
                                  m_viewMode == Lut3DViewMode::DeltaLines ? p0.depth : p1.depth,
                                  delta});
            }
        }
    }

    std::sort(lines.begin(), lines.end(), [](const DrawableLine& a, const DrawableLine& b) {
        return a.depth < b.depth;
    });
    std::sort(points.begin(), points.end(), [](const DrawablePoint& a, const DrawablePoint& b) {
        return a.depth < b.depth;
    });

    for (const DrawableLine& line : lines) {
        painter.setPen(QPen(line.color, line.width));
        painter.drawLine(line.from, line.to);
    }

    const float pointRadius = size <= 11 ? 2.2f : (size <= 17 ? 1.65f : 1.15f);
    for (const DrawablePoint& point : points) {
        QColor glow = point.color;
        glow.setAlpha(std::min(255, point.color.alpha() + 32));
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(point.screen, pointRadius, pointRadius);
    }

    const ProjectedPoint origin = projectPoint(QVector3D(0, 0, 0));
    const ProjectedPoint rx = projectPoint(QVector3D(1.15f, 0, 0));
    const ProjectedPoint gy = projectPoint(QVector3D(0, 1.15f, 0));
    const ProjectedPoint bz = projectPoint(QVector3D(0, 0, 1.15f));

    painter.setPen(QPen(QColor(222, 74, 70), 2.0));
    painter.drawLine(origin.screen, rx.screen);
    drawLabel(rx.screen, "R", QColor(255, 120, 116));
    painter.setPen(QPen(QColor(74, 205, 112), 2.0));
    painter.drawLine(origin.screen, gy.screen);
    drawLabel(gy.screen, "G", QColor(116, 235, 146));
    painter.setPen(QPen(QColor(92, 142, 255), 2.0));
    painter.drawLine(origin.screen, bz.screen);
    drawLabel(bz.screen, "B", QColor(130, 165, 255));

    const QString modeText = m_viewMode == Lut3DViewMode::Overlay
        ? tr("Overlay")
        : (m_viewMode == Lut3DViewMode::DeltaLines ? tr("Delta Lines") : tr("Heatmap"));

    const QRect titleRect(12, 10, width() - 24, 28);
    painter.setPen(QColor(225, 228, 235));
    painter.drawText(titleRect,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("3D LUT - %1 | avg delta %2 | max %3")
                         .arg(modeText)
                         .arg(m_averageDelta, 0, 'f', 3)
                         .arg(m_maxDelta, 0, 'f', 3));

    const QRect statsRect(width() - 178, 42, 160, 72);
    painter.setPen(QColor(68, 74, 84));
    painter.setBrush(QColor(11, 12, 16, 205));
    painter.drawRoundedRect(statsRect, 5, 5);
    painter.setPen(QColor(199, 204, 214));
    painter.drawText(statsRect.adjusted(10, 7, -10, -7),
                     Qt::AlignLeft | Qt::AlignTop,
                     tr("Samples %1\nGrid %2^3\nZoom %3x")
                         .arg(points.size())
                         .arg(size)
                         .arg(m_zoom, 0, 'f', 2));

    if (m_viewMode == Lut3DViewMode::Heatmap) {
        const QRect legend(width() - 178, statsRect.bottom() + 10, 160, 18);
        QLinearGradient gradient(legend.topLeft(), legend.topRight());
        gradient.setColorAt(0.0, heatColor(0.0f));
        gradient.setColorAt(0.5, heatColor(0.5f));
        gradient.setColorAt(1.0, heatColor(1.0f));
        painter.setPen(QColor(68, 74, 84));
        painter.setBrush(gradient);
        painter.drawRoundedRect(legend, 3, 3);
        painter.setPen(QColor(205, 208, 216));
        painter.drawText(legend.adjusted(0, 18, 0, 18), Qt::AlignLeft, "0");
        painter.drawText(legend.adjusted(0, 18, 0, 18), Qt::AlignRight, QString::number(m_maxDelta, 'f', 3));
    }
}

void Lut3DViewer::wheelEvent(QWheelEvent* event)
{
    m_zoom = std::clamp(m_zoom * (event->angleDelta().y() > 0 ? 1.08f : 0.92f), 0.3f, 4.0f);
    update();
}

void Lut3DViewer::mousePressEvent(QMouseEvent* event)
{
    m_lastMouse = event->pos();
}

void Lut3DViewer::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        const QPoint delta = event->pos() - m_lastMouse;
        m_yaw += delta.x() * 0.45f;
        m_pitch += delta.y() * 0.45f;
        m_pitch = std::clamp(m_pitch, -80.0f, 80.0f);
        m_lastMouse = event->pos();
        update();
    }
}

QPointF Lut3DViewer::project(const QVector3D& point, const QMatrix4x4& transform) const
{
    const QVector3D centered = point - QVector3D(0.5f, 0.5f, 0.5f);
    const QVector3D rotated = transform.map(centered);
    const float scale = std::min(width(), height()) * 0.58f * m_zoom;
    return QPointF(width() * 0.5f + rotated.x() * scale + rotated.z() * scale * 0.12f,
                   height() * 0.54f - rotated.y() * scale + rotated.z() * scale * 0.28f);
}

void Lut3DViewer::updateMetrics()
{
    m_averageDelta = 0.0f;
    m_maxDelta = 0.0f;
    const int size = m_lut.size();
    if (size < 2) {
        return;
    }

    const float denom = static_cast<float>(size - 1);
    double total = 0.0;
    int count = 0;
    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                const QVector3D identity(r / denom, g / denom, b / denom);
                const float delta = (m_lut.value(r, g, b) - identity).length();
                total += delta;
                m_maxDelta = std::max(m_maxDelta, delta);
                ++count;
            }
        }
    }
    m_averageDelta = count > 0 ? static_cast<float>(total / count) : 0.0f;
}

} // namespace ikclut
