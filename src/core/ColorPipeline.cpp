#include "core/ColorPipeline.h"

#include "core/MaskEvaluator.h"

#include <QtMath>
#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace ikclut {

namespace {

float clamp01(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

QVector3D clampColor(const QVector3D& c)
{
    return QVector3D(clamp01(c.x()), clamp01(c.y()), clamp01(c.z()));
}

float linearToDisplay(float value)
{
    const float v = clamp01(value);
    if (v <= 0.0031308f) {
        return v * 12.92f;
    }
    return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

QVector3D linearToDisplay(const QVector3D& color)
{
    return QVector3D(linearToDisplay(color.x()), linearToDisplay(color.y()), linearToDisplay(color.z()));
}

float decodeLogC3Ei800(float value)
{
    const float y = clamp01(value);
    constexpr float cut = 0.010591f;
    constexpr float a = 5.555556f;
    constexpr float b = 0.052272f;
    constexpr float c = 0.247190f;
    constexpr float d = 0.385537f;
    constexpr float e = 5.367655f;
    constexpr float f = 0.092809f;
    const float linearCut = e * cut + f;
    if (y > linearCut) {
        return clamp01((std::pow(10.0f, (y - d) / c) - b) / a);
    }
    return clamp01((y - f) / e);
}

float decodeSLog3(float value)
{
    const float y = clamp01(value);
    constexpr float blackCode = 95.0f;
    constexpr float cutCode = 171.2102946929f;
    constexpr float midCode = 420.0f;
    constexpr float logScale = 261.5f;
    constexpr float offset = 0.01f;
    const float code = y * 1023.0f;
    if (code >= cutCode) {
        return clamp01(std::pow(10.0f, (code - midCode) / logScale) * (0.18f + offset) - offset);
    }
    return clamp01((code - blackCode) * 0.01125f / (cutCode - blackCode));
}

float decodeVLog(float value)
{
    const float y = clamp01(value);
    constexpr float cut = 0.181f;
    if (y >= cut) {
        return clamp01(std::pow(10.0f, (y - 0.598206f) / 0.241514f) - 0.00873f);
    }
    return clamp01((y - 0.125f) / 5.6f);
}

QVector3D applyInputColorSpace(const QVector3D& color, const QString& colorSpace)
{
    const QString key = colorSpace.trimmed().toLower();
    if (key == "linear") {
        return linearToDisplay(color);
    }
    if (key == "logc3") {
        return linearToDisplay(QVector3D(decodeLogC3Ei800(color.x()),
                                         decodeLogC3Ei800(color.y()),
                                         decodeLogC3Ei800(color.z())));
    }
    if (key == "slog3") {
        return linearToDisplay(QVector3D(decodeSLog3(color.x()),
                                         decodeSLog3(color.y()),
                                         decodeSLog3(color.z())));
    }
    if (key == "vlog") {
        return linearToDisplay(QVector3D(decodeVLog(color.x()),
                                         decodeVLog(color.y()),
                                         decodeVLog(color.z())));
    }
    return clampColor(color);
}

float luma(const QVector3D& c)
{
    return c.x() * 0.2126f + c.y() * 0.7152f + c.z() * 0.0722f;
}

QVector3D lerpColor(const QVector3D& a, const QVector3D& b, float t)
{
    const float amount = std::clamp(t, 0.0f, 1.0f);
    return a * (1.0f - amount) + b * amount;
}

float wrapHue(float h)
{
    h = std::fmod(h, 1.0f);
    return h < 0.0f ? h + 1.0f : h;
}

QVector3D applySaturation(const QVector3D& c, float saturation, float vibrance)
{
    const float y = luma(c);
    const QVector3D gray(y, y, y);
    const float maxChannel = std::max({c.x(), c.y(), c.z()});
    const float amount = saturation + vibrance * (1.0f - std::abs(maxChannel - y));
    return gray + (c - gray) * std::max(0.0f, amount);
}

QPointF curveBezierHandle(const QVector<QPointF>& curve, const QVector<CurveHandleMode>& handles, int pointIndex, bool rightHandle)
{
    if (pointIndex < 0 || pointIndex >= curve.size()) {
        return QPointF();
    }

    const QPointF point = curve.at(pointIndex);
    const int neighborIndex = rightHandle ? pointIndex + 1 : pointIndex - 1;
    if (neighborIndex < 0 || neighborIndex >= curve.size()) {
        return point;
    }

    const QPointF neighbor = curve.at(neighborIndex);
    const bool autoHandle = pointIndex < handles.size() && handles.at(pointIndex) == CurveHandleMode::Auto;
    if (!autoHandle || pointIndex == 0 || pointIndex == curve.size() - 1) {
        return point + (neighbor - point) / 3.0;
    }

    const QPointF previous = curve.at(pointIndex - 1);
    const QPointF next = curve.at(pointIndex + 1);
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

float evalCurveSegment(const QVector<QPointF>& curve, const QVector<CurveHandleMode>& handles, int index, float x)
{
    const QPointF p1 = curve.at(index - 1);
    const QPointF p2 = curve.at(index);
    const bool vectorSegment = (index - 1 < handles.size() && handles.at(index - 1) == CurveHandleMode::Vector)
        && (index < handles.size() && handles.at(index) == CurveHandleMode::Vector);
    if (vectorSegment) {
        const float denom = std::max(0.0001f, static_cast<float>(p2.x() - p1.x()));
        const float t = std::clamp((x - static_cast<float>(p1.x())) / denom, 0.0f, 1.0f);
        return clamp01(static_cast<float>(p1.y()) * (1.0f - t) + static_cast<float>(p2.y()) * t);
    }

    const QPointF h1 = curveBezierHandle(curve, handles, index - 1, true);
    const QPointF h2 = curveBezierHandle(curve, handles, index, false);
    const auto cubic = [](double a, double b, double c, double d, double t) {
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
    return clamp01(static_cast<float>(cubic(p1.y(), h1.y(), h2.y(), p2.y(), t)));
}

float evalCurve(const QVector<QPointF>& curve, const QVector<CurveHandleMode>& handles, float x)
{
    const float clampedX = clamp01(x);
    if (curve.size() < 2) {
        return clampedX;
    }
    for (int i = 1; i < curve.size(); ++i) {
        const QPointF b = curve.at(i);
        if (clampedX <= b.x()) {
            return evalCurveSegment(curve, handles, i, clampedX);
        }
    }
    return clamp01(static_cast<float>(curve.last().y()));
}

QVector3D applyCurves(const QVector3D& c, const ColorGradeParams& params)
{
    QVector3D out(evalCurve(params.redCurve, params.redCurveHandles, c.x()),
                  evalCurve(params.greenCurve, params.greenCurveHandles, c.y()),
                  evalCurve(params.blueCurve, params.blueCurveHandles, c.z()));
    out.setX(evalCurve(params.masterCurve, params.masterCurveHandles, out.x()));
    out.setY(evalCurve(params.masterCurve, params.masterCurveHandles, out.y()));
    out.setZ(evalCurve(params.masterCurve, params.masterCurveHandles, out.z()));
    return out;
}

float applySoftClipChannel(float value, const ColorGradeParams& params)
{
    const float low = std::clamp(params.softClipLow, 0.0f, 0.45f);
    const float high = std::clamp(params.softClipHigh, 0.55f, 1.0f);
    const float lowSoftness = std::clamp(params.softClipLowSoftness, 0.0f, 1.0f);
    const float highSoftness = std::clamp(params.softClipHighSoftness, 0.0f, 1.0f);
    float v = value;

    if (low > 0.0001f) {
        v = std::max(v, low * lowSoftness);
        v = (v - low * lowSoftness) / std::max(0.0001f, 1.0f - low * lowSoftness);
    }
    if (high < 0.9999f) {
        v = std::min(v, high + (1.0f - high) * (1.0f - highSoftness));
        v = v / std::max(0.0001f, high + (1.0f - high) * (1.0f - highSoftness));
    }
    return clamp01(v);
}

QVector3D applySoftClip(const QVector3D& c, const ColorGradeParams& params)
{
    return QVector3D(applySoftClipChannel(c.x(), params),
                     applySoftClipChannel(c.y(), params),
                     applySoftClipChannel(c.z(), params));
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

float hueToRgb(float p, float q, float t)
{
    if (t < 0.0f) {
        t += 1.0f;
    }
    if (t > 1.0f) {
        t -= 1.0f;
    }
    if (t < 1.0f / 6.0f) {
        return p + (q - p) * 6.0f * t;
    }
    if (t < 1.0f / 2.0f) {
        return q;
    }
    if (t < 2.0f / 3.0f) {
        return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    }
    return p;
}

QVector3D hslToRgb(float h, float s, float l)
{
    h = wrapHue(h);
    s = clamp01(s);
    l = clamp01(l);
    if (s <= 0.00001f) {
        return QVector3D(l, l, l);
    }
    const float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    const float p = 2.0f * l - q;
    return QVector3D(hueToRgb(p, q, h + 1.0f / 3.0f),
                     hueToRgb(p, q, h),
                     hueToRgb(p, q, h - 1.0f / 3.0f));
}

void rgbToHsv(const QVector3D& rgb, float* h, float* s, float* v)
{
    const float r = rgb.x();
    const float g = rgb.y();
    const float b = rgb.z();
    const float maxv = std::max({r, g, b});
    const float minv = std::min({r, g, b});
    const float delta = maxv - minv;
    *v = maxv;
    *s = maxv <= 0.00001f ? 0.0f : delta / maxv;
    if (delta <= 0.00001f) {
        *h = 0.0f;
        return;
    }
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

QVector3D hsvToRgb(float h, float s, float v)
{
    h = std::fmod(h + 1.0f, 1.0f);
    s = clamp01(s);
    v = clamp01(v);
    const float c = v * s;
    const float x = c * (1.0f - std::abs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    const float m = v - c;
    QVector3D rgb;
    if (h < 1.0f / 6.0f) {
        rgb = QVector3D(c, x, 0.0f);
    } else if (h < 2.0f / 6.0f) {
        rgb = QVector3D(x, c, 0.0f);
    } else if (h < 3.0f / 6.0f) {
        rgb = QVector3D(0.0f, c, x);
    } else if (h < 4.0f / 6.0f) {
        rgb = QVector3D(0.0f, x, c);
    } else if (h < 5.0f / 6.0f) {
        rgb = QVector3D(x, 0.0f, c);
    } else {
        rgb = QVector3D(c, 0.0f, x);
    }
    return rgb + QVector3D(m, m, m);
}

QVector3D applyToneRanges(const QVector3D& color, const ColorGradeParams& params)
{
    const float y = luma(color);
    const float shadowMask = std::pow(clamp01(1.0f - y), 1.7f);
    const float highlightMask = std::pow(clamp01(y), 1.7f);
    const float whiteMask = std::pow(clamp01((y - 0.62f) / 0.38f), 1.4f);
    const float blackMask = std::pow(clamp01((0.38f - y) / 0.38f), 1.4f);

    QVector3D c = color;
    c += QVector3D(params.shadows * 0.28f * shadowMask,
                   params.shadows * 0.28f * shadowMask,
                   params.shadows * 0.28f * shadowMask);
    c += QVector3D(params.highlights * 0.24f * highlightMask,
                   params.highlights * 0.24f * highlightMask,
                   params.highlights * 0.24f * highlightMask);
    c += QVector3D(params.whites * 0.18f * whiteMask,
                   params.whites * 0.18f * whiteMask,
                   params.whites * 0.18f * whiteMask);
    c += QVector3D(params.blacks * 0.18f * blackMask,
                   params.blacks * 0.18f * blackMask,
                   params.blacks * 0.18f * blackMask);
    return c;
}

float bellWeight(float value, float center, float width)
{
    const float x = (value - center) / std::max(0.0001f, width);
    return std::exp(-x * x * 2.0f);
}

QVector3D applyLogHdrWheels(const QVector3D& color, const ColorGradeParams& params)
{
    const float y = luma(color);
    const float logShadow = bellWeight(y, 0.12f, 0.16f);
    const float logDark = bellWeight(y, 0.34f, 0.18f);
    const float logLight = bellWeight(y, 0.66f, 0.18f);
    const float logHighlight = bellWeight(y, 0.9f, 0.16f);
    const float hdrShadow = std::pow(clamp01((0.36f - y) / 0.36f), 1.9f);
    const float hdrDark = bellWeight(y, 0.28f, 0.2f);
    const float hdrLight = bellWeight(y, 0.72f, 0.2f);
    const float hdrHighlight = std::pow(clamp01((y - 0.64f) / 0.36f), 1.9f);

    const float logOffset = params.logShadow * logShadow
        + params.logDark * logDark
        + params.logLight * logLight
        + params.logHighlight * logHighlight;
    const float hdrOffset = params.hdrShadow * hdrShadow
        + params.hdrDark * hdrDark
        + params.hdrLight * hdrLight
        + params.hdrHighlight * hdrHighlight;
    return color + QVector3D(logOffset * 0.18f + hdrOffset * 0.22f,
                             logOffset * 0.18f + hdrOffset * 0.22f,
                             logOffset * 0.18f + hdrOffset * 0.22f);
}

QVector3D applyPrinterLights(const QVector3D& color, const ColorGradeParams& params)
{
    return color + QVector3D(params.printerRed, params.printerGreen, params.printerBlue) * 0.025f;
}

QVector3D applyPresence(const QVector3D& color, const ColorGradeParams& params)
{
    const float y = luma(color);
    const QVector3D gray(y, y, y);
    QVector3D c = color;

    const float midMask = 1.0f - std::min(1.0f, std::abs(y - 0.5f) * 2.0f);
    c = gray + (c - gray) * (1.0f + params.clarity * 0.35f * midMask);
    c = gray + (c - gray) * (1.0f + params.texture * 0.18f);
    c = (c - QVector3D(0.5f, 0.5f, 0.5f)) * (1.0f + params.dehaze * 0.22f) + QVector3D(0.5f, 0.5f, 0.5f);
    c += QVector3D(params.dehaze * -0.015f, params.dehaze * -0.01f, params.dehaze * 0.02f);
    return c;
}

QVector3D applyCalibration(const QVector3D& color, const ColorGradeParams& params)
{
    float h = 0.0f;
    float s = 0.0f;
    float l = 0.0f;
    rgbToHsl(color, &h, &s, &l);

    const auto hueDistance = [](float a, float b) {
        const float d = std::abs(a - b);
        return std::min(d, 1.0f - d);
    };
    const float redWeight = clamp01(1.0f - hueDistance(h, 0.0f) / 0.18f);
    const float greenWeight = clamp01(1.0f - hueDistance(h, 1.0f / 3.0f) / 0.18f);
    const float blueWeight = clamp01(1.0f - hueDistance(h, 2.0f / 3.0f) / 0.18f);

    const float hueShift = params.calibrationRedHue * redWeight
        + params.calibrationGreenHue * greenWeight
        + params.calibrationBlueHue * blueWeight;
    const float satShift = params.calibrationRedSaturation * redWeight
        + params.calibrationGreenSaturation * greenWeight
        + params.calibrationBlueSaturation * blueWeight;

    h = std::fmod(h + hueShift / 360.0f + 1.0f, 1.0f);
    s = clamp01(s * (1.0f + satShift));
    return hslToRgb(h, s, l);
}

QVector3D applyColorWarp(const QVector3D& color, const ColorGradeParams& params)
{
    if (params.colorWarpPoints.isEmpty()) {
        return color;
    }

    float h = 0.0f;
    float s = 0.0f;
    float l = 0.0f;
    rgbToHsl(color, &h, &s, &l);

    const int count = std::min(params.colorWarpSources.size(), params.colorWarpPoints.size());
    if (count <= 0) {
        return color;
    }
    float hueOffset = 0.0f;
    float satOffset = 0.0f;
    float lumOffset = 0.0f;
    float totalWeight = 0.0f;
    for (int i = 0; i < count; ++i) {
        const QPointF source = params.colorWarpSources.at(i);
        const QPointF target = params.colorWarpPoints.at(i);
        const float sourceHue = clamp01(static_cast<float>(source.x()));
        const float sourceSat = clamp01(static_cast<float>(source.y()));
        const float targetHue = clamp01(static_cast<float>(target.x()));
        const float targetSat = clamp01(static_cast<float>(target.y()));
        const float sourceLum = i < params.colorWarpSourceLuma.size() ? clamp01(params.colorWarpSourceLuma.at(i)) : 0.5f;
        const float targetLum = i < params.colorWarpTargetLuma.size() ? clamp01(params.colorWarpTargetLuma.at(i)) : sourceLum;
        const float hueDistRaw = std::abs(h - sourceHue);
        const float hueDist = std::min(hueDistRaw, 1.0f - hueDistRaw);
        const float satDist = std::abs(s - sourceSat);
        const float lumDist = std::abs(l - sourceLum);
        const float distance2 = hueDist * hueDist * 16.0f + satDist * satDist * 5.5f + lumDist * lumDist * 4.0f;
        const float weight = std::exp(-distance2);
        const float deltaHue = std::remainder(targetHue - sourceHue, 1.0f);
        const float deltaSat = targetSat - sourceSat;
        const float deltaLum = targetLum - sourceLum;
        hueOffset += deltaHue * weight;
        satOffset += deltaSat * weight;
        lumOffset += deltaLum * weight;
        totalWeight += weight;
    }
    if (totalWeight > 0.0f) {
        hueOffset /= totalWeight;
        satOffset /= totalWeight;
        lumOffset /= totalWeight;
    }

    h = std::fmod(h + hueOffset * 0.86f + 1.0f, 1.0f);
    s = clamp01(s + satOffset * 0.86f);
    l = clamp01(l + lumOffset * 0.86f);
    return hslToRgb(h, s, l);
}

QVector3D applyHsl(const QVector3D& c, const ColorGradeParams& params)
{
    float h = 0.0f;
    float s = 0.0f;
    float l = 0.0f;
    rgbToHsl(c, &h, &s, &l);

    const float scaled = h * 8.0f;
    const int i0 = std::clamp(static_cast<int>(std::floor(scaled)) % 8, 0, 7);
    const int i1 = (i0 + 1) % 8;
    const float t = scaled - std::floor(scaled);
    const auto lerpBand = [&](const std::array<float, 8>& values) {
        return values[static_cast<size_t>(i0)] * (1.0f - t) + values[static_cast<size_t>(i1)] * t;
    };

    h = std::fmod(h + lerpBand(params.hslHue) / 360.0f + 1.0f, 1.0f);
    s = clamp01(s * (1.0f + lerpBand(params.hslSaturation)));
    l = clamp01(l + lerpBand(params.hslLuminance));
    return hslToRgb(h, s, l);
}

QVector3D applyHueVsCurves(const QVector3D& c, const ColorGradeParams& params)
{
    float h = 0.0f;
    float s = 0.0f;
    float l = 0.0f;
    rgbToHsl(c, &h, &s, &l);

    const float hueShift = (evalCurve(params.hueVsHueCurve, params.hueVsHueCurveHandles, h) - 0.5f) * 0.5f;
    const float satAdjust = (evalCurve(params.hueVsSaturationCurve, params.hueVsSaturationCurveHandles, h) - 0.5f) * 2.0f;
    const float lumAdjust = evalCurve(params.hueVsLuminanceCurve, params.hueVsLuminanceCurveHandles, h) - 0.5f;

    h = std::fmod(h + hueShift + 1.0f, 1.0f);
    s = clamp01(s * (1.0f + satAdjust));
    l = clamp01(l + lumAdjust);
    return hslToRgb(h, s, l);
}

QVector3D applySkinToneTool(const QVector3D& c, const ColorGradeParams& params)
{
    if (std::abs(params.skinToneHue) <= 0.0001f
        && std::abs(params.skinToneSaturation) <= 0.0001f
        && std::abs(params.skinToneLuminance) <= 0.0001f) {
        return c;
    }

    float h = 0.0f;
    float s = 0.0f;
    float l = 0.0f;
    rgbToHsl(c, &h, &s, &l);
    const float targetHue = 28.0f / 360.0f;
    const float hueDelta = std::min(std::abs(h - targetHue), 1.0f - std::abs(h - targetHue));
    const float hueWeight = clamp01(1.0f - hueDelta / 0.11f);
    const float satWeight = clamp01((s - 0.12f) / 0.28f) * clamp01((0.92f - s) / 0.35f);
    const float lumWeight = clamp01((l - 0.18f) / 0.26f) * clamp01((0.92f - l) / 0.26f);
    const float weight = std::pow(hueWeight * satWeight * lumWeight, 0.65f);
    if (weight <= 0.0001f) {
        return c;
    }

    h = std::fmod(h + params.skinToneHue * weight / 360.0f + 1.0f, 1.0f);
    s = clamp01(s * (1.0f + params.skinToneSaturation * weight));
    l = clamp01(l + params.skinToneLuminance * 0.28f * weight);
    return hslToRgb(h, s, l);
}

float blendChannel(float base, float blend, const QString& mode)
{
    if (mode == "darken") {
        return std::min(base, blend);
    }
    if (mode == "multiply") {
        return base * blend;
    }
    if (mode == "color-burn") {
        return blend <= 0.00001f ? 0.0f : 1.0f - std::min(1.0f, (1.0f - base) / blend);
    }
    if (mode == "lighten") {
        return std::max(base, blend);
    }
    if (mode == "screen") {
        return 1.0f - (1.0f - base) * (1.0f - blend);
    }
    if (mode == "color-dodge") {
        return blend >= 0.99999f ? 1.0f : std::min(1.0f, base / (1.0f - blend));
    }
    if (mode == "overlay") {
        return base < 0.5f ? 2.0f * base * blend : 1.0f - 2.0f * (1.0f - base) * (1.0f - blend);
    }
    if (mode == "soft-light") {
        return (1.0f - 2.0f * blend) * base * base + 2.0f * blend * base;
    }
    if (mode == "linear-light") {
        return base + 2.0f * blend - 1.0f;
    }
    if (mode == "difference") {
        return std::abs(base - blend);
    }
    if (mode == "exclusion") {
        return base + blend - 2.0f * base * blend;
    }
    if (mode == "subtract") {
        return base - blend;
    }
    if (mode == "divide") {
        return blend <= 0.00001f ? 1.0f : base / blend;
    }
    if (mode == "add") {
        return base + blend;
    }
    return blend;
}

QVector3D blendColorMode(const QVector3D& base, const QVector3D& blend, const QString& mode)
{
    const QString key = mode.trimmed().toLower();
    if (key == "hue" || key == "saturation" || key == "color" || key == "value") {
        float bh = 0.0f;
        float bs = 0.0f;
        float bv = 0.0f;
        float lh = 0.0f;
        float ls = 0.0f;
        float lv = 0.0f;
        rgbToHsv(clampColor(base), &bh, &bs, &bv);
        rgbToHsv(clampColor(blend), &lh, &ls, &lv);
        if (key == "hue") {
            return hsvToRgb(lh, bs, bv);
        }
        if (key == "saturation") {
            return hsvToRgb(bh, ls, bv);
        }
        if (key == "color") {
            return hsvToRgb(lh, ls, bv);
        }
        return hsvToRgb(bh, bs, lv);
    }

    return QVector3D(blendChannel(base.x(), blend.x(), key),
                     blendChannel(base.y(), blend.y(), key),
                     blendChannel(base.z(), blend.z(), key));
}

QVector3D vectorParam(const QJsonObject& params, const QString& key, const QVector3D& fallback)
{
    return vectorFromJson(params.value(key), fallback);
}

QVector3D applyNodeStage(const QVector3D& color, const ColorGradeStage& stage, const Lut3D* importedLut = nullptr);
QVector3D blendStageResult(const QVector3D& before, const QVector3D& after, float opacity, const QString& blendMode);

QVector3D applyEmbeddedStages(const QVector3D& color, const QJsonArray& stages, const Lut3D* importedLut)
{
    QVector3D c = color;
    for (const QJsonValue& value : stages) {
        const QJsonObject obj = value.toObject();
        ColorGradeStage stage;
        stage.stageId = obj.value("stageId").toString();
        stage.type = obj.value("type").toString("globalColorGrade");
        stage.label = obj.value("label").toString();
        stage.enabled = obj.value("enabled").toBool(true);
        stage.opacity = std::clamp(static_cast<float>(obj.value("opacity").toDouble(1.0)), 0.0f, 1.0f);
        stage.blendMode = obj.value("blendMode").toString("normal");
        stage.params = obj.value("params").toObject();
        if (!stage.enabled) {
            continue;
        }
        const QVector3D before = c;
        const QVector3D after = applyNodeStage(before, stage, importedLut);
        c = blendStageResult(before, after, stage.opacity, stage.blendMode);
    }
    return clampColor(c);
}

struct MixerBranchResult {
    QVector3D color;
    float weight = 1.0f;
};

QVector<MixerBranchResult> evaluateMixerBranches(const QVector3D& color, const QJsonObject& params, const Lut3D* importedLut)
{
    QVector<MixerBranchResult> outputs;
    const QJsonArray branches = params.value("branches").toArray();
    const QJsonArray weights = params.value("weights").toArray();
    outputs.reserve(branches.size());
    for (int i = 0; i < branches.size(); ++i) {
        const QJsonValue value = branches.at(i);
        QJsonArray stages;
        float weight = i < weights.size() ? static_cast<float>(weights.at(i).toDouble(1.0)) : 1.0f;
        if (value.isArray()) {
            stages = value.toArray();
        } else if (value.isObject()) {
            const QJsonObject branchObj = value.toObject();
            stages = branchObj.value("stages").toArray();
            weight = static_cast<float>(branchObj.value("weight").toDouble(weight));
        }
        weight = std::clamp(weight, 0.0f, 4.0f);
        if (!stages.isEmpty()) {
            outputs.append(MixerBranchResult{applyEmbeddedStages(color, stages, importedLut), weight});
        }
    }
    return outputs;
}

QVector3D applyParallelMixerNode(const QVector3D& color, const QJsonObject& params, const Lut3D* importedLut)
{
    const QVector<MixerBranchResult> outputs = evaluateMixerBranches(color, params, importedLut);
    if (outputs.isEmpty()) {
        return clampColor(color);
    }
    QVector3D sum(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;
    for (const MixerBranchResult& output : outputs) {
        sum += output.color * output.weight;
        totalWeight += output.weight;
    }
    if (totalWeight <= 0.0f) {
        return clampColor(color);
    }
    const float mix = std::clamp(static_cast<float>(params.value("mix").toDouble(1.0)), 0.0f, 1.0f);
    return clampColor(lerpColor(color, sum / totalWeight, mix));
}

QVector3D applyLayerMixerNode(const QVector3D& color, const QJsonObject& params, const Lut3D* importedLut)
{
    const QVector<MixerBranchResult> outputs = evaluateMixerBranches(color, params, importedLut);
    if (outputs.isEmpty()) {
        return clampColor(color);
    }
    QVector3D mixed = color;
    for (const MixerBranchResult& output : outputs) {
        mixed += (output.color - color) * output.weight;
    }
    const float mix = std::clamp(static_cast<float>(params.value("mix").toDouble(1.0)), 0.0f, 1.0f);
    return clampColor(lerpColor(color, mixed, mix));
}

QVector3D applyMixColorNode(const QVector3D& color, const QJsonObject& params, const Lut3D* importedLut)
{
    const QString mode = params.value("blendType").toString("mix");
    const float factor = std::clamp(static_cast<float>(params.value("factor").toDouble(1.0)), 0.0f, 1.0f);
    const QVector3D colorA = vectorParam(params, "colorA", color);
    QVector3D colorB = vectorParam(params, "colorB", QVector3D(1.0f, 1.0f, 1.0f));
    const QJsonArray branchB = params.value("branchBStages").toArray();
    if (!branchB.isEmpty()) {
        colorB = applyEmbeddedStages(color, branchB, importedLut);
    }
    const bool useFlowAsA = params.value("useFlowAsA").toBool(true);
    const bool clampResult = params.value("clampResult").toBool(true);
    const QVector3D base = useFlowAsA ? color : colorA;
    QVector3D blended = blendColorMode(base, colorB, mode);
    QVector3D out = lerpColor(base, blended, factor);
    return clampResult ? clampColor(out) : out;
}

QVector3D applyBrightnessContrastNode(const QVector3D& color, const QJsonObject& params)
{
    const float brightness = static_cast<float>(params.value("brightness").toDouble(0.0));
    const float contrast = static_cast<float>(params.value("contrast").toDouble(0.0));
    const float factor = std::max(0.0f, 1.0f + contrast);
    return clampColor((color + QVector3D(brightness, brightness, brightness) - QVector3D(0.5f, 0.5f, 0.5f)) * factor
                      + QVector3D(0.5f, 0.5f, 0.5f));
}

QVector3D applyHueSaturationValueNode(const QVector3D& color, const QJsonObject& params)
{
    float h = 0.0f;
    float s = 0.0f;
    float v = 0.0f;
    rgbToHsv(clampColor(color), &h, &s, &v);
    h = wrapHue(h + static_cast<float>(params.value("hue").toDouble(0.5) - 0.5));
    s = clamp01(s * static_cast<float>(params.value("saturation").toDouble(1.0)));
    v = clamp01(v * static_cast<float>(params.value("value").toDouble(1.0)));
    return hsvToRgb(h, s, v);
}

QVector3D applyGammaNode(const QVector3D& color, const QJsonObject& params)
{
    const float gamma = std::max(0.05f, static_cast<float>(params.value("gamma").toDouble(1.0)));
    return QVector3D(std::pow(clamp01(color.x()), 1.0f / gamma),
                     std::pow(clamp01(color.y()), 1.0f / gamma),
                     std::pow(clamp01(color.z()), 1.0f / gamma));
}

QVector3D applyColorBalanceNode(const QVector3D& color, const QJsonObject& params)
{
    const QVector3D lift = vectorParam(params, "lift", QVector3D(0.0f, 0.0f, 0.0f));
    const QVector3D gamma = vectorParam(params, "gamma", QVector3D(1.0f, 1.0f, 1.0f));
    const QVector3D gain = vectorParam(params, "gain", QVector3D(1.0f, 1.0f, 1.0f));
    QVector3D c = (color + lift) * gain;
    c.setX(std::pow(clamp01(c.x()), 1.0f / std::max(0.05f, gamma.x())));
    c.setY(std::pow(clamp01(c.y()), 1.0f / std::max(0.05f, gamma.y())));
    c.setZ(std::pow(clamp01(c.z()), 1.0f / std::max(0.05f, gamma.z())));
    return clampColor(c);
}

QVector3D applyRgbCurvesNode(const QVector3D& color, const QJsonObject& params)
{
    ColorGradeParams curveParams;
    curveParams.masterCurve = curveFromJson(params.value("masterCurve"), curveParams.masterCurve);
    curveParams.redCurve = curveFromJson(params.value("redCurve"), curveParams.redCurve);
    curveParams.greenCurve = curveFromJson(params.value("greenCurve"), curveParams.greenCurve);
    curveParams.blueCurve = curveFromJson(params.value("blueCurve"), curveParams.blueCurve);
    curveParams.masterCurveHandles = curveHandlesFromJson(params.value("masterCurveHandles"), curveParams.masterCurve.size());
    curveParams.redCurveHandles = curveHandlesFromJson(params.value("redCurveHandles"), curveParams.redCurve.size());
    curveParams.greenCurveHandles = curveHandlesFromJson(params.value("greenCurveHandles"), curveParams.greenCurve.size());
    curveParams.blueCurveHandles = curveHandlesFromJson(params.value("blueCurveHandles"), curveParams.blueCurve.size());
    if (params.contains("masterMid")) {
        curveParams.masterCurve = {QPointF(0.0, 0.0), QPointF(0.5, params.value("masterMid").toDouble(0.5)), QPointF(1.0, 1.0)};
        curveParams.masterCurveHandles = {CurveHandleMode::Auto, CurveHandleMode::Auto, CurveHandleMode::Auto};
    }
    if (params.contains("redMid")) {
        curveParams.redCurve = {QPointF(0.0, 0.0), QPointF(0.5, params.value("redMid").toDouble(0.5)), QPointF(1.0, 1.0)};
        curveParams.redCurveHandles = {CurveHandleMode::Auto, CurveHandleMode::Auto, CurveHandleMode::Auto};
    }
    if (params.contains("greenMid")) {
        curveParams.greenCurve = {QPointF(0.0, 0.0), QPointF(0.5, params.value("greenMid").toDouble(0.5)), QPointF(1.0, 1.0)};
        curveParams.greenCurveHandles = {CurveHandleMode::Auto, CurveHandleMode::Auto, CurveHandleMode::Auto};
    }
    if (params.contains("blueMid")) {
        curveParams.blueCurve = {QPointF(0.0, 0.0), QPointF(0.5, params.value("blueMid").toDouble(0.5)), QPointF(1.0, 1.0)};
        curveParams.blueCurveHandles = {CurveHandleMode::Auto, CurveHandleMode::Auto, CurveHandleMode::Auto};
    }
    return applyCurves(clampColor(color), curveParams);
}

QVector3D applyColorCorrectionNode(const QVector3D& color, const QJsonObject& params)
{
    const float mask = std::pow(clamp01(luma(color)), 1.0f);
    const float shadowWeight = std::pow(clamp01((0.55f - mask) / 0.55f), 1.4f);
    const float highlightWeight = std::pow(clamp01((mask - 0.45f) / 0.55f), 1.4f);
    const float midtoneWeight = clamp01(1.0f - std::abs(mask - 0.5f) * 2.0f);

    const auto weightedParam = [&](const char* master, const char* shadows, const char* midtones, const char* highlights, float fallback) {
        return static_cast<float>(params.value(master).toDouble(fallback))
            + static_cast<float>(params.value(shadows).toDouble(0.0)) * shadowWeight
            + static_cast<float>(params.value(midtones).toDouble(0.0)) * midtoneWeight
            + static_cast<float>(params.value(highlights).toDouble(0.0)) * highlightWeight;
    };

    const float hue = weightedParam("masterHue", "shadowsHue", "midtonesHue", "highlightsHue", 0.0f);
    const float saturation = std::max(0.0f, weightedParam("masterSaturation", "shadowsSaturation", "midtonesSaturation", "highlightsSaturation", 1.0f));
    const float value = std::max(0.0f, weightedParam("masterValue", "shadowsValue", "midtonesValue", "highlightsValue", 1.0f));
    const float contrast = weightedParam("masterContrast", "shadowsContrast", "midtonesContrast", "highlightsContrast", 0.0f);
    const float gamma = std::max(0.05f, weightedParam("masterGamma", "shadowsGamma", "midtonesGamma", "highlightsGamma", 1.0f));
    const QVector3D lift = vectorParam(params, "lift", QVector3D(0.0f, 0.0f, 0.0f));
    const QVector3D gain = vectorParam(params, "gain", QVector3D(1.0f, 1.0f, 1.0f));

    float h = 0.0f;
    float s = 0.0f;
    float v = 0.0f;
    rgbToHsv(clampColor(color), &h, &s, &v);
    QVector3D c = hsvToRgb(h + hue / 360.0f, s * saturation, v * value);
    c = (c - QVector3D(0.5f, 0.5f, 0.5f)) * std::max(0.0f, 1.0f + contrast) + QVector3D(0.5f, 0.5f, 0.5f);
    c = (c + lift) * gain;
    c.setX(std::pow(clamp01(c.x()), 1.0f / gamma));
    c.setY(std::pow(clamp01(c.y()), 1.0f / gamma));
    c.setZ(std::pow(clamp01(c.z()), 1.0f / gamma));
    return clampColor(c);
}

QVector3D applyInvertNode(const QVector3D& color, const QJsonObject& params)
{
    const float factor = std::clamp(static_cast<float>(params.value("factor").toDouble(1.0)), 0.0f, 1.0f);
    return lerpColor(color, QVector3D(1.0f, 1.0f, 1.0f) - color, factor);
}

QVector3D applyPosterizeNode(const QVector3D& color, const QJsonObject& params)
{
    const float steps = std::max(2.0f, static_cast<float>(params.value("steps").toDouble(8.0)));
    const float denom = steps - 1.0f;
    const auto posterize = [&](float v) {
        return std::round(clamp01(v) * denom) / denom;
    };
    return QVector3D(posterize(color.x()), posterize(color.y()), posterize(color.z()));
}

QVector3D applyClampNode(const QVector3D& color, const QJsonObject& params)
{
    const float minimum = clamp01(static_cast<float>(params.value("minimum").toDouble(0.0)));
    const float maximum = clamp01(static_cast<float>(params.value("maximum").toDouble(1.0)));
    const float lo = std::min(minimum, maximum);
    const float hi = std::max(minimum, maximum);
    return QVector3D(std::clamp(color.x(), lo, hi),
                     std::clamp(color.y(), lo, hi),
                     std::clamp(color.z(), lo, hi));
}

QVector3D applyLevelsNode(const QVector3D& color, const QJsonObject& params)
{
    const float blackPoint = clamp01(static_cast<float>(params.value("blackPoint").toDouble(0.0)));
    const float whitePoint = clamp01(static_cast<float>(params.value("whitePoint").toDouble(1.0)));
    const float inputLo = std::min(blackPoint, whitePoint - 0.001f);
    const float inputHi = std::max(whitePoint, blackPoint + 0.001f);
    const float gamma = std::max(0.05f, static_cast<float>(params.value("gamma").toDouble(1.0)));
    const float outputBlack = clamp01(static_cast<float>(params.value("outputBlack").toDouble(0.0)));
    const float outputWhite = clamp01(static_cast<float>(params.value("outputWhite").toDouble(1.0)));
    const auto level = [&](float v) {
        const float normalized = clamp01((v - inputLo) / std::max(0.0001f, inputHi - inputLo));
        const float corrected = std::pow(normalized, 1.0f / gamma);
        return outputBlack + corrected * (outputWhite - outputBlack);
    };
    return clampColor(QVector3D(level(color.x()), level(color.y()), level(color.z())));
}

QVector3D applyExposureNode(const QVector3D& color, const QJsonObject& params)
{
    const float exposure = static_cast<float>(params.value("exposure").toDouble(0.0));
    const float offset = static_cast<float>(params.value("offset").toDouble(0.0));
    return clampColor(color * std::pow(2.0f, exposure) + QVector3D(offset, offset, offset));
}

QVector3D applyThresholdNode(const QVector3D& color, const QJsonObject& params)
{
    const float threshold = clamp01(static_cast<float>(params.value("threshold").toDouble(0.5)));
    const float softness = std::max(0.0f, static_cast<float>(params.value("softness").toDouble(0.0)));
    const float y = luma(color);
    const float value = softness <= 0.0001f
        ? (y >= threshold ? 1.0f : 0.0f)
        : clamp01((y - threshold + softness) / (softness * 2.0f));
    return QVector3D(value, value, value);
}

QVector3D applySepiaNode(const QVector3D& color, const QJsonObject& params)
{
    const float factor = std::clamp(static_cast<float>(params.value("factor").toDouble(1.0)), 0.0f, 1.0f);
    const QVector3D sepia(std::min(1.0f, color.x() * 0.393f + color.y() * 0.769f + color.z() * 0.189f),
                          std::min(1.0f, color.x() * 0.349f + color.y() * 0.686f + color.z() * 0.168f),
                          std::min(1.0f, color.x() * 0.272f + color.y() * 0.534f + color.z() * 0.131f));
    return clampColor(lerpColor(color, sepia, factor));
}

QVector3D applyChannelMixerNode(const QVector3D& color, const QJsonObject& params)
{
    const QVector3D red = vectorParam(params, "red", QVector3D(1.0f, 0.0f, 0.0f));
    const QVector3D green = vectorParam(params, "green", QVector3D(0.0f, 1.0f, 0.0f));
    const QVector3D blue = vectorParam(params, "blue", QVector3D(0.0f, 0.0f, 1.0f));
    return clampColor(QVector3D(QVector3D::dotProduct(color, red),
                                QVector3D::dotProduct(color, green),
                                QVector3D::dotProduct(color, blue)));
}

QVector3D applyTemperatureTintNode(const QVector3D& color, const QJsonObject& params)
{
    const float temperature = static_cast<float>(params.value("temperature").toDouble(0.0));
    const float tint = static_cast<float>(params.value("tint").toDouble(0.0));
    return clampColor(color + QVector3D(temperature * 0.08f, tint * 0.05f, -temperature * 0.08f));
}

QVector3D applyGrayscaleNode(const QVector3D& color, const QJsonObject& params)
{
    const float factor = std::clamp(static_cast<float>(params.value("factor").toDouble(1.0)), 0.0f, 1.0f);
    const float y = luma(color);
    return clampColor(lerpColor(color, QVector3D(y, y, y), factor));
}

QVector3D applyVibranceNode(const QVector3D& color, const QJsonObject& params)
{
    const float saturation = static_cast<float>(params.value("saturation").toDouble(1.0));
    const float vibrance = static_cast<float>(params.value("vibrance").toDouble(0.0));
    return clampColor(applySaturation(color, saturation, vibrance));
}

QVector3D applySoftClipNode(const QVector3D& color, const QJsonObject& params)
{
    ColorGradeParams softClipParams;
    softClipParams.softClipLow = static_cast<float>(params.value("low").toDouble(0.0));
    softClipParams.softClipHigh = static_cast<float>(params.value("high").toDouble(1.0));
    softClipParams.softClipLowSoftness = static_cast<float>(params.value("lowSoftness").toDouble(0.0));
    softClipParams.softClipHighSoftness = static_cast<float>(params.value("highSoftness").toDouble(0.0));
    return applySoftClip(color, softClipParams);
}

QVector3D applyDuotoneNode(const QVector3D& color, const QJsonObject& params)
{
    const float factor = std::clamp(static_cast<float>(params.value("factor").toDouble(1.0)), 0.0f, 1.0f);
    const QVector3D shadow = vectorParam(params, "shadowColor", QVector3D(0.05f, 0.05f, 0.08f));
    const QVector3D highlight = vectorParam(params, "highlightColor", QVector3D(1.0f, 0.92f, 0.78f));
    const QVector3D toned = lerpColor(shadow, highlight, luma(color));
    return clampColor(lerpColor(color, toned, factor));
}

QVector3D applyAscCdlNode(const QVector3D& color, const QJsonObject& params)
{
    const QVector3D slope = vectorParam(params, "slope", QVector3D(1.0f, 1.0f, 1.0f));
    const QVector3D offset = vectorParam(params, "offset", QVector3D(0.0f, 0.0f, 0.0f));
    const QVector3D power = vectorParam(params, "power", QVector3D(1.0f, 1.0f, 1.0f));
    QVector3D c = color * slope + offset;
    c.setX(std::pow(clamp01(c.x()), 1.0f / std::max(0.05f, power.x())));
    c.setY(std::pow(clamp01(c.y()), 1.0f / std::max(0.05f, power.y())));
    c.setZ(std::pow(clamp01(c.z()), 1.0f / std::max(0.05f, power.z())));
    return clampColor(applySaturation(c, static_cast<float>(params.value("saturation").toDouble(1.0)), 0.0f));
}

QVector3D applyColorRampNode(const QVector3D& color, const QJsonObject& params)
{
    const float black = clamp01(static_cast<float>(params.value("blackPoint").toDouble(0.0)));
    const float white = clamp01(static_cast<float>(params.value("whitePoint").toDouble(1.0)));
    const float lo = std::min(black, white - 0.001f);
    const float hi = std::max(white, black + 0.001f);
    const float t = clamp01((luma(color) - lo) / std::max(0.0001f, hi - lo));
    return clampColor(lerpColor(vectorParam(params, "shadowColor", QVector3D(0.0f, 0.0f, 0.0f)),
                                vectorParam(params, "highlightColor", QVector3D(1.0f, 1.0f, 1.0f)),
                                t));
}

QVector3D applySelectiveColorNode(const QVector3D& color, const QJsonObject& params)
{
    float h = 0.0f;
    float s = 0.0f;
    float l = 0.0f;
    rgbToHsl(clampColor(color), &h, &s, &l);
    const float target = clamp01(static_cast<float>(params.value("targetHue").toDouble(0.0)));
    const float range = std::max(0.001f, static_cast<float>(params.value("range").toDouble(0.12)));
    const float raw = std::abs(h - target);
    const float distance = std::min(raw, 1.0f - raw);
    const float weight = clamp01(1.0f - distance / range);
    h = wrapHue(h + static_cast<float>(params.value("hue").toDouble(0.0)) / 360.0f * weight);
    s = clamp01(s * (1.0f + static_cast<float>(params.value("saturation").toDouble(0.0)) * weight));
    l = clamp01(l + static_cast<float>(params.value("luminance").toDouble(0.0)) * weight);
    return hslToRgb(h, s, l);
}

QVector3D applyLutMixNode(const QVector3D& color, const QJsonObject& params, const Lut3D* importedLut)
{
    const float strength = std::clamp(static_cast<float>(params.value("strength").toDouble(1.0)), 0.0f, 1.0f);
    if (!importedLut || !importedLut->isValid() || strength <= 0.0f) {
        return clampColor(color);
    }
    return clampColor(lerpColor(color, importedLut->sample(clampColor(color)), strength));
}

QVector3D applyNodeStage(const QVector3D& color, const ColorGradeStage& stage, const Lut3D* importedLut)
{
    const QString type = stage.type.trimmed();
    if (type == "mixColor") {
        return applyMixColorNode(color, stage.params, importedLut);
    }
    if (type == "brightnessContrast") {
        return applyBrightnessContrastNode(color, stage.params);
    }
    if (type == "hueSaturationValue") {
        return applyHueSaturationValueNode(color, stage.params);
    }
    if (type == "gammaNode") {
        return applyGammaNode(color, stage.params);
    }
    if (type == "colorBalance") {
        return applyColorBalanceNode(color, stage.params);
    }
    if (type == "rgbCurves") {
        return applyRgbCurvesNode(color, stage.params);
    }
    if (type == "colorCorrection") {
        return applyColorCorrectionNode(color, stage.params);
    }
    if (type == "invertColor") {
        return applyInvertNode(color, stage.params);
    }
    if (type == "posterizeColor") {
        return applyPosterizeNode(color, stage.params);
    }
    if (type == "clampColor") {
        return applyClampNode(color, stage.params);
    }
    if (type == "levelsColor") {
        return applyLevelsNode(color, stage.params);
    }
    if (type == "exposureColor") {
        return applyExposureNode(color, stage.params);
    }
    if (type == "thresholdColor") {
        return applyThresholdNode(color, stage.params);
    }
    if (type == "sepiaColor") {
        return applySepiaNode(color, stage.params);
    }
    if (type == "channelMixer") {
        return applyChannelMixerNode(color, stage.params);
    }
    if (type == "temperatureTint") {
        return applyTemperatureTintNode(color, stage.params);
    }
    if (type == "grayscaleColor") {
        return applyGrayscaleNode(color, stage.params);
    }
    if (type == "vibranceColor") {
        return applyVibranceNode(color, stage.params);
    }
    if (type == "softClipColor") {
        return applySoftClipNode(color, stage.params);
    }
    if (type == "duotoneColor") {
        return applyDuotoneNode(color, stage.params);
    }
    if (type == "ascCdl") {
        return applyAscCdlNode(color, stage.params);
    }
    if (type == "colorRamp") {
        return applyColorRampNode(color, stage.params);
    }
    if (type == "selectiveColor") {
        return applySelectiveColorNode(color, stage.params);
    }
    if (type == "lutMix") {
        return applyLutMixNode(color, stage.params, importedLut);
    }
    if (type == "parallelMixer") {
        return applyParallelMixerNode(color, stage.params, importedLut);
    }
    if (type == "layerMixer") {
        return applyLayerMixerNode(color, stage.params, importedLut);
    }

    ColorGradeParams params = paramsFromJson(stage.params);
    params.inputColorSpace = "srgb";
    if (type == "curveGrade") {
        ColorGradeParams curveOnly;
        curveOnly.masterCurve = params.masterCurve;
        curveOnly.redCurve = params.redCurve;
        curveOnly.greenCurve = params.greenCurve;
        curveOnly.blueCurve = params.blueCurve;
        curveOnly.masterCurveHandles = params.masterCurveHandles;
        curveOnly.redCurveHandles = params.redCurveHandles;
        curveOnly.greenCurveHandles = params.greenCurveHandles;
        curveOnly.blueCurveHandles = params.blueCurveHandles;
        return applyCurves(clampColor(color), curveOnly);
    }
    if (type == "hslGrade") {
        ColorGradeParams hslOnly;
        hslOnly.hslHue = params.hslHue;
        hslOnly.hslSaturation = params.hslSaturation;
        hslOnly.hslLuminance = params.hslLuminance;
        hslOnly.hueVsHueCurve = params.hueVsHueCurve;
        hslOnly.hueVsSaturationCurve = params.hueVsSaturationCurve;
        hslOnly.hueVsLuminanceCurve = params.hueVsLuminanceCurve;
        hslOnly.hueVsHueCurveHandles = params.hueVsHueCurveHandles;
        hslOnly.hueVsSaturationCurveHandles = params.hueVsSaturationCurveHandles;
        hslOnly.hueVsLuminanceCurveHandles = params.hueVsLuminanceCurveHandles;
        QVector3D c = applyHsl(clampColor(color), hslOnly);
        return applyHueVsCurves(clampColor(c), hslOnly);
    }
    if (type == "proGrade") {
        ColorGradeParams proOnly;
        proOnly.logShadow = params.logShadow;
        proOnly.logDark = params.logDark;
        proOnly.logLight = params.logLight;
        proOnly.logHighlight = params.logHighlight;
        proOnly.hdrShadow = params.hdrShadow;
        proOnly.hdrDark = params.hdrDark;
        proOnly.hdrLight = params.hdrLight;
        proOnly.hdrHighlight = params.hdrHighlight;
        proOnly.printerRed = params.printerRed;
        proOnly.printerGreen = params.printerGreen;
        proOnly.printerBlue = params.printerBlue;
        proOnly.skinToneHue = params.skinToneHue;
        proOnly.skinToneSaturation = params.skinToneSaturation;
        proOnly.skinToneLuminance = params.skinToneLuminance;
        QVector3D c = applyLogHdrWheels(color, proOnly);
        c = applyPrinterLights(c, proOnly);
        return applySkinToneTool(clampColor(c), proOnly);
    }
    return ColorPipeline::applyParams(color, params);
}

QVector3D blendStageResult(const QVector3D& before, const QVector3D& after, float opacity, const QString& blendMode)
{
    const float amount = std::clamp(opacity, 0.0f, 1.0f);
    const QString mode = blendMode.trimmed().toLower();
    QVector3D blended = after;
    if (mode == "color") {
        float beforeH = 0.0f;
        float beforeS = 0.0f;
        float beforeL = 0.0f;
        float afterH = 0.0f;
        float afterS = 0.0f;
        float afterL = 0.0f;
        rgbToHsl(clampColor(before), &beforeH, &beforeS, &beforeL);
        rgbToHsl(clampColor(after), &afterH, &afterS, &afterL);
        blended = hslToRgb(afterH, afterS, beforeL);
    } else if (mode == "luminosity") {
        float beforeH = 0.0f;
        float beforeS = 0.0f;
        float beforeL = 0.0f;
        float afterH = 0.0f;
        float afterS = 0.0f;
        float afterL = 0.0f;
        rgbToHsl(clampColor(before), &beforeH, &beforeS, &beforeL);
        rgbToHsl(clampColor(after), &afterH, &afterS, &afterL);
        blended = hslToRgb(beforeH, beforeS, afterL);
    }
    return clampColor(before * (1.0f - amount) + blended * amount);
}

bool pipelineHasSoloStage(const ColorGradePipeline& pipeline)
{
    return std::any_of(pipeline.stages.begin(), pipeline.stages.end(), [](const ColorGradeStage& stage) {
        return stage.enabled && stage.solo;
    });
}

const MaskAsset* findMaskAsset(const QVector<MaskAsset>& masks, const MaskReference& ref)
{
    if (ref.id.isEmpty()) {
        return nullptr;
    }
    const auto it = std::find_if(masks.begin(), masks.end(), [&](const MaskAsset& mask) {
        return mask.maskId == ref.id;
    });
    return it == masks.end() ? nullptr : &(*it);
}

bool isSpatialMask(const MaskAsset& mask)
{
    return mask.kind == MaskKind::Gradient
        || mask.kind == MaskKind::Radial
        || mask.kind == MaskKind::Paint
        || mask.kind == MaskKind::ExternalImage;
}

float exportableMaskValue(const QVector<MaskAsset>& masks, const MaskReference& ref, const QVector3D& color)
{
    if (ref.id.isEmpty()) {
        return 1.0f;
    }
    const MaskAsset* mask = findMaskAsset(masks, ref);
    if (!mask || !mask->enabled || mask->kind == MaskKind::Full) {
        return 1.0f;
    }
    if (isSpatialMask(*mask)) {
        return 0.0f;
    }
    return MaskEvaluator::evaluate(masks, ref, QPointF(0.5, 0.5), color);
}

QVector3D applyPipelineForLut(const QVector3D& color, const ColorGradePipeline& pipeline, const QVector<MaskAsset>& masks)
{
    if (pipeline.stages.isEmpty()) {
        return ColorPipeline::applyParams(color, pipeline.params);
    }

    QVector3D c = applyInputColorSpace(color, pipeline.params.inputColorSpace);
    const bool hasSolo = pipelineHasSoloStage(pipeline);
    for (const ColorGradeStage& stage : pipeline.stages) {
        if (!stage.enabled || (hasSolo && !stage.solo)) {
            continue;
        }
        const QVector3D before = c;
        const QVector3D after = applyNodeStage(before, stage);
        const float mask = exportableMaskValue(masks, stage.maskRef, before);
        const float opacity = std::clamp(stage.opacity * mask, 0.0f, 1.0f);
        c = blendStageResult(before, after, opacity, stage.blendMode);
    }
    return clampColor(c);
}

} // namespace

QVector3D ColorPipeline::applyParams(const QVector3D& color, const ColorGradeParams& params)
{
    QVector3D c = applyInputColorSpace(color, params.inputColorSpace) * std::pow(2.0f, params.exposure);

    c = applyPrinterLights(c, params);
    const float contrastFactor = std::max(0.0f, 1.0f + params.contrast);
    c = (c - QVector3D(0.5f, 0.5f, 0.5f)) * contrastFactor + QVector3D(0.5f, 0.5f, 0.5f);
    c = applyToneRanges(c, params);
    c = applyLogHdrWheels(c, params);

    c += QVector3D(params.temperature * 0.08f, params.tint * 0.05f, -params.temperature * 0.08f);
    c = (c + params.lift + params.offset) * params.gain;

    c.setX(std::pow(clamp01(c.x()), 1.0f / std::max(0.05f, params.gamma.x())));
    c.setY(std::pow(clamp01(c.y()), 1.0f / std::max(0.05f, params.gamma.y())));
    c.setZ(std::pow(clamp01(c.z()), 1.0f / std::max(0.05f, params.gamma.z())));

    c = applyPresence(c, params);
    c = applySaturation(c, params.saturation, params.vibrance);
    c = applyCalibration(clampColor(c), params);
    c = applyColorWarp(clampColor(c), params);
    c = applyHsl(clampColor(c), params);
    c = applyHueVsCurves(clampColor(c), params);
    c = applySkinToneTool(clampColor(c), params);
    c = applyCurves(clampColor(c), params);
    c = applySoftClip(clampColor(c), params);
    return clampColor(c);
}

QVector3D ColorPipeline::applyPipeline(const QVector3D& color, const ColorGradePipeline& pipeline)
{
    return applyPipeline(color, pipeline, {}, QPointF());
}

QVector3D ColorPipeline::applyPipeline(const QVector3D& color, const ColorGradePipeline& pipeline, const QVector<MaskAsset>& masks, const QPointF& uv)
{
    return applyPipeline(color, pipeline, masks, uv, nullptr);
}

QVector3D ColorPipeline::applyPipeline(const QVector3D& color, const ColorGradePipeline& pipeline, const QVector<MaskAsset>& masks, const QPointF& uv, const Lut3D* importedLut)
{
    if (pipeline.stages.isEmpty()) {
        return applyParams(color, pipeline.params);
    }

    QVector3D c = applyInputColorSpace(color, pipeline.params.inputColorSpace);
    const bool hasSolo = pipelineHasSoloStage(pipeline);
    for (const ColorGradeStage& stage : pipeline.stages) {
        if (!stage.enabled || (hasSolo && !stage.solo)) {
            continue;
        }
        const QVector3D before = c;
        const QVector3D after = applyNodeStage(before, stage, importedLut);
        const float mask = MaskEvaluator::evaluate(masks, stage.maskRef, uv, before);
        const float opacity = std::clamp(stage.opacity * mask, 0.0f, 1.0f);
        c = blendStageResult(before, after, opacity, stage.blendMode);
    }
    return clampColor(c);
}

QVector3D ColorPipeline::applyTransform(const QVector3D& color, const ColorTransformSource& source, const Lut3D* importedLut)
{
    const QVector3D graded = source.usePipeline
        ? applyPipeline(color, source.pipeline, source.masks, QPointF(), importedLut)
        : applyParams(color, source.params);
    const float strength = std::clamp(source.importedLutStrength, 0.0f, 1.0f);
    if (source.kind == ColorTransformKind::ImportedLut && importedLut && importedLut->isValid() && strength > 0.0f) {
        const QVector3D lutted = importedLut->sample(graded);
        return clampColor(graded * (1.0f - strength) + lutted * strength);
    }
    return graded;
}

Lut3D ColorPipeline::bakeLut(const ColorGradeParams& params, int size)
{
    Lut3D lut(size);
    if (!lut.isValid()) {
        return lut;
    }

    const float denom = static_cast<float>(size - 1);
    const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    const int workerCount = std::max(1, std::min(size, static_cast<int>(hardwareThreads)));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(workerCount));
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&, worker]() {
            for (int b = worker; b < size; b += workerCount) {
                for (int g = 0; g < size; ++g) {
                    for (int r = 0; r < size; ++r) {
                        lut.setValue(r, g, b, applyParams(QVector3D(r / denom, g / denom, b / denom), params));
                    }
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    return lut;
}

Lut3D ColorPipeline::bakePipelineLut(const ColorGradePipeline& pipeline, int size)
{
    Lut3D lut(size);
    if (!lut.isValid()) {
        return lut;
    }

    const float denom = static_cast<float>(size - 1);
    const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    const int workerCount = std::max(1, std::min(size, static_cast<int>(hardwareThreads)));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(workerCount));
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&, worker]() {
            for (int b = worker; b < size; b += workerCount) {
                for (int g = 0; g < size; ++g) {
                    for (int r = 0; r < size; ++r) {
                        lut.setValue(r, g, b, applyPipeline(QVector3D(r / denom, g / denom, b / denom), pipeline));
                    }
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    return lut;
}

Lut3D ColorPipeline::bakePipelineLut(const ColorGradePipeline& pipeline, const QVector<MaskAsset>& masks, int size)
{
    Lut3D lut(size);
    if (!lut.isValid()) {
        return lut;
    }

    const float denom = static_cast<float>(size - 1);
    const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    const int workerCount = std::max(1, std::min(size, static_cast<int>(hardwareThreads)));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(workerCount));
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&, worker]() {
            for (int b = worker; b < size; b += workerCount) {
                for (int g = 0; g < size; ++g) {
                    for (int r = 0; r < size; ++r) {
                        lut.setValue(r, g, b, applyPipelineForLut(QVector3D(r / denom, g / denom, b / denom), pipeline, masks));
                    }
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    return lut;
}

Lut3D ColorPipeline::composeLut(const ColorGradeParams& params, const Lut3D& importedLut, float importedLutStrength, int size)
{
    Lut3D lut(size);
    if (!lut.isValid()) {
        return lut;
    }

    ColorTransformSource source;
    source.kind = importedLut.isValid() && importedLutStrength > 0.0f
        ? ColorTransformKind::ImportedLut
        : ColorTransformKind::ParamPipeline;
    source.params = params;
    source.importedLutStrength = importedLutStrength;

    const float denom = static_cast<float>(size - 1);
    const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    const int workerCount = std::max(1, std::min(size, static_cast<int>(hardwareThreads)));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(workerCount));
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&, worker]() {
            for (int b = worker; b < size; b += workerCount) {
                for (int g = 0; g < size; ++g) {
                    for (int r = 0; r < size; ++r) {
                        lut.setValue(r, g, b, applyTransform(QVector3D(r / denom, g / denom, b / denom), source, &importedLut));
                    }
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    return lut;
}

Lut3D ColorPipeline::composePipelineLut(const ColorGradePipeline& pipeline, const Lut3D& importedLut, float importedLutStrength, int size)
{
    Lut3D lut(size);
    if (!lut.isValid()) {
        return lut;
    }

    ColorTransformSource source;
    source.kind = importedLut.isValid() && importedLutStrength > 0.0f
        ? ColorTransformKind::ImportedLut
        : ColorTransformKind::ParamPipeline;
    source.usePipeline = true;
    source.pipeline = pipeline;
    source.importedLutStrength = importedLutStrength;

    const float denom = static_cast<float>(size - 1);
    const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    const int workerCount = std::max(1, std::min(size, static_cast<int>(hardwareThreads)));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(workerCount));
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&, worker]() {
            for (int b = worker; b < size; b += workerCount) {
                for (int g = 0; g < size; ++g) {
                    for (int r = 0; r < size; ++r) {
                        lut.setValue(r, g, b, applyTransform(QVector3D(r / denom, g / denom, b / denom), source, &importedLut));
                    }
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    return lut;
}

Lut3D ColorPipeline::composePipelineLut(const ColorGradePipeline& pipeline, const QVector<MaskAsset>& masks, const Lut3D& importedLut, float importedLutStrength, int size)
{
    Lut3D lut(size);
    if (!lut.isValid()) {
        return lut;
    }

    const float strength = std::clamp(importedLutStrength, 0.0f, 1.0f);
    const float denom = static_cast<float>(size - 1);
    const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    const int workerCount = std::max(1, std::min(size, static_cast<int>(hardwareThreads)));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(workerCount));
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&, worker]() {
            for (int b = worker; b < size; b += workerCount) {
                for (int g = 0; g < size; ++g) {
                    for (int r = 0; r < size; ++r) {
                        const QVector3D input(r / denom, g / denom, b / denom);
                        const QVector3D graded = applyPipelineForLut(input, pipeline, masks);
                        if (importedLut.isValid() && strength > 0.0f) {
                            const QVector3D lutted = importedLut.sample(graded);
                            lut.setValue(r, g, b, clampColor(graded * (1.0f - strength) + lutted * strength));
                        } else {
                            lut.setValue(r, g, b, graded);
                        }
                    }
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    return lut;
}

QImage ColorPipeline::applyToImage(const QImage& input, const ColorTransformSource& source, const Lut3D* importedLut)
{
    if (input.isNull()) {
        return QImage();
    }

    QImage src = input.convertToFormat(QImage::Format_ARGB32);
    QImage out(src.size(), QImage::Format_ARGB32);

    for (int y = 0; y < src.height(); ++y) {
        const QRgb* inLine = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        QRgb* outLine = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            const QRgb p = inLine[x];
            const QVector3D c = applyTransform(QVector3D(qRed(p) / 255.0f, qGreen(p) / 255.0f, qBlue(p) / 255.0f), source, importedLut);
            outLine[x] = qRgba(static_cast<int>(clamp01(c.x()) * 255.0f + 0.5f),
                               static_cast<int>(clamp01(c.y()) * 255.0f + 0.5f),
                               static_cast<int>(clamp01(c.z()) * 255.0f + 0.5f),
                               qAlpha(p));
        }
    }

    return out;
}

QImage ColorPipeline::applyLutToImage(const QImage& input, const Lut3D& lut)
{
    if (input.isNull()) {
        return QImage();
    }
    if (!lut.isValid()) {
        return input.convertToFormat(QImage::Format_ARGB32);
    }

    QImage src = input.convertToFormat(QImage::Format_ARGB32);
    QImage out(src.size(), QImage::Format_ARGB32);
    for (int y = 0; y < src.height(); ++y) {
        const QRgb* inLine = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        QRgb* outLine = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            const QRgb p = inLine[x];
            const QVector3D c = lut.sample(QVector3D(qRed(p) / 255.0f, qGreen(p) / 255.0f, qBlue(p) / 255.0f));
            outLine[x] = qRgba(static_cast<int>(clamp01(c.x()) * 255.0f + 0.5f),
                               static_cast<int>(clamp01(c.y()) * 255.0f + 0.5f),
                               static_cast<int>(clamp01(c.z()) * 255.0f + 0.5f),
                               qAlpha(p));
        }
    }
    return out;
}

} // namespace ikclut
