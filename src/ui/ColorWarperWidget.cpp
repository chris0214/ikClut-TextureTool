#include "ui/ColorWarperWidget.h"

#include <QContextMenuEvent>
#include <QLineF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ikclut {

namespace {

QPointF normalizedToWidgetPoint(const QRectF& field, const QPointF& point)
{
    return QPointF(field.left() + point.x() * field.width(),
                   field.bottom() - point.y() * field.height());
}

QPointF resolveChromaCenter()
{
    return QPointF(0.42, 0.52);
}

const QVector<QPointF>& resolveChromaPolygon()
{
    static const QVector<QPointF> polygon = {
        {0.10, 0.08}, {0.07, 0.20}, {0.05, 0.38}, {0.05, 0.58},
        {0.08, 0.76}, {0.16, 0.90}, {0.28, 0.98}, {0.43, 0.94},
        {0.57, 0.86}, {0.70, 0.75}, {0.82, 0.61}, {0.90, 0.49},
        {0.93, 0.40}, {0.88, 0.32}, {0.76, 0.23}, {0.60, 0.16},
        {0.42, 0.11}, {0.24, 0.08}
    };
    return polygon;
}

QPainterPath resolveChromaPath(const QRectF& field)
{
    const QVector<QPointF>& polygon = resolveChromaPolygon();
    QPainterPath path;
    path.moveTo(normalizedToWidgetPoint(field, polygon.first()));
    for (int i = 0; i < polygon.size(); ++i) {
        const QPointF current = normalizedToWidgetPoint(field, polygon.at(i));
        const QPointF next = normalizedToWidgetPoint(field, polygon.at((i + 1) % polygon.size()));
        const QPointF after = normalizedToWidgetPoint(field, polygon.at((i + 2) % polygon.size()));
        const QPointF c1 = current + (next - normalizedToWidgetPoint(field, polygon.at((i - 1 + polygon.size()) % polygon.size()))) / 6.0;
        const QPointF c2 = next - (after - current) / 6.0;
        path.cubicTo(c1, c2, next);
    }
    path.closeSubpath();
    return path;
}

bool pointInNormalizedPolygon(const QPointF& p, const QVector<QPointF>& polygon)
{
    bool inside = false;
    int j = polygon.size() - 1;
    for (int i = 0; i < polygon.size(); ++i) {
        const QPointF a = polygon.at(i);
        const QPointF b = polygon.at(j);
        if (((a.y() > p.y()) != (b.y() > p.y()))
            && (p.x() < (b.x() - a.x()) * (p.y() - a.y()) / std::max(0.000001, b.y() - a.y()) + a.x())) {
            inside = !inside;
        }
        j = i;
    }
    return inside;
}

double distanceToSegment(const QPointF& point, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const double denom = ab.x() * ab.x() + ab.y() * ab.y();
    if (denom <= 0.000001) {
        return QLineF(point, a).length();
    }
    const double t = std::clamp(((point.x() - a.x()) * ab.x() + (point.y() - a.y()) * ab.y()) / denom, 0.0, 1.0);
    const QPointF projected(a.x() + ab.x() * t, a.y() + ab.y() * t);
    return QLineF(point, projected).length();
}

QColor resolveChromaColor(qreal x, qreal y)
{
    const QPointF center = resolveChromaCenter();
    const qreal dx = x - center.x();
    const qreal dy = y - center.y();
    const qreal angle = std::atan2(-dy, dx);
    qreal hue = std::fmod(0.60 - angle / (2.0 * M_PI) + 1.0, 1.0);
    const qreal radius = std::sqrt(dx * dx + dy * dy);
    const qreal saturation = std::clamp(radius * 2.15, 0.0, 1.0);
    const qreal neutral = std::exp(-radius * radius * 28.0);
    QColor c = QColor::fromHsvF(hue, saturation, 0.86);
    const qreal gray = 0.48;
    return QColor::fromRgbF(c.redF() * (1.0 - neutral) + gray * neutral,
                            c.greenF() * (1.0 - neutral) + gray * neutral,
                            c.blueF() * (1.0 - neutral) + gray * neutral);
}

} // namespace

ColorWarperWidget::ColorWarperWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(300);
    setMouseTracking(true);
    resetWarp();
}

void ColorWarperWidget::resetWarp()
{
    pushUndoState();
    m_sources.clear();
    m_points.clear();
    m_pinned.clear();
    m_selectedPoint = -1;
    m_dragPoint = -1;
    emitWarpChanged();
    emitSelectionChanged();
    update();
}

