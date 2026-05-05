#include "ui/ColorWheelControl.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

namespace ikclut {

namespace {

QVector3D hueVector(float hue)
{
    const QColor c = QColor::fromHsvF(std::fmod(hue + 1.0f, 1.0f), 1.0, 1.0);
    return QVector3D(c.redF(), c.greenF(), c.blueF()) - QVector3D(0.5f, 0.5f, 0.5f);
}

} // namespace

ColorWheelControl::ColorWheelControl(const QString& title,
                                     float baseValue,
                                     float minimumValue,
                                     float maximumValue,
                                     float maximumDelta,
                                     QWidget* parent)
    : QWidget(parent)
    , m_title(title)
    , m_baseValue(baseValue)
    , m_minimumValue(minimumValue)
    , m_maximumValue(maximumValue)
    , m_maximumDelta(maximumDelta)
{
    setMinimumSize(160, 150);
    setMouseTracking(true);
    m_value = QVector3D(baseValue, baseValue, baseValue);
    handleFromVector(m_value);
}

void ColorWheelControl::setValue(const QVector3D& value)
{
    m_value = value;
    handleFromVector(m_value);
    update();
}

QVector3D ColorWheelControl::value() const
{
    return m_value;
}

void ColorWheelControl::paintEvent(QPaintEvent*)
{
    if (m_cache.size() != size()) {
        rebuildCache(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), palette().window());
    painter.drawImage(rect(), m_cache);

    const QRectF wheel = wheelRect();
    const QPointF center = wheel.center();
    const QPointF handle = center + m_handle * (wheel.width() * 0.5 - 9.0);

    painter.setPen(QPen(QColor(54, 58, 66), 1.2));
    painter.drawEllipse(wheel);
    painter.setPen(QPen(QColor(210, 214, 220, 150), 1.0));
    painter.drawLine(QPointF(wheel.left(), center.y()), QPointF(wheel.right(), center.y()));
    painter.drawLine(QPointF(center.x(), wheel.top()), QPointF(center.x(), wheel.bottom()));

    painter.setPen(QPen(QColor(245, 246, 248), 2.0));
    painter.setBrush(QColor(208, 142, 54));
    painter.drawEllipse(handle, 6.5, 6.5);

    painter.setPen(QColor(232, 235, 240));
    painter.drawText(QRectF(8, 7, width() - 16, 18), Qt::AlignLeft | Qt::AlignVCenter, m_title);
}

void ColorWheelControl::mousePressEvent(QMouseEvent* event)
{
    updateFromPoint(event->position(), true);
}

void ColorWheelControl::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        updateFromPoint(event->position(), true);
    }
}

void ColorWheelControl::wheelEvent(QWheelEvent* event)
{
    event->ignore();
}

void ColorWheelControl::updateFromPoint(const QPointF& point, bool emitChange)
{
    const QRectF wheel = wheelRect();
    const QPointF center = wheel.center();
    QPointF delta = point - center;
    const qreal radius = wheel.width() * 0.5;
    const qreal length = std::hypot(delta.x(), delta.y());
    if (length > radius) {
        delta *= radius / std::max<qreal>(0.001, length);
    }

    const float nx = static_cast<float>(delta.x() / radius);
    const float ny = static_cast<float>(delta.y() / radius);
    const float amount = std::min(1.0f, static_cast<float>(std::hypot(nx, ny)));
    const float angle = std::atan2(-ny, nx);
    const float hue = std::fmod(angle / (2.0f * static_cast<float>(M_PI)) + 1.0f, 1.0f);

    m_handle = QPointF(nx, ny);
    m_value = QVector3D(m_baseValue, m_baseValue, m_baseValue) + hueVector(hue) * (m_maximumDelta * amount);
    m_value.setX(std::clamp(m_value.x(), m_minimumValue, m_maximumValue));
    m_value.setY(std::clamp(m_value.y(), m_minimumValue, m_maximumValue));
    m_value.setZ(std::clamp(m_value.z(), m_minimumValue, m_maximumValue));
    update();

    if (emitChange) {
        emit valueChanged(m_value);
    }
}

void ColorWheelControl::handleFromVector(const QVector3D& value)
{
    const QVector3D delta = value - QVector3D(m_baseValue, m_baseValue, m_baseValue);
    const float magnitude = std::max({std::abs(delta.x()), std::abs(delta.y()), std::abs(delta.z())});
    if (magnitude <= 0.0001f) {
        m_handle = QPointF(0.0, 0.0);
        return;
    }

    const QVector3D normalized = delta.normalized();
    float bestHue = 0.0f;
    float bestDot = -999.0f;
    for (int i = 0; i < 96; ++i) {
        const float hue = static_cast<float>(i) / 96.0f;
        const float dot = QVector3D::dotProduct(normalized, hueVector(hue).normalized());
        if (dot > bestDot) {
            bestDot = dot;
            bestHue = hue;
        }
    }

    const float amount = std::min(1.0f, magnitude / std::max(0.0001f, m_maximumDelta * 0.5f));
    const float angle = bestHue * 2.0f * static_cast<float>(M_PI);
    m_handle = QPointF(std::cos(angle) * amount, -std::sin(angle) * amount);
}

QRectF ColorWheelControl::wheelRect() const
{
    const qreal top = 28.0;
    const qreal bottom = 10.0;
    const qreal availableWidth = std::max<qreal>(32.0, width() - 24.0);
    const qreal availableHeight = std::max<qreal>(32.0, height() - top - bottom);
    const qreal side = std::min(availableWidth, availableHeight);
    const qreal left = (width() - side) * 0.5;
    return QRectF(left, top, side, side).adjusted(1, 1, -1, -1);
}

void ColorWheelControl::rebuildCache(const QSize& size)
{
    m_cache = QImage(size, QImage::Format_ARGB32_Premultiplied);
    m_cache.fill(Qt::transparent);

    const QRectF wheel = wheelRect();
    const QPointF center = wheel.center();
    const float radius = static_cast<float>(wheel.width() * 0.5);
    const QRect bounds = wheel.toAlignedRect().adjusted(-1, -1, 1, 1).intersected(m_cache.rect());

    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(m_cache.scanLine(y));
        for (int x = bounds.left(); x <= bounds.right(); ++x) {
            const float dx = static_cast<float>(x + 0.5 - center.x());
            const float dy = static_cast<float>(y + 0.5 - center.y());
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance > radius) {
                continue;
            }

            const float amount = std::clamp(distance / std::max(1.0f, radius), 0.0f, 1.0f);
            const float angle = std::atan2(-dy, dx);
            const float hue = std::fmod(angle / (2.0f * static_cast<float>(M_PI)) + 1.0f, 1.0f);
            const QColor color = QColor::fromHsvF(hue, amount, 0.24f + amount * 0.76f);
            line[x] = qRgba(color.red(), color.green(), color.blue(), 255);
        }
    }

    QPainter painter(&m_cache);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(18, 20, 24), 7.0));
    painter.drawEllipse(wheel.adjusted(3, 3, -3, -3));
}

} // namespace ikclut
