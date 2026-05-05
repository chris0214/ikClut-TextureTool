#include "ui/ImageView.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace ikclut {

namespace {

int warningLuma(int r, int g, int b)
{
    return std::clamp(static_cast<int>(r * 0.2126 + g * 0.7152 + b * 0.0722), 0, 255);
}

bool isWarningPixel(ImageWarningOverlay overlay, int r, int g, int b)
{
    if (overlay == ImageWarningOverlay::FalseColor) {
        return true;
    }
    const int lum = warningLuma(r, g, b);
    switch (overlay) {
    case ImageWarningOverlay::ShadowClip:
        return r <= 1 || g <= 1 || b <= 1 || lum <= 1;
    case ImageWarningOverlay::HighlightClip:
        return r >= 254 || g >= 254 || b >= 254 || lum >= 254;
    case ImageWarningOverlay::Gamut:
        return (std::max({r, g, b}) >= 250 && std::min({r, g, b}) <= 5)
            || std::abs(r - g) + std::abs(g - b) + std::abs(b - r) > 430;
    case ImageWarningOverlay::None:
    default:
        return false;
    }
}

QRgb warningColor(ImageWarningOverlay overlay)
{
    switch (overlay) {
    case ImageWarningOverlay::ShadowClip:
        return qRgba(54, 178, 255, 165);
    case ImageWarningOverlay::HighlightClip:
        return qRgba(255, 86, 64, 165);
    case ImageWarningOverlay::Gamut:
        return qRgba(244, 82, 255, 165);
    case ImageWarningOverlay::FalseColor:
        return qRgba(255, 255, 255, 155);
    case ImageWarningOverlay::None:
    default:
        return qRgba(0, 0, 0, 0);
    }
}

QRgb falseColor(int r, int g, int b)
{
    const int lum = warningLuma(r, g, b);
    if (lum < 16) return qRgba(33, 47, 155, 175);
    if (lum < 48) return qRgba(30, 124, 214, 165);
    if (lum < 88) return qRgba(28, 170, 132, 160);
    if (lum < 132) return qRgba(96, 194, 87, 150);
    if (lum < 176) return qRgba(238, 205, 78, 155);
    if (lum < 220) return qRgba(239, 122, 46, 165);
    return qRgba(232, 48, 60, 180);
}

QString warningLabel(ImageWarningOverlay overlay)
{
    switch (overlay) {
    case ImageWarningOverlay::ShadowClip:
        return QCoreApplication::translate("ikclut::ImageView", "Shadow Clip Overlay");
    case ImageWarningOverlay::HighlightClip:
        return QCoreApplication::translate("ikclut::ImageView", "Highlight Clip Overlay");
    case ImageWarningOverlay::Gamut:
        return QCoreApplication::translate("ikclut::ImageView", "Gamut Warning Overlay");
    case ImageWarningOverlay::FalseColor:
        return QCoreApplication::translate("ikclut::ImageView", "False Color / Exposure Zones");
    case ImageWarningOverlay::None:
    default:
        return QString();
    }
}

} // namespace

ImageView::ImageView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(280, 220);
    setMouseTracking(true);
}

void ImageView::setImage(const QImage& image)
{
    m_image = image;
    update();
}

void ImageView::setLabel(const QString& label)
{
    m_label = label;
    update();
}

void ImageView::setOverlayInfo(const QString& info)
{
    m_overlayInfo = info;
    update();
}

void ImageView::setWarningOverlay(ImageWarningOverlay overlay)
{
    m_warningOverlay = overlay;
    update();
}

void ImageView::setMaskOverlay(const QImage& overlay)
{
    m_maskOverlay = overlay;
    update();
}

void ImageView::setMaskGuide(MaskKind kind, const QJsonObject& params, bool visible)
{
    m_maskGuideKind = kind;
    m_maskGuideParams = params;
    m_maskGuideVisible = visible;
    update();
}