QVector<QPointF> ColorWarperWidget::points() const
{
    return m_points;
}

QVector<QPointF> ColorWarperWidget::sources() const
{
    return m_sources;
}

QVector<bool> ColorWarperWidget::pinned() const
{
    return m_pinned;
}

void ColorWarperWidget::setPoints(const QVector<QPointF>& points)
{
    setWarpPoints(points, points);
}

void ColorWarperWidget::setWarpPoints(const QVector<QPointF>& sources, const QVector<QPointF>& targets)
{
    setWarpPoints(sources, targets, QVector<bool>(targets.size(), false));
}

void ColorWarperWidget::setWarpPoints(const QVector<QPointF>& sources, const QVector<QPointF>& targets, const QVector<bool>& pinned)
{
    m_sources = sources;
    m_points = targets;
    m_pinned = pinned;
    if (m_sources.size() != m_points.size()) {
        m_sources = m_points;
    }
    while (m_pinned.size() < m_points.size()) {
        m_pinned.append(false);
    }
    while (m_pinned.size() > m_points.size()) {
        m_pinned.removeLast();
    }
    bool removedInvalidPoints = false;
    for (int i = m_points.size() - 1; i >= 0; --i) {
        const QPointF source = i < m_sources.size() ? m_sources.at(i) : m_points.at(i);
        if (!isInGamut(source) || !isInGamut(m_points.at(i))) {
            m_points.removeAt(i);
            if (i < m_sources.size()) {
                m_sources.removeAt(i);
            }
            if (i < m_pinned.size()) {
                m_pinned.removeAt(i);
            }
            removedInvalidPoints = true;
        }
    }
    m_selectedPoint = (m_selectedPoint >= 0 && m_selectedPoint < m_points.size()) ? m_selectedPoint : -1;
    emitSelectionChanged();
    if (removedInvalidPoints) {
        emitWarpChanged();
    }
    update();
}

void ColorWarperWidget::setSnapEnabled(bool enabled)
{
    m_snapEnabled = enabled;
}

bool ColorWarperWidget::snapEnabled() const
{
    return m_snapEnabled;
}

void ColorWarperWidget::setPinCreationMode(bool enabled)
{
    m_pinCreationMode = enabled;
}

bool ColorWarperWidget::pinCreationMode() const
{
    return m_pinCreationMode;
}

void ColorWarperWidget::addWarpPoint(const QPointF& source, bool pinned)
{
    const QPointF fittedSource = fitPointToGamut(source);
    pushUndoState();
    m_sources.append(fittedSource);
    m_points.append(fittedSource);
    m_pinned.append(pinned);
    m_selectedPoint = m_points.size() - 1;
    m_dragPoint = -1;
    m_dragHandle = WarperDragHandle::None;
    emitWarpChanged();
    emitSelectionChanged();
    update();
}

void ColorWarperWidget::deleteSelectedPoint()
{
    if (m_selectedPoint >= 0 && m_selectedPoint < m_points.size()) {
        pushUndoState();
        m_points.removeAt(m_selectedPoint);
        if (m_selectedPoint < m_sources.size()) {
            m_sources.removeAt(m_selectedPoint);
        }
        if (m_selectedPoint < m_pinned.size()) {
            m_pinned.removeAt(m_selectedPoint);
        }
        m_selectedPoint = -1;
        m_dragPoint = -1;
        emitWarpChanged();
        emitSelectionChanged();
        update();
    }
}

void ColorWarperWidget::setSelectedPoint(int index)
{
    m_selectedPoint = (index >= 0 && index < m_points.size()) ? index : -1;
    emitSelectionChanged();
    update();
}

void ColorWarperWidget::setSelectedPointPosition(const QPointF& point)
{
    if (m_selectedPoint < 0 || m_selectedPoint >= m_points.size()) {
        return;
    }
    const QPointF candidate(std::clamp(point.x(), 0.0, 1.0),
                            std::clamp(point.y(), 0.0, 1.0));
    if (!isInGamut(candidate)) {
        emitSelectionChanged();
        update();
        return;
    }
    pushUndoState();
    m_points[m_selectedPoint] = candidate;
    if (isPinned(m_selectedPoint) && m_selectedPoint < m_sources.size()) {
        m_sources[m_selectedPoint] = candidate;
    }
    emitWarpChanged();
    emitSelectionChanged();
    update();
}

