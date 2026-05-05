#include "core/IkClutExporter.h"

#include "core/ColorPipeline.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace ikclut {

namespace {

bool nearChannel(int actual, int expected, int tolerance = 2)
{
    return std::abs(actual - expected) <= tolerance;
}

int haldLevelForLutSize(int size)
{
    const int level = static_cast<int>(std::lround(std::sqrt(size)));
    return level * level == size ? level : 0;
}

int quantize8(float v)
{
    return std::clamp(static_cast<int>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)), 0, 255);
}

} // namespace

Lut3D IkClutExporter::resolveExportLut(const ColorTransformSource& source, const ColorGradeParams& params, const Lut3D& importedLut)
{
    if (source.kind == ColorTransformKind::ImportedLut && importedLut.isValid()) {
        if (source.usePipeline) {
            return ColorPipeline::composePipelineLut(source.pipeline, source.masks, importedLut, source.importedLutStrength, 64);
        }
        return ColorPipeline::composeLut(params, importedLut, source.importedLutStrength, 64);
    }
    if (source.usePipeline) {
        return ColorPipeline::bakePipelineLut(source.pipeline, source.masks, 64);
    }
    return ColorPipeline::bakeLut(params, 64);
}

QImage IkClutExporter::makeImage(const Lut3D& lut)
{
    if (!lut.isValid()) {
        return QImage();
    }
    return lut.resampled(IkClutSize).toIkClutImage();
}

ExportValidationResult IkClutExporter::validateImage(const QImage& image)
{
    ExportValidationResult result;
    if (image.isNull()) {
        result.error = "Export image is null.";
        return result;
    }
    result.imageSize = image.size();
    if (image.size() != QSize(IkClutWidth, IkClutHeight)) {
        result.error = QString("Invalid ikClut image size %1x%2. Expected %3x%4.")
            .arg(image.width())
            .arg(image.height())
            .arg(IkClutWidth)
            .arg(IkClutHeight);
        return result;
    }

    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    const QRgb black = rgba.pixel(0, 0);
    const QRgb red = rgba.pixel(IkClutSize - 1, 0);
    const QRgb green = rgba.pixel(0, IkClutSize - 1);
    const QRgb blue = rgba.pixel(IkClutSize * (IkClutSize - 1), 0);
    result.identityCornersOk =
        nearChannel(qRed(black), 0) && nearChannel(qGreen(black), 0) && nearChannel(qBlue(black), 0) &&
        nearChannel(qRed(red), 255) &&
        nearChannel(qGreen(green), 255) &&
        nearChannel(qBlue(blue), 255);
    result.ok = true;
    return result;
}

ExportResult IkClutExporter::savePng(const Lut3D& lut, const QString& path)
{
    ExportResult result;
    const QImage image = makeImage(lut);
    const ExportValidationResult validation = validateImage(image);
    if (!validation.ok) {
        result.error = validation.error;
        return result;
    }
    if (!image.save(path, "PNG")) {
        result.error = "Failed to write PNG file.";
        return result;
    }
    result.ok = true;
    result.imageSize = image.size();
    result.diagnostic = validation.identityCornersOk
        ? "Export validation passed; identity layout checkpoints match."
        : "Export validation passed; non-identity LUT layout size is valid.";
    return result;
}

ExportResult IkClutExporter::saveCube(const Lut3D& lut, const QString& path)
{
    ExportResult result;
    if (!lut.isValid()) {
        result.error = "Export LUT is invalid.";
        return result;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        result.error = "Failed to write CUBE file.";
        return result;
    }

    QTextStream out(&file);
    out.setRealNumberNotation(QTextStream::FixedNotation);
    out.setRealNumberPrecision(7);
    QString title = QFileInfo(path).completeBaseName();
    title.replace('"', '\'');
    out << "TITLE \"" << title << "\"\n";
    out << "LUT_3D_SIZE " << lut.size() << "\n";
    out << "DOMAIN_MIN 0.0 0.0 0.0\n";
    out << "DOMAIN_MAX 1.0 1.0 1.0\n";
    for (int b = 0; b < lut.size(); ++b) {
        for (int g = 0; g < lut.size(); ++g) {
            for (int r = 0; r < lut.size(); ++r) {
                const QVector3D value = lut.value(r, g, b);
                out << value.x() << " " << value.y() << " " << value.z() << "\n";
            }
        }
    }

    result.ok = true;
    result.imageSize = QSize(lut.size(), lut.size());
    result.diagnostic = QString("Exported %1^3 CUBE LUT.").arg(lut.size());
    return result;
}

ExportResult IkClutExporter::saveHaldPng(const Lut3D& lut, const QString& path)
{
    ExportResult result;
    if (!lut.isValid()) {
        result.error = "Export LUT is invalid.";
        return result;
    }

    const Lut3D haldLut = haldLevelForLutSize(lut.size()) > 0 ? lut : lut.resampled(64);
    const int size = haldLut.size();
    const int level = haldLevelForLutSize(size);
    if (level <= 0) {
        result.error = "Hald export requires a square LUT size such as 16 or 64.";
        return result;
    }

    const int side = level * level * level;
    QImage image(side, side, QImage::Format_ARGB32);
    const int expectedPixels = size * size * size;
    for (int i = 0; i < expectedPixels; ++i) {
        const int x = i % side;
        const int y = i / side;
        const int r = i % size;
        const int g = (i / size) % size;
        const int b = i / (size * size);
        const QVector3D value = haldLut.value(r, g, b);
        image.setPixel(x, y, qRgba(quantize8(value.x()), quantize8(value.y()), quantize8(value.z()), 255));
    }

    if (!image.save(path, "PNG")) {
        result.error = "Failed to write Hald PNG file.";
        return result;
    }
    result.ok = true;
    result.imageSize = image.size();
    result.diagnostic = QString("Exported Hald level %1 PNG (%2^3 LUT, %3x%3).")
        .arg(level)
        .arg(size)
        .arg(side);
    return result;
}

} // namespace ikclut
