#pragma once

#include <QImage>
#include <QVector>
#include <QPointF>

namespace ikclut {

struct ScopeData {
    QVector<int> luminanceHistogram;
    QVector<int> redHistogram;
    QVector<int> greenHistogram;
    QVector<int> blueHistogram;
    QVector<int> exposureHistogram;
    QVector<int> exposureZoneCounts;
    QVector<QPointF> vectorscope;
    QVector<QPointF> redWaveform;
    QVector<QPointF> greenWaveform;
    QVector<QPointF> blueWaveform;
    int sampledPixels = 0;
    int shadowClipPixels = 0;
    int highlightClipPixels = 0;
    int gamutWarningPixels = 0;
    double exposureMean = 0.0;
    double exposureLowPercent = 0.0;
    double exposureHighPercent = 0.0;
};

class ScopeRenderer {
public:
    static ScopeData compute(const QImage& image, int maxSamples = 80000);
    static QPointF rec709CodePoint(double uCode, double vCode);
};

} // namespace ikclut