void ColorWarperWidget::setSelectedSourcePosition(const QPointF& point)
{
    if (m_selectedPoint < 0 || m_selectedPoint >= m_sources.size()) {
        return;
    }
    const QPointF candidate(std::clamp(point.x(), 0.0, 1.0),
                            std::clamp(point.y(), 0.0, 1.0));
    if (!isInGamut(candidate)) {
        emitSelectionChanged();
        update();
        return;
    }
    pushUndoState();
    m_sources[m_selectedPoint] = candidate;
    if (isPinned(m_selectedPoint) && m_selectedPoint < m_points.size()) {
        m_points[m_selectedPoint] = candidate;
    }
    emitWarpChanged();
    emitSelectionChanged();
    update();
}

void ColorWarperWidget::setSelectedPinned(bool pinned)
{
    if (m_selectedPoint < 0 || m_selectedPoint >= m_points.size()) {
        return;
    }
    if (m_selectedPoint >= m_pinned.size()) {
        m_pinned.resize(m_points.size());
    }
    if (m_pinned.at(m_selectedPoint) == pinned) {
        return;
    }

    pushUndoState();
    m_pinned[m_selectedPoint] = pinned;
    if (pinned && m_selectedPoint < m_sources.size()) {
        m_points[m_selectedPoint] = m_sources.at(m_selectedPoint);
    }
    emitWarpChanged();
    emitSelectionChanged();
    update();
}

void ColorWarperWidget::undo()
{
    if (m_undoStack.isEmpty()) {
        return;
    }
    m_redoStack.append(currentState());
    restoreState(m_undoStack.takeLast());
}

void ColorWarperWidget::redo()
{
    if (m_redoStack.isEmpty()) {
        return;
    }
    m_undoStack.append(currentState());
    restoreState(m_redoStack.takeLast());
}

bool ColorWarperWidget::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool ColorWarperWidget::canRedo() const
{
    return !m_redoStack.isEmpty();
}

int ColorWarperWidget::selectedPoint() const
{
    return m_selectedPoint;
}

bool ColorWarperWidget::selectedPinned() const
{
    return isPinned(m_selectedPoint);
}

void ColorWarperWidget::contextMenuEvent(QContextMenuEvent* event)
{
    m_selectedPoint = nearestPoint(event->pos());
    emitSelectionChanged();
    update();
    QMenu menu(this);
    QAction* reset = menu.addAction(tr("Reset Warp"));
    QAction* deletePoint = menu.addAction(tr("Delete Point"));
    QAction* snapAction = menu.addAction(m_snapEnabled ? tr("Disable Snap") : tr("Enable Snap"));
    QAction* selected = menu.exec(event->globalPos());
    if (selected == reset) {
        resetWarp();
    } else if (selected == deletePoint) {
        deleteSelectedPoint();
    } else if (selected == snapAction) {
        m_snapEnabled = !m_snapEnabled;
    }
}

