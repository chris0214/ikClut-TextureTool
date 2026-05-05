#include "render/ScopeRenderer.h"

#include <QtMath>

namespace ikclut {

QPointF ScopeRenderer::rec709CodePoint(double uCode, double vCode)
{
    return QPointF((uCode - 128.0) / 112.0, (128.0 - vCode) / 112.0);
}

ScopeData ScopeRenderer::compute(const QImage& image, int maxSamples)
{
    ScopeData data;
    data.luminanceHistogram.fill(0, 256);
    data.redHistogram.fill(0, 256);
    data.greenHistogram.fill(0, 256);
    data.blueHistogram.fill(0, 256);
    data.exposureHistogram.fill(0, 120);
    data.exposureZoneCounts.fill(0, 6);

    if (image.isNull()) {
        return data;
    }

    const QImage src = image.convertToFormat(QImage::Format_ARGB32);
    const int total = src.width() * src.height();
    const int stride = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(std::max(1, total / std::max(1, maxSamples))))));

    double exposureTotal = 0.0;
    int lowExposurePixels = 0;
    int highExposurePixels = 0;

    for (int y = 0; y < src.height(); y += stride) {
        const QRgb* line = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        for (int x = 0; x < src.width(); x += stride) {
            const QRgb p = line[x];
            const int r = qRed(p);
            const int g = qGreen(p);
            const int b = qBlue(p);
            const int lum = std::clamp(static_cast<int>(r * 0.2126 + g * 0.7152 + b * 0.0722), 0, 255);
            ++data.luminanceHistogram[lum];
            ++data.redHistogram[r];
            ++data.greenHistogram[g];
            ++data.blueHistogram[b];
            ++data.sampledPixels;

            const double exposureValue = std::clamp(lum / 255.0 * 5.0, 0.0, 5.0);
            const int exposureBin = std::clamp(static_cast<int>(std::round(exposureValue / 5.0 * (data.exposureHistogram.size() - 1))),
                                               0,
                                               static_cast<int>(data.exposureHistogram.size() - 1));
            const int exposureZone = std::clamp(static_cast<int>(std::floor(exposureValue + 0.0001)), 0, 5);
            ++data.exposureHistogram[exposureBin];
            ++data.exposureZoneCounts[exposureZone];
            exposureTotal += exposureValue;
            if (exposureValue < 1.0) {
                ++lowExposurePixels;
            }
            if (exposureValue >= 4.0) {
                ++highExposurePixels;
            }

            if (r <= 1 || g <= 1 || b <= 1 || lum <= 1) {
                ++data.shadowClipPixels;
            }
            if (r >= 254 || g >= 254 || b >= 254 || lum >= 254) {
                ++data.highlightClipPixels;
            }
            if ((std::max({r, g, b}) >= 250 && std::min({r, g, b}) <= 5)
                || std::abs(r - g) + std::abs(g - b) + std::abs(b - r) > 430) {
                ++data.gamutWarningPixels;
            }

            if (data.vectorscope.size() < 12000) {
                const double rr = r / 255.0;
                const double gg = g / 255.0;
                const double bb = b / 255.0;
                const double yPrime = rr * 0.2126 + gg * 0.7152 + bb * 0.0722;
                const double cb = (bb - yPrime) / (2.0 * (1.0 - 0.0722));
                const double cr = (rr - yPrime) / (2.0 * (1.0 - 0.2126));
                const double uCode = 128.0 + 224.0 * cb;
                const double vCode = 128.0 + 224.0 * cr;
                data.vectorscope.append(rec709CodePoint(uCode, vCode));
            }
            if (data.redWaveform.size() < 16000) {
                const double nx = src.width() <= 1 ? 0.0 : x / static_cast<double>(src.width() - 1);
                data.redWaveform.append(QPointF(nx, r / 255.0));
                data.greenWaveform.append(QPointF(nx, g / 255.0));
                data.blueWaveform.append(QPointF(nx, b / 255.0));
            }
        }
    }

    if (data.sampledPixels > 0) {
        data.exposureMean = exposureTotal / data.sampledPixels;
        data.exposureLowPercent = 100.0 * lowExposurePixels / data.sampledPixels;
        data.exposureHighPercent = 100.0 * highExposurePixels / data.sampledPixels;
    }

    return data;
}

} // namespace ikclut