void ImageView::setMaskEditMode(bool enabled, const QString& hint)
{
    m_maskEditMode = enabled;
    m_maskEditHint = hint;
    setCursor((m_pickMode || m_maskEditMode) ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void ImageView::setPickMode(bool enabled, const QString& hint)
{
    m_pickMode = enabled;
    m_pickHint = hint;
    setCursor((m_pickMode || m_maskEditMode) ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

bool ImageView::pickMode() const
{
    return m_pickMode;
}

QSize ImageView::sizeHint() const
{
    return QSize(520, 420);
}

void ImageView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(18, 19, 22));

    painter.setPen(QColor(42, 45, 50));
    for (int y = 0; y < height(); y += 24) {
        painter.drawLine(0, y, width(), y);
    }
    for (int x = 0; x < width(); x += 24) {
        painter.drawLine(x, 0, x, height());
    }

    const QRectF drawnImageRect = imageRect();
    if (!m_image.isNull()) {
        painter.drawImage(drawnImageRect, m_image);
        if (m_warningOverlay != ImageWarningOverlay::None) {
            QImage src = m_image.convertToFormat(QImage::Format_ARGB32);
            QImage warning(src.size(), QImage::Format_ARGB32_Premultiplied);
            warning.fill(Qt::transparent);
            const QRgb color = warningColor(m_warningOverlay);
            for (int y = 0; y < src.height(); ++y) {
                const QRgb* sourceLine = reinterpret_cast<const QRgb*>(src.constScanLine(y));
                QRgb* warningLine = reinterpret_cast<QRgb*>(warning.scanLine(y));
                for (int x = 0; x < src.width(); ++x) {
                    const QRgb p = sourceLine[x];
                    if (isWarningPixel(m_warningOverlay, qRed(p), qGreen(p), qBlue(p)) && ((x + y) % 6 < 4)) {
                        warningLine[x] = m_warningOverlay == ImageWarningOverlay::FalseColor
                            ? falseColor(qRed(p), qGreen(p), qBlue(p))
                            : color;
                    }
                }
            }
            painter.drawImage(drawnImageRect, warning);
        }
        if (!m_maskOverlay.isNull()) {
            painter.drawImage(drawnImageRect, m_maskOverlay);
        }
        if (m_maskGuideVisible && (m_maskGuideKind == MaskKind::Radial || m_maskGuideKind == MaskKind::Gradient)) {
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(234, 244, 255, 220), 1.6));
            const auto pointAtUv = [&](double u, double v) {
                return QPointF(drawnImageRect.left() + u * drawnImageRect.width(),
                               drawnImageRect.top() + v * drawnImageRect.height());
            };
            if (m_maskGuideKind == MaskKind::Radial) {
                const double cx = std::clamp(m_maskGuideParams.value("centerX").toDouble(0.5), 0.0, 1.0);
                const double cy = std::clamp(m_maskGuideParams.value("centerY").toDouble(0.5), 0.0, 1.0);
                const double radius = std::clamp(m_maskGuideParams.value("radius").toDouble(0.45), 0.001, 1.5);
                const double softness = std::clamp(m_maskGuideParams.value("softness").toDouble(0.18), 0.001, 1.5);
                const QPointF center = pointAtUv(cx, cy);
                const double scale = std::min(drawnImageRect.width(), drawnImageRect.height());
                painter.drawEllipse(center, radius * scale, radius * scale);
                painter.setPen(QPen(QColor(94, 202, 255, 185), 1.2, Qt::DashLine));
                painter.drawEllipse(center, (radius + softness) * scale, (radius + softness) * scale);
                painter.setBrush(QColor(234, 244, 255, 230));
                painter.setPen(QColor(18, 20, 24));
                painter.drawEllipse(center, 4.5, 4.5);
            } else if (m_maskGuideKind == MaskKind::Gradient) {
                const double angle = qDegreesToRadians(m_maskGuideParams.value("angle").toDouble(0.0));
                const double position = std::clamp(m_maskGuideParams.value("position").toDouble(0.5), 0.0, 1.0);
                const double softness = std::clamp(m_maskGuideParams.value("softness").toDouble(0.18), 0.001, 1.0);
                const QPointF axis(std::cos(angle), std::sin(angle));
                const QPointF normal(-axis.y(), axis.x());
                const QPointF center = pointAtUv(0.5 + (position - 0.5) * axis.x(),
                                                 0.5 + (position - 0.5) * axis.y());
                const double length = std::hypot(drawnImageRect.width(), drawnImageRect.height());
                const QPointF a = center - normal * length;
                const QPointF b = center + normal * length;
                painter.drawLine(a, b);
                painter.setPen(QPen(QColor(94, 202, 255, 185), 1.2, Qt::DashLine));
                const double softPixels = softness * std::min(drawnImageRect.width(), drawnImageRect.height());
                painter.drawLine(a - axis * softPixels, b - axis * softPixels);
                painter.drawLine(a + axis * softPixels, b + axis * softPixels);
                painter.setPen(QPen(QColor(255, 226, 128, 220), 1.4));
                painter.drawLine(center - axis * 26.0, center + axis * 26.0);
            }
            painter.restore();
        }
    } else {
        painter.setPen(QColor(115, 120, 130));
        painter.drawText(rect(), Qt::AlignCenter, tr("No image"));
    }

    painter.setPen(QColor(210, 214, 220));
    painter.setFont(QFont("Segoe UI", 9, QFont::DemiBold));
    painter.drawText(QRect(12, 10, width() - 24, 24), Qt::AlignLeft | Qt::AlignVCenter, m_label);

    if (!m_overlayInfo.isEmpty()) {
        const QFontMetrics metrics(painter.font());
        const QRect textRect = metrics.boundingRect(m_overlayInfo).adjusted(-10, -5, 10, 5);
        const QRect badge(width() - textRect.width() - 12, 10, textRect.width(), textRect.height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(8, 10, 12, 190));
        painter.drawRoundedRect(badge, 4, 4);
        painter.setPen(QColor(222, 226, 235));
        painter.drawText(badge, Qt::AlignCenter, m_overlayInfo);
    }

    const QString warning = warningLabel(m_warningOverlay);
    if (!warning.isEmpty()) {
        const QFontMetrics metrics(painter.font());
        const QRect textRect = metrics.boundingRect(warning).adjusted(-10, -5, 10, 5);
        const QRect badge(12, 38, textRect.width(), textRect.height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(8, 10, 12, 205));
        painter.drawRoundedRect(badge, 4, 4);
        painter.setPen(QColor(255, 224, 155));
        painter.drawText(badge, Qt::AlignCenter, warning);
    }

    if (m_pickMode) {
        if (!drawnImageRect.isNull()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(208, 142, 54, 215), 2.0));
            painter.drawRoundedRect(drawnImageRect.adjusted(1.0, 1.0, -1.0, -1.0), 4.0, 4.0);
        }
        const QString hint = m_pickHint.isEmpty() ? tr("Click to sample a Warper point") : m_pickHint;
        const QFontMetrics metrics(painter.font());
        const QRect textRect = metrics.boundingRect(hint).adjusted(-10, -5, 10, 5);
        const QRect badge(12, height() - textRect.height() - 12, textRect.width(), textRect.height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(8, 10, 12, 210));
        painter.drawRoundedRect(badge, 4, 4);
        painter.setPen(QColor(248, 229, 189));
        painter.drawText(badge, Qt::AlignCenter, hint);
    }

    if (m_maskEditMode) {
        if (!drawnImageRect.isNull()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(94, 202, 255, 225), 2.0, Qt::DashLine));
            painter.drawRoundedRect(drawnImageRect.adjusted(2.0, 2.0, -2.0, -2.0), 4.0, 4.0);
        }
        const QString hint = m_maskEditHint.isEmpty() ? tr("Drag to edit the active mask") : m_maskEditHint;
        const QFontMetrics metrics(painter.font());
        const QRect textRect = metrics.boundingRect(hint).adjusted(-10, -5, 10, 5);
        const QRect badge(12, height() - textRect.height() - 12, textRect.width(), textRect.height());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(8, 10, 12, 210));
        painter.drawRoundedRect(badge, 4, 4);
        painter.setPen(QColor(202, 238, 255));
        painter.drawText(badge, Qt::AlignCenter, hint);
    }
}