void ColorWarperWidget::paintEvent(QPaintEvent*)
{
    if (m_cache.size() != size()) {
        rebuildCache(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawImage(rect(), m_cache);

    const QRectF field = fieldRect();

    const QPainterPath paintGamut = resolveChromaPath(field);

    painter.save();
    painter.setClipPath(paintGamut);
    painter.setPen(QPen(QColor(215, 219, 225, 65), 1.0));
    for (int i = 1; i < 5; ++i) {
        const qreal y = field.top() + field.height() * i / 5.0;
        painter.drawLine(QPointF(field.left(), y), QPointF(field.right(), y));
    }
    painter.restore();

    painter.setPen(QPen(QColor(160, 164, 172, 95), 1.0));
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF source = QPointF(toWidget(i < m_sources.size() ? m_sources.at(i) : m_points.at(i)));
        const QPointF target = QPointF(toWidget(m_points.at(i)));
        const bool selected = i == m_selectedPoint;
        if (isPinned(i)) {
            continue;
        }
        painter.setPen(QPen(selected ? QColor(255, 218, 126, 210) : QColor(170, 176, 186, 92),
                            selected ? 2.0 : 1.0));
        painter.drawLine(source, target);

        const QPointF delta = target - source;
        const qreal length = std::hypot(delta.x(), delta.y());
        if (length > 12.0) {
            const QPointF dir(delta.x() / length, delta.y() / length);
            const QPointF normal(-dir.y(), dir.x());
            const QPointF tip = target - dir * 8.0;
            QPolygonF arrow;
            arrow << tip
                  << tip - dir * 9.0 + normal * 4.5
                  << tip - dir * 9.0 - normal * 4.5;
            painter.setBrush(selected ? QColor(255, 218, 126, 210) : QColor(170, 176, 186, 100));
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(arrow);
        }

        painter.setBrush(QColor(18, 20, 24, 210));
        painter.setPen(QPen(selected ? QColor(255, 218, 126, 210) : QColor(225, 229, 235, 145),
                            selected ? 1.6 : 1.0));
        painter.drawEllipse(source, selected ? 5.0 : 3.8, selected ? 5.0 : 3.8);
    }

    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF p = QPointF(toWidget(m_points.at(i)));
        const bool selected = i == m_selectedPoint;
        if (isPinned(i)) {
            const qreal r = selected ? 8.0 : 6.4;
            QPolygonF diamond;
            diamond << QPointF(p.x(), p.y() - r)
                    << QPointF(p.x() + r, p.y())
                    << QPointF(p.x(), p.y() + r)
                    << QPointF(p.x() - r, p.y());
            painter.setPen(QPen(selected ? QColor(255, 238, 170) : QColor(125, 214, 255),
                                selected ? 2.0 : 1.4));
            painter.setBrush(selected ? QColor(255, 220, 140) : QColor(68, 176, 255));
            painter.drawPolygon(diamond);
            continue;
        }
        painter.setPen(QPen(selected ? QColor(255, 235, 168) : QColor(255, 155, 82),
                            selected ? 2.2 : 1.4));
        painter.setBrush(selected ? QColor(255, 220, 140) : pointColor(m_points.at(i)));
        painter.drawEllipse(p, selected ? 7.4 : 6.3, selected ? 7.4 : 6.3);
        if (i == m_selectedPoint) {
            painter.setPen(QPen(QColor(255, 220, 140, 150), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(p, 12.0, 12.0);
            painter.setPen(QPen(QColor(255, 155, 82), 2.0));
        }
    }

    if (m_selectedPoint >= 0 && m_selectedPoint < m_points.size()) {
        const QPointF selected = m_points.at(m_selectedPoint);
        painter.setPen(QColor(225, 229, 235));
        painter.drawText(QRectF(field.left(), field.bottom() + 8.0, field.width(), 20.0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString("Selected %1  H:%2  S:%3")
                             .arg(m_selectedPoint + 1)
                             .arg(selected.x(), 0, 'f', 3)
                             .arg(selected.y(), 0, 'f', 3));
    }
}

void ColorWarperWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_pressPos = event->pos();
    m_dragHandle = WarperDragHandle::None;
    m_selectedPoint = nearestPoint(event->pos(), &m_dragHandle);
    if (m_selectedPoint < 0) {
        const QPointF source = toField(event->pos());
        if (!isInGamut(source)) {
            emitSelectionChanged();
            update();
            return;
        }
        pushUndoState();
        m_sources.append(source);
        m_points.append(source);
        m_pinned.append(m_pinCreationMode);
        m_selectedPoint = m_points.size() - 1;
        m_dragHandle = m_pinCreationMode ? WarperDragHandle::Source : WarperDragHandle::Target;
        emitWarpChanged();
    }
    m_dragPoint = m_selectedPoint;
    m_dragUndoSaved = false;
    emitSelectionChanged();
    update();
}

void ColorWarperWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragPoint < 0 || !(event->buttons() & Qt::LeftButton)) {
        return;
    }
    if (QLineF(m_pressPos, event->pos()).length() < 3.0) {
        return;
    }
    QPointF fieldPoint = toField(event->pos());
    if (!isInGamut(fieldPoint)) {
        return;
    }
    if (m_snapEnabled) {
        fieldPoint.setX(std::round(fieldPoint.x() * 8.0) / 8.0);
        fieldPoint.setY(std::round(fieldPoint.y() * 8.0) / 8.0);
        if (!isInGamut(fieldPoint)) {
            return;
        }
    }
    if (!m_dragUndoSaved) {
        pushUndoState();
        m_dragUndoSaved = true;
    }
    if (isPinned(m_dragPoint)) {
        if (m_dragPoint < m_sources.size()) {
            m_sources[m_dragPoint] = fieldPoint;
        }
        if (m_dragPoint < m_points.size()) {
            m_points[m_dragPoint] = fieldPoint;
        }
    } else if (m_dragHandle == WarperDragHandle::Source) {
        if (m_dragPoint < m_sources.size()) {
            m_sources[m_dragPoint] = fieldPoint;
        }
    } else {
        m_points[m_dragPoint] = fieldPoint;
    }
    emitWarpChanged();
    emitSelectionChanged();
    update();
}

