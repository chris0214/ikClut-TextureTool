#include "core/MaskEvaluator.h"

#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace ikclut {

namespace {

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float smoothstep(float edge0, float edge1, float value)
{
    if (std::abs(edge1 - edge0) <= 0.000001f) {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    const float t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float luma(const QVector3D& color)
{
    return color.x() * 0.2126f + color.y() * 0.7152f + color.z() * 0.0722f;
}

void rgbToHsl(const QVector3D& rgb, float* h, float* s, float* l)
{
    const float r = rgb.x();
    const float g = rgb.y();
    const float b = rgb.z();
    const float maxv = std::max({r, g, b});
    const float minv = std::min({r, g, b});
    const float delta = maxv - minv;
    *l = (maxv + minv) * 0.5f;
    if (delta <= 0.00001f) {
        *h = 0.0f;
        *s = 0.0f;
        return;
    }
    *s = delta / (1.0f - std::abs(2.0f * (*l) - 1.0f));
    if (maxv == r) {
        *h = std::fmod((g - b) / delta, 6.0f);
    } else if (maxv == g) {
        *h = ((b - r) / delta) + 2.0f;
    } else {
        *h = ((r - g) / delta) + 4.0f;
    }
    *h /= 6.0f;
    if (*h < 0.0f) {
        *h += 1.0f;
    }
}

float colorRangeMask(const MaskAsset& mask, const QVector3D& color)
{
    float h = 0.0f;
    float s = 0.0f;
    float l = 0.0f;
    rgbToHsl(color, &h, &s, &l);
    const float tolerance = std::clamp(static_cast<float>(mask.params.value("tolerance").toDouble(0.18)), 0.001f, 1.0f);
    const float softness = std::clamp(static_cast<float>(mask.params.value("softness").toDouble(0.12)), 0.001f, 1.0f);
    const auto sampleValue = [&](float targetHue, float targetSat, float targetLuma) {
        const float hueDeltaRaw = std::abs(h - targetHue);
        const float hueDelta = std::min(hueDeltaRaw, 1.0f - hueDeltaRaw);
        const float satDelta = std::abs(s - targetSat);
        const float lumDelta = std::abs(luma(color) - targetLuma);
        const float distance = std::sqrt(hueDelta * hueDelta * 4.0f + satDelta * satDelta + lumDelta * lumDelta);
        return 1.0f - smoothstep(tolerance, tolerance + softness, distance);
    };

    float positive = sampleValue(clamp01(static_cast<float>(mask.params.value("hue").toDouble(0.0))),
                                 clamp01(static_cast<float>(mask.params.value("saturation").toDouble(0.65))),
                                 clamp01(static_cast<float>(mask.params.value("luma").toDouble(0.5))));
    float negative = 0.0f;
    const QJsonArray samples = mask.params.value("samples").toArray();
    for (const QJsonValue& value : samples) {
        const QJsonObject obj = value.toObject();
        const float sample = sampleValue(clamp01(static_cast<float>(obj.value("hue").toDouble(0.0))),
                                         clamp01(static_cast<float>(obj.value("saturation").toDouble(0.65))),
                                         clamp01(static_cast<float>(obj.value("luma").toDouble(0.5))));
        if (obj.value("subtract").toBool(false)) {
            negative = std::max(negative, sample);
        } else {
            positive = std::max(positive, sample);
        }
    }
    return clamp01(positive * (1.0f - negative));
}

float distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const double vx = b.x() - a.x();
    const double vy = b.y() - a.y();
    const double len2 = vx * vx + vy * vy;
    if (len2 <= 0.0000001) {
        const double dx = p.x() - a.x();
        const double dy = p.y() - a.y();
        return static_cast<float>(std::sqrt(dx * dx + dy * dy));
    }
    const double t = std::clamp(((p.x() - a.x()) * vx + (p.y() - a.y()) * vy) / len2, 0.0, 1.0);
    const QPointF closest(a.x() + vx * t, a.y() + vy * t);
    const double dx = p.x() - closest.x();
    const double dy = p.y() - closest.y();
    return static_cast<float>(std::sqrt(dx * dx + dy * dy));
}

float paintMask(const MaskAsset& mask, const QPointF& uv)
{
    const QJsonArray strokes = mask.params.value("strokes").toArray();
    if (strokes.isEmpty()) {
        return 0.0f;
    }

    float positive = 0.0f;
    float negative = 0.0f;
    for (const QJsonValue& value : strokes) {
        const QJsonObject stroke = value.toObject();
        const QPointF a(clamp01(static_cast<float>(stroke.value("x1").toDouble(0.5))),
                        clamp01(static_cast<float>(stroke.value("y1").toDouble(0.5))));
        const QPointF b(clamp01(static_cast<float>(stroke.value("x2").toDouble(a.x()))),
                        clamp01(static_cast<float>(stroke.value("y2").toDouble(a.y()))));
        const float fallbackRadius = mask.params.contains("brushRadius")
            ? static_cast<float>(mask.params.value("brushRadius").toDouble(0.04))
            : static_cast<float>(mask.params.value("radius").toDouble(0.04));
        const float radius = std::clamp(static_cast<float>(stroke.value("radius").toDouble(fallbackRadius)), 0.001f, 1.0f);
        const float softness = std::clamp(static_cast<float>(stroke.value("softness").toDouble(mask.params.value("softness").toDouble(0.04))), 0.001f, 1.0f);
        const float strength = std::clamp(static_cast<float>(stroke.value("strength").toDouble(1.0)), 0.0f, 1.0f);
        const float distance = distanceToSegment(uv, a, b);
        const float sample = (1.0f - smoothstep(radius, radius + softness, distance)) * strength;
        if (stroke.value("subtract").toBool(false)) {
            negative = std::max(negative, sample);
        } else {
            positive = std::max(positive, sample);
        }
    }
    return clamp01(positive * (1.0f - negative));
}

QImage externalMaskImage(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return QImage();
    }
    const QFileInfo info(path);
    const QString key = QString("%1|%2")
        .arg(info.absoluteFilePath())
        .arg(info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0);
    static QHash<QString, QImage> cache;
    if (cache.contains(key)) {
        return cache.value(key);
    }

    QImage image(info.absoluteFilePath());
    if (!image.isNull()) {
        image = image.convertToFormat(QImage::Format_RGBA8888);
    }
    cache.insert(key, image);
    return image;
}

float externalImageMask(const MaskAsset& mask, const QPointF& uv)
{
    const QImage image = externalMaskImage(mask.params.value("imagePath").toString());
    if (image.isNull()) {
        return 0.0f;
    }
    const int x = std::clamp(static_cast<int>(std::lround(clamp01(static_cast<float>(uv.x())) * (image.width() - 1))), 0, image.width() - 1);
    const int y = std::clamp(static_cast<int>(std::lround(clamp01(static_cast<float>(uv.y())) * (image.height() - 1))), 0, image.height() - 1);
    const QRgb pixel = image.pixel(x, y);
    const float gray = (qRed(pixel) * 0.2126f + qGreen(pixel) * 0.7152f + qBlue(pixel) * 0.0722f) / 255.0f;
    return clamp01(gray * (qAlpha(pixel) / 255.0f));
}

float spatialMask(const MaskAsset& mask, const QPointF& uv)
{
    switch (mask.kind) {
    case MaskKind::Gradient: {
        const float angle = static_cast<float>(qDegreesToRadians(mask.params.value("angle").toDouble(0.0)));
        const QPointF axis(std::cos(angle), std::sin(angle));
        const float position = clamp01(static_cast<float>(mask.params.value("position").toDouble(0.5)));
        const float softness = std::clamp(static_cast<float>(mask.params.value("softness").toDouble(0.18)), 0.001f, 1.0f);
        const float projection = static_cast<float>((uv.x() - 0.5) * axis.x() + (uv.y() - 0.5) * axis.y() + 0.5);
        return smoothstep(position - softness, position + softness, projection);
    }
    case MaskKind::ColorRange:
        return 1.0f;
    case MaskKind::Radial:
    case MaskKind::ExternalImage:
    case MaskKind::Full:
    default: {
        const float cx = clamp01(static_cast<float>(mask.params.value("centerX").toDouble(0.5)));
        const float cy = clamp01(static_cast<float>(mask.params.value("centerY").toDouble(0.5)));
        const float radius = std::clamp(static_cast<float>(mask.params.value("radius").toDouble(0.45)), 0.001f, 2.0f);
        const float softness = std::clamp(static_cast<float>(mask.params.value("softness").toDouble(0.18)), 0.001f, 2.0f);
        const float dx = static_cast<float>(uv.x()) - cx;
        const float dy = static_cast<float>(uv.y()) - cy;
        const float distance = std::sqrt(dx * dx + dy * dy);
        return 1.0f - smoothstep(radius, radius + softness, distance);
    }
    }
}

float evaluateMaskAsset(const MaskAsset& mask, const QPointF& uv, const QVector3D& color)
{
    switch (mask.kind) {
    case MaskKind::Full:
        return 1.0f;
    case MaskKind::Gradient:
        return spatialMask(mask, uv);
    case MaskKind::Radial:
        return spatialMask(mask, uv);
    case MaskKind::ColorRange:
        return colorRangeMask(mask, color);
    case MaskKind::Paint:
        return paintMask(mask, uv);
    case MaskKind::ExternalImage:
        return externalImageMask(mask, uv);
    }
    return 1.0f;
}

} // namespace

float MaskEvaluator::evaluate(const QVector<MaskAsset>& masks, const MaskReference& ref, const QPointF& uv)
{
    return evaluate(masks, ref, uv, QVector3D(0.5f, 0.5f, 0.5f));
}

float MaskEvaluator::evaluate(const QVector<MaskAsset>& masks, const MaskReference& ref, const QPointF& uv, const QVector3D& color)
{
    if (ref.id.isEmpty()) {
        return 1.0f;
    }

    const auto it = std::find_if(masks.begin(), masks.end(), [&](const MaskAsset& mask) {
        return mask.maskId == ref.id;
    });
    if (it == masks.end() || !it->enabled) {
        return 1.0f;
    }

    float value = evaluateMaskAsset(*it, uv, color);

    if (it->inverted ^ ref.inverted) {
        value = 1.0f - value;
    }
    return std::clamp(value * it->opacity * ref.opacity, 0.0f, 1.0f);
}

} // namespace ikclut