void ImageView::wheelEvent(QWheelEvent* event)
{
    const float delta = event->angleDelta().y() > 0 ? 1.1f : 0.9f;
    m_zoom = std::clamp(m_zoom * delta, 0.1f, 12.0f);
    update();
}

void ImageView::mousePressEvent(QMouseEvent* event)
{
    if (m_pickMode && event->button() == Qt::LeftButton && !m_image.isNull()) {
        bool ok = false;
        const QPoint pixel = imagePixelAt(event->pos(), &ok);
        if (ok) {
            emit imageSampled(pixel, m_image.pixelColor(pixel));
            return;
        }
    }
    if (m_maskEditMode && event->button() == Qt::LeftButton && !m_image.isNull()) {
        bool uvOk = false;
        const QPointF uv = imageUvAt(event->pos(), &uvOk);
        bool pixelOk = false;
        const QPoint pixel = imagePixelAt(event->pos(), &pixelOk);
        if (uvOk) {
            m_maskDragStart = event->pos();
            if (pixelOk) {
                emit maskPointSampled(uv, m_image.pixelColor(pixel));
            }
            return;
        }
    }
    m_lastMouse = event->pos();
}

void ImageView::mouseMoveEvent(QMouseEvent* event)
{
    const bool leftPan = !m_pickMode && (event->buttons() & Qt::LeftButton);
    const bool middlePan = event->buttons() & Qt::MiddleButton;
    if (leftPan || middlePan) {
        m_pan += event->pos() - m_lastMouse;
        m_lastMouse = event->pos();
        update();
    }
}

