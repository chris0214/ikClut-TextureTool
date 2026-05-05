#include "ui/ScopeWidget.h"

#include <algorithm>

#include <QPainter>
#include <QPainterPath>

namespace ikclut {

ScopeWidget::ScopeWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(360);
}

void ScopeWidget::setScopeData(const ScopeData& data)
{
    m_data = data;
    update();
}

void ScopeWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(15, 16, 18));

    const int gap = 10;
    const int panelHeight = (height() - 44) / 4;
    const QRect histogramRect(8, 8, width() - 16, panelHeight);
    const QRect exposureRect(8, histogramRect.bottom() + gap, width() - 16, panelHeight);
    const QRect paradeRect(8, exposureRect.bottom() + gap, width() - 16, panelHeight);
    const QRect vectorRect(8, paradeRect.bottom() + gap, width() - 16, height() - paradeRect.bottom() - gap - 8);

    drawPanelFrame(painter, histogramRect, tr("Luma / RGB Histogram"));
    drawPanelFrame(painter, exposureRect, tr("Exposure Analysis 0-5"));
    drawPanelFrame(painter, paradeRect, tr("RGB Parade"));
    drawPanelFrame(painter, vectorRect, tr("Vectorscope"));

    const QRect histogramPlot = histogramRect.adjusted(8, 26, -8, -8);
    drawHistogram(painter, histogramPlot, m_data.luminanceHistogram, QColor(220, 220, 220, 185));
    drawHistogram(painter, histogramPlot, m_data.redHistogram, QColor(245, 78, 78, 85));
    drawHistogram(painter, histogramPlot, m_data.greenHistogram, QColor(76, 224, 126, 85));
    drawHistogram(painter, histogramPlot, m_data.blueHistogram, QColor(86, 140, 255, 85));
    drawExposureAnalysis(painter, exposureRect.adjusted(8, 26, -8, -8));
    drawWaveform(painter, paradeRect.adjusted(8, 26, -8, -8), m_data.redWaveform, QColor(245, 78, 78, 135));
    drawWaveform(painter, paradeRect.adjusted(8, 26, -8, -8), m_data.greenWaveform, QColor(76, 224, 126, 135));
    drawWaveform(painter, paradeRect.adjusted(8, 26, -8, -8), m_data.blueWaveform, QColor(86, 140, 255, 135));
    const QRect vectorScopeRect = vectorRect.adjusted(0, 24, 0, 0);
    drawVectorscope(painter, vectorScopeRect);

    painter.setPen(QColor(255, 196, 92));
    painter.drawText(QRect(14, histogramRect.bottom() - 24, histogramRect.width() - 12, 18),
                     Qt::AlignRight | Qt::AlignVCenter,
                     clippingSummary());
}

void ScopeWidget::drawPanelFrame(QPainter& painter, const QRect& rect, const QString& title)
{
    painter.save();
    painter.setPen(QColor(58, 62, 68));
    painter.setBrush(QColor(19, 21, 24));
    painter.drawRoundedRect(rect, 4, 4);
    painter.setPen(QColor(190, 194, 202));
    painter.drawText(QRect(rect.left() + 8, rect.top() + 4, rect.width() - 16, 18),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     title);
    painter.restore();
}

void ScopeWidget::drawHistogram(QPainter& painter, const QRect& rect, const QVector<int>& hist, const QColor& color)
{
    if (hist.isEmpty()) {
        return;
    }
    const int maxValue = *std::max_element(hist.constBegin(), hist.constEnd());
    if (maxValue <= 0) {
        return;
    }
    painter.setPen(color);
    for (int i = 0; i < hist.size(); ++i) {
        const float x = rect.left() + i * rect.width() / static_cast<float>(hist.size() - 1);
        const float h = rect.height() * (hist.at(i) / static_cast<float>(maxValue));
        painter.drawLine(QPointF(x, rect.bottom()), QPointF(x, rect.bottom() - h));
    }
}

