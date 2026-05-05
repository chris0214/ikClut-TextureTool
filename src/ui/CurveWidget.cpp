#include "ui/CurveWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace ikclut {

CurveWidget::CurveWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setMouseTracking(true);
}

void CurveWidget::setCurve(const QVector<QPointF>& curve)
{
    m_curve = curve;
    normalize();
    ensureHandleModes();
    update();
}

QVector<QPointF> CurveWidget::curve() const
{
    return m_curve;
}

void CurveWidget::setHandleModes(const QVector<CurveHandleMode>& modes)
{
    m_handleModes = modes;
    ensureHandleModes();
    update();
}

QVector<CurveHandleMode> CurveWidget::handleModes() const
{
    return m_handleModes;
}

void CurveWidget::setChannelColor(const QColor& color)
{
    m_channelColor = color;
    update();
}

void CurveWidget::setHistogram(const QVector<float>& histogram)
{
    m_histogram = histogram;
    update();
}

void CurveWidget::setNeutralLineY(double value)
{
    m_neutralLineY = value;
    update();
}

void CurveWidget::setSelectedHandleMode(CurveHandleMode mode)
{
    if (m_activePoint >= 0 && m_activePoint < m_handleModes.size()) {
        m_handleModes[m_activePoint] = mode;
        emit handleModesChanged(m_handleModes);
        emit selectedPointChanged(m_activePoint, mode);
    } else {
        m_newPointHandleMode = mode;
        emit selectedPointChanged(-1, m_newPointHandleMode);
    }
    update();
}

CurveHandleMode CurveWidget::selectedHandleMode() const
{
    if (m_activePoint >= 0 && m_activePoint < m_handleModes.size()) {
        return m_handleModes.at(m_activePoint);
    }
    return m_newPointHandleMode;
}

void CurveWidget::setSelectedPoint(int index)
{
    m_activePoint = (index >= 0 && index < m_curve.size()) ? index : -1;
    if (m_activePoint >= 0) {
        m_dragStartPoint = m_curve.at(m_activePoint);
    }
    emit selectedPointChanged(m_activePoint, selectedHandleMode());
    update();
}

int CurveWidget::selectedPoint() const
{
    return m_activePoint;
}

void CurveWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* autoHandle = menu.addAction(tr("Auto Handle"));
    QAction* vectorHandle = menu.addAction(tr("Vector Handle"));
    menu.addSeparator();
    QAction* copy = menu.addAction(tr("Copy Curve"));
    QAction* paste = menu.addAction(tr("Paste Curve"));
    QAction* reset = menu.addAction(tr("Reset Curve"));
    QAction* selected = menu.exec(event->globalPos());
    if (!selected) {
        return;
    }
    if (selected == autoHandle) {
        setSelectedHandleMode(CurveHandleMode::Auto);
        return;
    }
    if (selected == vectorHandle) {
        setSelectedHandleMode(CurveHandleMode::Vector);
        return;
    }
    if (selected == copy) {
        QJsonArray arr;
        for (const QPointF& point : m_curve) {
            arr.append(QJsonArray{point.x(), point.y()});
        }
        QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
        return;
    }
    if (selected == paste) {
        const QJsonDocument doc = QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8());
        if (!doc.isArray()) {
            return;
        }
        QVector<QPointF> parsed;
        for (const QJsonValue& item : doc.array()) {
            const QJsonArray point = item.toArray();
            if (point.size() == 2) {
                parsed.append(QPointF(point.at(0).toDouble(), point.at(1).toDouble()));
            }
        }
        if (parsed.size() >= 2) {
            m_curve = parsed;
            normalize();
            emit curveChanged(m_curve);
            update();
        }
        return;
    }
    if (selected == reset) {
        resetCurve();
    }
}

void CurveWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(14, 15, 18));

    const QRectF g = graphRect();
    painter.setPen(QColor(52, 56, 64));
    painter.drawRect(g);
    for (int i = 1; i < 8; ++i) {
        const double x = g.left() + g.width() * i / 8.0;
        const double y = g.top() + g.height() * i / 8.0;
        painter.setPen(i % 2 == 0 ? QColor(46, 50, 58) : QColor(32, 35, 41));
        painter.drawLine(QPointF(x, g.top()), QPointF(x, g.bottom()));
        painter.drawLine(QPointF(g.left(), y), QPointF(g.right(), y));
    }

    if (!m_histogram.isEmpty()) {
        QPainterPath histogramPath;
        histogramPath.moveTo(g.left(), g.bottom());
        const int denominator = qMax(1, static_cast<int>(m_histogram.size()) - 1);
        for (int i = 0; i < m_histogram.size(); ++i) {
            const double x = g.left() + g.width() * i / denominator;
            const double y = g.bottom() - g.height() * std::clamp(static_cast<double>(m_histogram.at(i)), 0.0, 1.0);
            histogramPath.lineTo(QPointF(x, y));
        }
        histogramPath.lineTo(g.right(), g.bottom());
        histogramPath.closeSubpath();
        QColor fill = m_channelColor;
        fill.setAlpha(44);
        painter.fillPath(histogramPath, fill);
    }

    painter.setPen(QPen(QColor(82, 86, 94), 1, Qt::DashLine));
    if (m_neutralLineY >= 0.0) {
        const double y = std::clamp(m_neutralLineY, 0.0, 1.0);
        painter.drawLine(toWidget(QPointF(0, y)), toWidget(QPointF(1, y)));
    } else {
        painter.drawLine(toWidget(QPointF(0, 0)), toWidget(QPointF(1, 1)));
    }

    QPainterPath path;
    if (!m_curve.isEmpty()) {
        path.moveTo(toWidget(m_curve.first()));
        const int samples = std::max(16, width() / 2);
        for (int i = 1; i <= samples; ++i) {
            const float x = static_cast<float>(i) / static_cast<float>(samples);
            path.lineTo(toWidget(QPointF(x, evaluate(x))));
        }
    }
    painter.setPen(QPen(m_channelColor, 2.4));
    painter.drawPath(path);

    for (int i = 0; i < m_curve.size(); ++i) {
        const QPoint p = toWidget(m_curve.at(i));
        const bool autoHandle = i < m_handleModes.size() && m_handleModes.at(i) == CurveHandleMode::Auto;
        painter.setBrush(i == m_activePoint ? QColor(255, 205, 120) : QColor(232, 236, 242));
        painter.setPen(QColor(18, 20, 24));
        if (autoHandle) {
            painter.drawEllipse(p, 5, 5);
        } else {
            painter.drawRect(QRect(p.x() - 5, p.y() - 5, 10, 10));
        }
    }
}

void CurveWidget::mousePressEvent(QMouseEvent* event)
{
    m_activePoint = nearestPoint(event->pos());
    if (m_activePoint < 0 && event->button() == Qt::LeftButton) {
        m_curve.append(toGraph(event->pos()));
        m_handleModes.append(m_newPointHandleMode);
        normalize();
        m_activePoint = nearestPoint(event->pos());
        emit curveChanged(m_curve);
        emit handleModesChanged(m_handleModes);
    }
    emit selectedPointChanged(m_activePoint, selectedHandleMode());
    if (m_activePoint >= 0 && m_activePoint < m_curve.size()) {
        m_dragStartPoint = m_curve.at(m_activePoint);
    }
    update();
}

void CurveWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_activePoint < 0 || !(event->buttons() & Qt::LeftButton)) {
        return;
    }
    QPointF point = toGraph(event->pos());
    const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        const QPointF delta = point - m_dragStartPoint;
        if (std::abs(delta.x()) >= std::abs(delta.y())) {
            point.setY(m_dragStartPoint.y());
        } else {
            point.setX(m_dragStartPoint.x());
        }
    }
    if (modifiers.testFlag(Qt::ControlModifier)) {
        point.setX(std::round(point.x() * 16.0) / 16.0);
        point.setY(std::round(point.y() * 16.0) / 16.0);
    }
    if (m_activePoint == 0) {
        point.setX(0.0);
    }
    if (m_activePoint == m_curve.size() - 1) {
        point.setX(1.0);
    }
    m_curve[m_activePoint] = point;
    normalize();
    m_activePoint = nearestPoint(toWidget(point));
    emit curveChanged(m_curve);
    emit handleModesChanged(m_handleModes);
    emit selectedPointChanged(m_activePoint, selectedHandleMode());
    update();
}

void CurveWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int idx = nearestPoint(event->pos());
    if (idx > 0 && idx < m_curve.size() - 1) {
        m_curve.removeAt(idx);
        if (idx < m_handleModes.size()) {
            m_handleModes.removeAt(idx);
        }
    } else {
        resetCurve();
        return;
    }
    m_activePoint = -1;
    emit curveChanged(m_curve);
    emit handleModesChanged(m_handleModes);
    emit selectedPointChanged(m_activePoint, selectedHandleMode());
    update();
}

void CurveWidget::resetCurve()
{
    m_curve = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
    m_handleModes = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    m_activePoint = -1;
    emit curveChanged(m_curve);
    emit handleModesChanged(m_handleModes);
    emit selectedPointChanged(m_activePoint, selectedHandleMode());
    update();
}

QRectF CurveWidget::graphRect() const
{
    return rect().adjusted(14, 14, -14, -14);
}

QPointF CurveWidget::toGraph(const QPoint& pos) const
{
    const QRectF g = graphRect();
    const double x = std::clamp((pos.x() - g.left()) / g.width(), 0.0, 1.0);
    const double y = std::clamp(1.0 - (pos.y() - g.top()) / g.height(), 0.0, 1.0);
    return QPointF(x, y);
}

QPoint CurveWidget::toWidget(const QPointF& point) const
{
    const QRectF g = graphRect();
    return QPoint(static_cast<int>(g.left() + point.x() * g.width()),
                  static_cast<int>(g.bottom() - point.y() * g.height()));
}