void ImageView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_maskEditMode && event->button() == Qt::LeftButton && !m_image.isNull()) {
        bool startOk = false;
        bool endOk = false;
        const QPointF startUv = imageUvAt(m_maskDragStart, &startOk);
        const QPointF endUv = imageUvAt(event->pos(), &endOk);
        if (startOk && endOk) {
            emit maskDragCompleted(startUv, endUv);
            return;
        }
    }
}

void ImageView::mouseDoubleClickEvent(QMouseEvent*)
{
    m_zoom = 1.0f;
    m_pan = QPointF();
    update();
}

QRectF ImageView::imageRect() const
{
    if (m_image.isNull()) {
        return QRectF();
    }
    const QSizeF base = m_image.size().scaled(size(), Qt::KeepAspectRatio);
    const QSizeF scaled(base.width() * m_zoom, base.height() * m_zoom);
    const QPointF topLeft((width() - scaled.width()) * 0.5 + m_pan.x(),
                          (height() - scaled.height()) * 0.5 + m_pan.y());
    return QRectF(topLeft, scaled);
}

QPoint ImageView::imagePixelAt(const QPoint& pos, bool* ok) const
{
    if (ok) {
        *ok = false;
    }
    if (m_image.isNull()) {
        return QPoint();
    }

    const QRectF rect = imageRect();
    if (!rect.contains(pos)) {
        return QPoint();
    }

    const qreal nx = (pos.x() - rect.left()) / std::max(1.0, rect.width());
    const qreal ny = (pos.y() - rect.top()) / std::max(1.0, rect.height());
    const int x = std::clamp(static_cast<int>(nx * m_image.width()), 0, m_image.width() - 1);
    const int y = std::clamp(static_cast<int>(ny * m_image.height()), 0, m_image.height() - 1);
    if (ok) {
        *ok = true;
    }
    return QPoint(x, y);
}

QPointF ImageView::imageUvAt(const QPoint& pos, bool* ok) const
{
    if (ok) {
        *ok = false;
    }
    if (m_image.isNull()) {
        return QPointF();
    }

    const QRectF rect = imageRect();
    if (!rect.contains(pos)) {
        return QPointF();
    }

    const qreal nx = std::clamp((pos.x() - rect.left()) / std::max(1.0, rect.width()), 0.0, 1.0);
    const qreal ny = std::clamp((pos.y() - rect.top()) / std::max(1.0, rect.height()), 0.0, 1.0);
    if (ok) {
        *ok = true;
    }
    return QPointF(nx, ny);
}

} // namespace ikclut
