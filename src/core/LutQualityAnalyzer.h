#pragma once

#include "core/Lut3D.h"
#include "model/ColorTypes.h"

#include <QString>
#include <QStringList>

namespace ikclut {

struct LutQualityReport {
    bool ok = false;
    int size = 0;
    int sampleCount = 0;
    int warningCount = 0;
    double clippingRatio = 0.0;
    double nonMonotonicRatio = 0.0;
    double maxStep = 0.0;
    double averageStep = 0.0;
    double maxIdentityShift = 0.0;
    int distinctRed = 0;
    int distinctGreen = 0;
    int distinctBlue = 0;
    bool hasSpatialMasks = false;
    bool usesImportedLut = false;
    bool hasStrongCurves = false;
    QString exportFormat;
    QString outputPath;
    QStringList warnings;

    QString summaryText() const;
};

class LutQualityAnalyzer {
public:
    static LutQualityReport analyze(const Lut3D& lut);
    static LutQualityReport analyzeExport(const Lut3D& lut,
                                          const ImageDocument& document,
                                          bool usesImportedLut,
                                          const QString& exportFormat,
                                          const QString& outputPath = QString());
};

} // namespace ikclut
