#include "core/LutQualityAnalyzer.h"

#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace ikclut {

namespace {

double distance3(const QVector3D& a, const QVector3D& b)
{
    const QVector3D d = a - b;
    return std::sqrt(d.x() * d.x() + d.y() * d.y() + d.z() * d.z());
}

int quantizedChannel(float value)
{
    return std::clamp(static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)), 0, 255);
}

bool clipsChannel(float value)
{
    return value <= 0.002f || value >= 0.998f;
}

bool isSpatialMaskKind(MaskKind kind)
{
    return kind == MaskKind::Gradient
        || kind == MaskKind::Radial
        || kind == MaskKind::Paint
        || kind == MaskKind::ExternalImage;
}

bool curveIsStrong(const QVector<QPointF>& curve)
{
    for (const QPointF& point : curve) {
        if (std::abs(point.y() - point.x()) > 0.28) {
            return true;
        }
    }
    return false;
}

bool paramsHaveStrongCurves(const ColorGradeParams& params)
{
    return curveIsStrong(params.masterCurve)
        || curveIsStrong(params.redCurve)
        || curveIsStrong(params.greenCurve)
        || curveIsStrong(params.blueCurve);
}

} // namespace

QString LutQualityReport::summaryText() const
{
    QString text;
    QTextStream out(&text);
    out << "Export Quality Report\n";
    if (!exportFormat.isEmpty()) {
        out << "Format: " << exportFormat << "\n";
    }
    if (!outputPath.isEmpty()) {
        out << "Path: " << outputPath << "\n";
    }
    out << "Size: " << size << "^3 (" << sampleCount << " samples)\n";
    out << "Clipping: " << QString::number(clippingRatio * 100.0, 'f', 2) << "%\n";
    out << "Non-monotonic steps: " << QString::number(nonMonotonicRatio * 100.0, 'f', 2) << "%\n";
    out << "Max adjacent step: " << QString::number(maxStep, 'f', 4) << "\n";
    out << "Average adjacent step: " << QString::number(averageStep, 'f', 4) << "\n";
    out << "Max identity shift: " << QString::number(maxIdentityShift, 'f', 4) << "\n";
    out << "Distinct RGB levels: R " << distinctRed << ", G " << distinctGreen << ", B " << distinctBlue << "\n\n";
    out << "Project checks:\n";
    out << "- Spatial masks ignored by LUT export: " << (hasSpatialMasks ? "Yes" : "No") << "\n";
    out << "- Imported LUT in chain: " << (usesImportedLut ? "Yes" : "No") << "\n";
    out << "- Strong curve shaping: " << (hasStrongCurves ? "Yes" : "No") << "\n\n";

    if (warnings.isEmpty()) {
        out << "No major quality risks detected.";
    } else {
        out << "Warnings:\n";
        for (const QString& warning : warnings) {
            out << "- " << warning << "\n";
        }
    }
    return text;
}

