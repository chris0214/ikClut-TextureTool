#pragma once

#include "render/ScopeRenderer.h"

#include <QWidget>

namespace ikclut {

class ScopeWidget : public QWidget {
    Q_OBJECT

public:
    explicit ScopeWidget(QWidget* parent = nullptr);
    void setScopeData(const ScopeData& data);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void drawHistogram(QPainter& painter, const QRect& rect, const QVector<int>& hist, const QColor& color);
    void drawPanelFrame(QPainter& painter, const QRect& rect, const QString& title);
    void drawExposureAnalysis(QPainter& painter, const QRect& rect);
    void drawWaveform(QPainter& painter, const QRect& rect, const QVector<QPointF>& samples, const QColor& color);
    void drawVectorscope(QPainter& painter, const QRect& rect);
    QString clippingSummary() const;

    ScopeData m_data;
};

} // namespace ikclut