void ColorWarperWidget::mouseReleaseEvent(QMouseEvent*)
{
    m_dragPoint = -1;
    m_dragHandle = WarperDragHandle::None;
    m_dragUndoSaved = false;
}

void ColorWarperWidget::rebuildCache(const QSize& size)
{
    m_cache = QImage(size, QImage::Format_ARGB32_Premultiplied);
    m_cache.fill(QColor(20, 22, 26));
    QPainter painter(&m_cache);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF field = fieldRect();

    const QPainterPath gamut = resolveChromaPath(field);

    painter.save();
    painter.setClipPath(gamut);
    QImage spectrum(field.size().toSize().expandedTo(QSize(1, 1)), QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < spectrum.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(spectrum.scanLine(y));
        const qreal normalizedY = 1.0 - y / static_cast<qreal>(std::max(1, spectrum.height() - 1));
        for (int x = 0; x < spectrum.width(); ++x) {
            const qreal normalizedX = x / static_cast<qreal>(std::max(1, spectrum.width() - 1));
            line[x] = resolveChromaColor(normalizedX, normalizedY).rgba();
        }
    }
    painter.drawImage(field.topLeft(), spectrum);
    const QPointF centerPoint = normalizedToWidgetPoint(field, QPointF(0.42, 0.52));
    QRadialGradient centerGlow(centerPoint, field.width() * 0.24);
    centerGlow.setColorAt(0.0, QColor(245, 245, 245, 135));
    centerGlow.setColorAt(0.46, QColor(235, 235, 235, 28));
    centerGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillPath(gamut, centerGlow);
    painter.restore();

    painter.setPen(QPen(QColor(255, 255, 255, 70), 1.0));
    painter.drawPath(gamut);

    painter.save();
    painter.setClipPath(gamut);
    painter.setPen(QPen(QColor(0, 0, 0, 54), 1.0));
    for (qreal x = 0.125; x < 1.0; x += 0.125) {
        painter.drawLine(QPointF(field.left() + field.width() * x, field.top()),
                         QPointF(field.left() + field.width() * x, field.bottom()));
    }
    for (qreal y = 0.125; y < 1.0; y += 0.125) {
        painter.drawLine(QPointF(field.left(), field.top() + field.height() * y),
                         QPointF(field.right(), field.top() + field.height() * y));
    }

    painter.setPen(QPen(QColor(0, 0, 0, 72), 1.0));
    painter.drawLine(field.topLeft(), field.bottomRight());
    painter.drawLine(field.topRight(), field.bottomLeft());
    painter.drawLine(QPointF(field.center().x(), field.top()), QPointF(field.center().x(), field.bottom()));
    painter.drawLine(QPointF(field.left(), field.center().y()), QPointF(field.right(), field.center().y()));
    painter.restore();

    painter.setBrush(QColor(230, 234, 242, 110));
    painter.setPen(QPen(QColor(20, 22, 26, 170), 1.0));
    painter.drawEllipse(centerPoint, 4.5, 4.5);
}

QRectF ColorWarperWidget::fieldRect() const
{
    const QRectF available = rect().adjusted(24, 28, -24, -42);
    constexpr qreal desiredAspect = 1.55;
    qreal width = available.width();
    qreal height = width / desiredAspect;
    if (height > available.height()) {
        height = available.height();
        width = height * desiredAspect;
    }
    return QRectF(available.center().x() - width * 0.5,
                  available.center().y() - height * 0.5,
                  width,
                  height);
}

QPointF ColorWarperWidget::toField(const QPoint& pos) const
{
    const QRectF field = fieldRect();
    const qreal x = std::clamp((pos.x() - field.left()) / field.width(), 0.0, 1.0);
    const qreal y = std::clamp((field.bottom() - pos.y()) / field.height(), 0.0, 1.0);
    return QPointF(x, y);
}

QPoint ColorWarperWidget::toWidget(const QPointF& point) const
{
    const QPointF p = toWidgetF(point);
    return QPoint(static_cast<int>(p.x()), static_cast<int>(p.y()));
}

QPointF ColorWarperWidget::toWidgetF(const QPointF& point) const
{
    const QRectF field = fieldRect();
    return QPointF(field.left() + point.x() * field.width(),
                   field.bottom() - point.y() * field.height());
}