LutQualityReport LutQualityAnalyzer::analyze(const Lut3D& lut)
{
    LutQualityReport report;
    report.size = lut.size();
    if (!lut.isValid()) {
        report.warnings.append("LUT is invalid or empty.");
        report.warningCount = report.warnings.size();
        return report;
    }

    const int size = lut.size();
    const double expectedStep = 1.0 / std::max(1, size - 1);
    const double monotonicTolerance = std::max(0.004, expectedStep * 0.45);
    QSet<int> redLevels;
    QSet<int> greenLevels;
    QSet<int> blueLevels;
    int clipped = 0;
    int monotonicChecks = 0;
    int nonMonotonic = 0;
    int stepChecks = 0;
    double stepSum = 0.0;

    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                const QVector3D value = lut.value(r, g, b);
                redLevels.insert(quantizedChannel(value.x()));
                greenLevels.insert(quantizedChannel(value.y()));
                blueLevels.insert(quantizedChannel(value.z()));
                ++report.sampleCount;
                if (clipsChannel(value.x()) || clipsChannel(value.y()) || clipsChannel(value.z())) {
                    ++clipped;
                }

                const QVector3D identity(r / static_cast<float>(size - 1),
                                         g / static_cast<float>(size - 1),
                                         b / static_cast<float>(size - 1));
                report.maxIdentityShift = std::max(report.maxIdentityShift, distance3(value, identity));

                if (r > 0) {
                    const QVector3D previous = lut.value(r - 1, g, b);
                    const double step = distance3(value, previous);
                    report.maxStep = std::max(report.maxStep, step);
                    stepSum += step;
                    ++stepChecks;
                    ++monotonicChecks;
                    if (value.x() + monotonicTolerance < previous.x()) {
                        ++nonMonotonic;
                    }
                }
                if (g > 0) {
                    const QVector3D previous = lut.value(r, g - 1, b);
                    const double step = distance3(value, previous);
                    report.maxStep = std::max(report.maxStep, step);
                    stepSum += step;
                    ++stepChecks;
                    ++monotonicChecks;
                    if (value.y() + monotonicTolerance < previous.y()) {
                        ++nonMonotonic;
                    }
                }
                if (b > 0) {
                    const QVector3D previous = lut.value(r, g, b - 1);
                    const double step = distance3(value, previous);
                    report.maxStep = std::max(report.maxStep, step);
                    stepSum += step;
                    ++stepChecks;
                    ++monotonicChecks;
                    if (value.z() + monotonicTolerance < previous.z()) {
                        ++nonMonotonic;
                    }
                }
            }
        }
    }

    report.ok = true;
    report.clippingRatio = report.sampleCount > 0 ? clipped / static_cast<double>(report.sampleCount) : 0.0;
    report.nonMonotonicRatio = monotonicChecks > 0 ? nonMonotonic / static_cast<double>(monotonicChecks) : 0.0;
    report.averageStep = stepChecks > 0 ? stepSum / static_cast<double>(stepChecks) : 0.0;
    report.distinctRed = redLevels.size();
    report.distinctGreen = greenLevels.size();
    report.distinctBlue = blueLevels.size();

    if (report.clippingRatio > 0.18) {
        report.warnings.append("Large portions of the LUT clamp to pure black or white; exported looks may crush shadows or highlights.");
    }
    if (report.nonMonotonicRatio > 0.02) {
        report.warnings.append("Primary-channel monotonicity breaks are present; this can cause hue reversals or unstable gradients.");
    }
    if (report.maxStep > expectedStep * 6.0) {
        report.warnings.append("Large adjacent jumps were found; this may show as banding, posterization, or hard color edges.");
    }
    if (std::min({report.distinctRed, report.distinctGreen, report.distinctBlue}) < 24) {
        report.warnings.append("One or more channels use very few output levels; posterization risk is high.");
    }
    if (report.maxIdentityShift > 0.85) {
        report.warnings.append("The LUT contains very extreme color shifts; verify it is intentional before shipping.");
    }
    report.warningCount = report.warnings.size();
    return report;
}

LutQualityReport LutQualityAnalyzer::analyzeExport(const Lut3D& lut,
                                                   const ImageDocument& document,
                                                   bool usesImportedLut,
                                                   const QString& exportFormat,
                                                   const QString& outputPath)
{
    LutQualityReport report = analyze(lut);
    report.exportFormat = exportFormat;
    report.outputPath = outputPath;
    report.usesImportedLut = usesImportedLut;
    report.hasStrongCurves = paramsHaveStrongCurves(document.pipeline.params);
    for (const ColorGradeStage& stage : document.pipeline.stages) {
        if (paramsHaveStrongCurves(paramsFromJson(stage.params))) {
            report.hasStrongCurves = true;
        }
        if (stage.maskRef.id.isEmpty()) {
            continue;
        }
        const auto maskIt = std::find_if(document.masks.begin(), document.masks.end(), [&](const MaskAsset& mask) {
            return mask.maskId == stage.maskRef.id;
        });
        if (maskIt != document.masks.end() && maskIt->enabled && isSpatialMaskKind(maskIt->kind)) {
            report.hasSpatialMasks = true;
        }
    }

    if (report.hasSpatialMasks) {
        report.warnings.append("One or more spatial masks are project-only and cannot be represented in a 3D LUT export.");
    }
    if (report.usesImportedLut) {
        report.warnings.append("An imported LUT is baked into this export; verify licensing and cumulative clipping.");
    }
    if (report.hasStrongCurves) {
        report.warnings.append("Strong curve shaping is present; inspect gradients for banding before delivery.");
    }
    report.warningCount = report.warnings.size();
    return report;
}

} // namespace ikclut
