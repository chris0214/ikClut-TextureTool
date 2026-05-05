#pragma once

#include "core/Lut3D.h"
#include "model/ColorTypes.h"

#include <QString>

namespace ikclut {

struct ExportResult {
    bool ok = false;
    QString error;
    QString diagnostic;
    QSize imageSize;
};

struct ExportValidationResult {
    bool ok = false;
    QString error;
    QSize imageSize;
    bool identityCornersOk = false;
};

class IkClutExporter {
public:
    static constexpr int IkClutSize = 32;
    static constexpr int IkClutWidth = IkClutSize * IkClutSize;
    static constexpr int IkClutHeight = IkClutSize;

    static Lut3D resolveExportLut(const ColorTransformSource& source, const ColorGradeParams& params, const Lut3D& importedLut);
    static QImage makeImage(const Lut3D& lut);
    static ExportValidationResult validateImage(const QImage& image);
    static ExportResult savePng(const Lut3D& lut, const QString& path);
    static ExportResult saveCube(const Lut3D& lut, const QString& path);
    static ExportResult saveHaldPng(const Lut3D& lut, const QString& path);
};

} // namespace ikclut