QPointF ColorWarperWidget::fitPointToGamut(const QPointF& point) const
{
    const QPointF clamped(std::clamp(point.x(), 0.0, 1.0),
                          std::clamp(point.y(), 0.0, 1.0));
    if (isInGamut(clamped)) {
        return clamped;
    }

    const QPointF center = resolveChromaCenter();
    for (int step = 1; step <= 48; ++step) {
        const double t = static_cast<double>(step) / 48.0;
        const QPointF reducedSat(clamped.x(),
                                 clamped.y() + (center.y() - clamped.y()) * t);
        if (isInGamut(reducedSat)) {
            return reducedSat;
        }
    }
    for (int step = 1; step <= 64; ++step) {
        const double t = static_cast<double>(step) / 64.0;
        const QPointF blended(clamped.x() + (center.x() - clamped.x()) * t,
                              clamped.y() + (center.y() - clamped.y()) * t);
        if (isInGamut(blended)) {
            return blended;
        }
    }
    return center;
}

bool ColorWarperWidget::isInGamut(const QPointF& point) const
{
    const QPointF clamped(std::clamp(point.x(), 0.0, 1.0),
                          std::clamp(point.y(), 0.0, 1.0));
    return resolveChromaPath(fieldRect()).contains(toWidgetF(clamped));
}

int ColorWarperWidget::nearestPoint(const QPoint& pos) const
{
    WarperDragHandle ignored = WarperDragHandle::None;
    return nearestPoint(pos, &ignored);
}

int ColorWarperWidget::nearestPoint(const QPoint& pos, WarperDragHandle* handle) const
{
    int best = -1;
    double bestDistance = 18.0;
    if (handle) {
        *handle = WarperDragHandle::None;
    }
    for (int i = 0; i < m_points.size(); ++i) {
        const double distance = QLineF(pos, toWidget(m_points.at(i))).length();
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
            if (handle) {
                *handle = WarperDragHandle::Target;
            }
        }
    }
    if (best >= 0) {
        return best;
    }

    bestDistance = 14.0;
    for (int i = 0; i < m_sources.size(); ++i) {
        const double distance = QLineF(pos, toWidget(m_sources.at(i))).length();
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
            if (handle) {
                *handle = WarperDragHandle::Source;
            }
        }
    }
    if (best >= 0) {
        return best;
    }

    bestDistance = 8.0;
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF source = QPointF(toWidget(i < m_sources.size() ? m_sources.at(i) : m_points.at(i)));
        const QPointF target = QPointF(toWidget(m_points.at(i)));
        const double distance = distanceToSegment(pos, source, target);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
            if (handle) {
                *handle = WarperDragHandle::Target;
            }
        }
    }
    return best;
}

QColor ColorWarperWidget::pointColor(const QPointF& point) const
{
    return QColor::fromHsvF(point.x(), std::clamp(point.y(), 0.0, 1.0), 1.0);
}

bool ColorWarperWidget::isPinned(int index) const
{
    return index >= 0 && index < m_pinned.size() && m_pinned.at(index);
}

void ColorWarperWidget::pushUndoState()
{
    m_undoStack.append(currentState());
    if (m_undoStack.size() > 80) {
        m_undoStack.removeFirst();
    }
    m_redoStack.clear();
    emit undoAvailabilityChanged(canUndo(), canRedo());
}

void ColorWarperWidget::restoreState(const WarperState& state)
{
    m_sources = state.sources;
    m_points = state.targets;
    m_pinned = state.pinned;
    m_selectedPoint = state.selectedPoint >= 0 && state.selectedPoint < m_points.size() ? state.selectedPoint : -1;
    m_dragPoint = -1;
    m_dragHandle = WarperDragHandle::None;
    emitWarpChanged();
    emitSelectionChanged();
    emit undoAvailabilityChanged(canUndo(), canRedo());
    update();
}

WarperState ColorWarperWidget::currentState() const
{
    WarperState state;
    state.sources = m_sources;
    state.targets = m_points;
    state.pinned = m_pinned;
    state.selectedPoint = m_selectedPoint;
    return state;
}

void ColorWarperWidget::emitWarpChanged()
{
    emit warpChanged(m_points);
    emit warpVectorsChanged(m_sources, m_points, m_pinned);
}

void ColorWarperWidget::emitSelectionChanged()
{
    if (m_selectedPoint >= 0 && m_selectedPoint < m_points.size()) {
        emit selectionChanged(m_selectedPoint, m_points.at(m_selectedPoint));
    } else {
        emit selectionChanged(-1, QPointF());
    }
}

} // namespace ikclut