void ScopeWidget::drawExposureAnalysis(QPainter& painter, const QRect& rect)
{
    painter.save();
    painter.setClipRect(rect.adjusted(0, 0, 1, 1));

    painter.setPen(QColor(50, 55, 62));
    for (int i = 0; i <= 5; ++i) {
        const double x = rect.left() + rect.width() * i / 5.0;
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        painter.setPen(QColor(150, 154, 162));
        painter.drawText(QRectF(x - 12, rect.bottom() - 17, 24, 14), Qt::AlignCenter, QString::number(i));
        painter.setPen(QColor(50, 55, 62));
    }
    for (int i = 1; i < 4; ++i) {
        const double y = rect.top() + rect.height() * i / 4.0;
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }

    const int zoneMax = m_data.exposureZoneCounts.isEmpty()
        ? 0
        : *std::max_element(m_data.exposureZoneCounts.constBegin(), m_data.exposureZoneCounts.constEnd());
    const int barAreaBottom = rect.bottom() - 18;
    if (zoneMax > 0) {
        const double zoneWidth = rect.width() / 6.0;
        for (int i = 0; i < m_data.exposureZoneCounts.size(); ++i) {
            const double ratio = m_data.exposureZoneCounts.at(i) / static_cast<double>(zoneMax);
            const QRectF bar(rect.left() + i * zoneWidth + 5,
                             barAreaBottom - (rect.height() - 24) * ratio,
                             zoneWidth - 10,
                             (rect.height() - 24) * ratio);
            const QColor color = QColor::fromHslF(0.09 + i * 0.018, 0.66, 0.42 + i * 0.055, 0.65);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawRoundedRect(bar, 2, 2);
        }
    }

    if (!m_data.exposureHistogram.isEmpty()) {
        const int maxValue = *std::max_element(m_data.exposureHistogram.constBegin(), m_data.exposureHistogram.constEnd());
        if (maxValue > 0) {
            QPainterPath path;
            for (int i = 0; i < m_data.exposureHistogram.size(); ++i) {
                const double x = rect.left() + rect.width() * i / static_cast<double>(m_data.exposureHistogram.size() - 1);
                const double y = barAreaBottom - (rect.height() - 24) * (m_data.exposureHistogram.at(i) / static_cast<double>(maxValue));
                if (i == 0) {
                    path.moveTo(x, y);
                } else {
                    path.lineTo(x, y);
                }
            }
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(QColor(255, 214, 128, 210), 2.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }
    }

    if (m_data.sampledPixels > 0) {
        const double meanX = rect.left() + rect.width() * std::clamp(m_data.exposureMean / 5.0, 0.0, 1.0);
        painter.setPen(QPen(QColor(255, 245, 210, 230), 1.4));
        painter.drawLine(QPointF(meanX, rect.top()), QPointF(meanX, barAreaBottom));
        painter.setPen(QColor(225, 226, 232));
        painter.drawText(QRect(rect.left() + 6, rect.top() + 4, rect.width() - 12, 18),
                         Qt::AlignRight | Qt::AlignVCenter,
                         tr("Avg %1 | <1 %2% | >=4 %3%")
                             .arg(m_data.exposureMean, 0, 'f', 2)
                             .arg(m_data.exposureLowPercent, 0, 'f', 1)
                             .arg(m_data.exposureHighPercent, 0, 'f', 1));
    }

    painter.restore();
}

void ScopeWidget::drawWaveform(QPainter& painter, const QRect& rect, const QVector<QPointF>& samples, const QColor& color)
{
    if (samples.isEmpty()) {
        return;
    }
    painter.save();
    painter.setClipRect(rect.adjusted(1, 1, -1, -1));
    painter.setPen(QColor(54, 58, 64, 130));
    for (int i = 1; i < 4; ++i) {
        const double y = rect.top() + rect.height() * i / 4.0;
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
    painter.setPen(color);
    for (const QPointF& sample : samples) {
        const QPointF p(rect.left() + sample.x() * rect.width(),
                        rect.bottom() - sample.y() * rect.height());
        painter.drawPoint(p);
    }
    painter.restore();
}

void ScopeWidget::drawVectorscope(QPainter& painter, const QRect& rect)
{
    painter.save();
    painter.setClipRect(rect.adjusted(1, 1, -1, -1));
    painter.translate(rect.center());
    const float scale = std::min(rect.width(), rect.height()) * 0.42f;
    painter.setPen(QColor(55, 65, 72));
    painter.drawEllipse(QPointF(0, 0), scale, scale);
    painter.drawEllipse(QPointF(0, 0), scale * 0.5, scale * 0.5);
    painter.drawLine(QPointF(-scale, 0), QPointF(scale, 0));
    painter.drawLine(QPointF(0, -scale), QPointF(0, scale));

    auto toScopePoint = [scale](const QPointF& point) {
        const double radius = std::hypot(point.x(), point.y());
        const double gain = radius > 0.98 ? 0.98 / radius : 1.0;
        return QPointF(point.x() * gain * scale, point.y() * gain * scale);
    };

    painter.setPen(QColor(240, 220, 120, 110));
    for (const QPointF& p : m_data.vectorscope) {
        painter.drawPoint(toScopePoint(p));
    }
    painter.restore();
}

QString ScopeWidget::clippingSummary() const
{
    if (m_data.sampledPixels <= 0) {
        return tr("Clip 0.00% | Gamut 0.00%");
    }
    const double shadow = 100.0 * m_data.shadowClipPixels / m_data.sampledPixels;
    const double highlight = 100.0 * m_data.highlightClipPixels / m_data.sampledPixels;
    const double gamut = 100.0 * m_data.gamutWarningPixels / m_data.sampledPixels;
    return tr("Shadow %1% | Highlight %2% | Gamut %3%")
        .arg(shadow, 0, 'f', 2)
        .arg(highlight, 0, 'f', 2)
        .arg(gamut, 0, 'f', 2);
}

} // namespace ikclut
