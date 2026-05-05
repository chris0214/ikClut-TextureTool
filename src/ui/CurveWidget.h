#pragma once

#include "model/ColorTypes.h"

#include <QPointF>
#include <QColor>
#include <QVector>
#include <QWidget>

namespace ikclut {

class CurveWidget : public QWidget {
    Q_OBJECT

public:
    explicit CurveWidget(QWidget* parent = nullptr);

    void setCurve(const QVector<QPointF>& curve);
    QVector<QPointF> curve() const;
    void setHandleModes(const QVector<CurveHandleMode>& modes);
    QVector<CurveHandleMode> handleModes() const;
    void setChannelColor(const QColor& color);
    void setHistogram(const QVector<float>& histogram);
    void setNeutralLineY(double value);
    void setSelectedHandleMode(CurveHandleMode mode);
    CurveHandleMode selectedHandleMode() const;
    void setSelectedPoint(int index);
    int selectedPoint() const;

signals:
    void curveChanged(const QVector<QPointF>& curve);
    void handleModesChanged(const QVector<CurveHandleMode>& modes);
    void selectedPointChanged(int index, CurveHandleMode mode);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void resetCurve();
    QRectF graphRect() const;
    QPointF toGraph(const QPoint& pos) const;
    QPoint toWidget(const QPointF& point) const;
    int nearestPoint(const QPoint& pos) const;
    float evaluate(float x) const;
    float evaluateSegment(int index, float x) const;
    QPointF bezierHandle(int pointIndex, bool rightHandle) const;
    void ensureHandleModes();
    void normalize();

    QVector<QPointF> m_curve = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
    QVector<CurveHandleMode> m_handleModes = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    QVector<float> m_histogram;
    QColor m_channelColor = QColor(220, 152, 58);
    double m_neutralLineY = -1.0;
    int m_activePoint = -1;
    QPointF m_dragStartPoint;
    CurveHandleMode m_newPointHandleMode = CurveHandleMode::Auto;
};

} // namespace ikclut