int CurveWidget::nearestPoint(const QPoint& pos) const
{
    int best = -1;
    double bestDistance = 12.0;
    for (int i = 0; i < m_curve.size(); ++i) {
        const double distance = QLineF(pos, toWidget(m_curve.at(i))).length();
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

float CurveWidget::evaluate(float x) const
{
    const float clampedX = std::clamp(x, 0.0f, 1.0f);
    if (m_curve.size() < 2) {
        return clampedX;
    }
    for (int i = 1; i < m_curve.size(); ++i) {
        if (clampedX <= m_curve.at(i).x()) {
            return evaluateSegment(i, clampedX);
        }
    }
    return static_cast<float>(m_curve.last().y());
}

float CurveWidget::evaluateSegment(int index, float x) const
{
    const QPointF p1 = m_curve.at(index - 1);
    const QPointF p2 = m_curve.at(index);
    const bool vectorSegment = (index - 1 < m_handleModes.size() && m_handleModes.at(index - 1) == CurveHandleMode::Vector)
        && (index < m_handleModes.size() && m_handleModes.at(index) == CurveHandleMode::Vector);
    if (vectorSegment) {
        const float denom = std::max(0.0001f, static_cast<float>(p2.x() - p1.x()));
        const float t = std::clamp((x - static_cast<float>(p1.x())) / denom, 0.0f, 1.0f);
        return std::clamp(static_cast<float>(p1.y()) * (1.0f - t) + static_cast<float>(p2.y()) * t, 0.0f, 1.0f);
    }

    const QPointF h1 = bezierHandle(index - 1, true);
    const QPointF h2 = bezierHandle(index, false);
    auto cubic = [](double a, double b, double c, double d, double t) {
        const double it = 1.0 - t;
        return it * it * it * a + 3.0 * it * it * t * b + 3.0 * it * t * t * c + t * t * t * d;
    };

    double low = 0.0;
    double high = 1.0;
    const double targetX = std::clamp(static_cast<double>(x), p1.x(), p2.x());
    for (int i = 0; i < 18; ++i) {
        const double mid = (low + high) * 0.5;
        const double bx = cubic(p1.x(), h1.x(), h2.x(), p2.x(), mid);
        if (bx < targetX) {
            low = mid;
        } else {
            high = mid;
        }
    }
    const double t = (low + high) * 0.5;
    return std::clamp(static_cast<float>(cubic(p1.y(), h1.y(), h2.y(), p2.y(), t)), 0.0f, 1.0f);
}

QPointF CurveWidget::bezierHandle(int pointIndex, bool rightHandle) const
{
    if (pointIndex < 0 || pointIndex >= m_curve.size()) {
        return QPointF();
    }

    const QPointF point = m_curve.at(pointIndex);
    const int neighborIndex = rightHandle ? pointIndex + 1 : pointIndex - 1;
    if (neighborIndex < 0 || neighborIndex >= m_curve.size()) {
        return point;
    }

    const QPointF neighbor = m_curve.at(neighborIndex);
    const bool autoHandle = pointIndex < m_handleModes.size() && m_handleModes.at(pointIndex) == CurveHandleMode::Auto;
    if (!autoHandle || pointIndex == 0 || pointIndex == m_curve.size() - 1) {
        return point + (neighbor - point) / 3.0;
    }

    const QPointF previous = m_curve.at(pointIndex - 1);
    const QPointF next = m_curve.at(pointIndex + 1);
    const QPointF tangent = next - previous;
    const double tangentLength = std::hypot(tangent.x(), tangent.y());
    if (tangentLength <= 0.000001) {
        return point + (neighbor - point) / 3.0;
    }

    const double neighborDistance = std::hypot(neighbor.x() - point.x(), neighbor.y() - point.y());
    const double handleLength = neighborDistance / 3.0;
    const QPointF direction(tangent.x() / tangentLength, tangent.y() / tangentLength);
    QPointF handle = rightHandle ? point + direction * handleLength : point - direction * handleLength;
    handle.setX(std::clamp(handle.x(), std::min(point.x(), neighbor.x()), std::max(point.x(), neighbor.x())));
    handle.setY(std::clamp(handle.y(), 0.0, 1.0));
    return handle;
}

void CurveWidget::ensureHandleModes()
{
    while (m_handleModes.size() < m_curve.size()) {
        m_handleModes.append(CurveHandleMode::Vector);
    }
    while (m_handleModes.size() > m_curve.size()) {
        m_handleModes.removeLast();
    }
}

void CurveWidget::normalize()
{
    QVector<QPair<QPointF, CurveHandleMode>> points;
    points.reserve(m_curve.size());
    ensureHandleModes();
    for (int i = 0; i < m_curve.size(); ++i) {
        QPointF point = m_curve.at(i);
        point.setX(std::clamp(point.x(), 0.0, 1.0));
        point.setY(std::clamp(point.y(), 0.0, 1.0));
        points.append(qMakePair(point, m_handleModes.at(i)));
    }
    std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) {
        return a.first.x() < b.first.x();
    });
    m_curve.clear();
    m_handleModes.clear();
    for (const auto& item : points) {
        m_curve.append(item.first);
        m_handleModes.append(item.second);
    }
    if (m_curve.size() < 2) {
        m_curve = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
        m_handleModes = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    }
    m_curve.first().setX(0.0);
    m_curve.last().setX(1.0);
    ensureHandleModes();
}

} // namespace ikclut
