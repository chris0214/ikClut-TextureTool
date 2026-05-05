#include "core/LutImportService.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace ikclut {

namespace {

QStringList tokensForLine(QString line)
{
    const int comment = line.indexOf('#');
    if (comment >= 0) {
        line.truncate(comment);
    }
    return line.simplified().split(' ', Qt::SkipEmptyParts);
}

bool parseFloatToken(const QString& token, float* out)
{
    bool ok = false;
    const float value = token.toFloat(&ok);
    if (ok && std::isfinite(value)) {
        *out = value;
        return true;
    }
    return false;
}

int haldLevelFromSide(int side)
{
    for (int level = 2; level <= 16; ++level) {
        if (level * level * level == side) {
            return level;
        }
    }
    return 0;
}

float sample1DChannel(const QVector<QVector3D>& values, float input, int channel)
{
    if (values.isEmpty()) {
        return input;
    }
    const int lastIndex = static_cast<int>(values.size()) - 1;
    const float position = std::clamp(input, 0.0f, 1.0f) * static_cast<float>(values.size() - 1);
    const int low = std::clamp(static_cast<int>(std::floor(position)), 0, lastIndex);
    const int high = std::clamp(low + 1, 0, lastIndex);
    const float t = position - static_cast<float>(low);
    const auto component = [channel](const QVector3D& value) {
        return channel == 0 ? value.x() : channel == 1 ? value.y() : value.z();
    };
    return component(values.at(low)) * (1.0f - t) + component(values.at(high)) * t;
}

} // namespace

LutImportResult LutImportService::importFile(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "cube") {
        return importCube(path);
    }
    if (suffix == "png") {
        return importHaldPng(path);
    }

    LutImportResult result;
    result.error = "Unsupported LUT file extension. Supported formats are .cube and Hald PNG.";
    return result;
}

LutImportResult LutImportService::importCube(const QString& path)
{
    LutImportResult result;
    result.format = "cube";

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = "Failed to open CUBE file.";
        return result;
    }

    int lut3DSize = 0;
    int lut1DSize = 0;
    QVector<QVector3D> values;
    QTextStream stream(&file);
    int lineNumber = 0;

    while (!stream.atEnd()) {
        ++lineNumber;
        const QString rawLine = stream.readLine();
        const QStringList parts = tokensForLine(rawLine);
        if (parts.isEmpty()) {
            continue;
        }

        const QString head = parts.first().toUpper();
        if (head == "TITLE" || head == "DOMAIN_MIN" || head == "DOMAIN_MAX") {
            continue;
        }
        if (head == "LUT_1D_INPUT_RANGE") {
            continue;
        }
        if (head == "LUT_1D_SIZE") {
            if (parts.size() != 2) {
                result.error = QString("Invalid LUT_1D_SIZE at line %1.").arg(lineNumber);
                return result;
            }
            bool ok = false;
            lut1DSize = parts.at(1).toInt(&ok);
            if (!ok || lut1DSize < 2 || lut1DSize > 65536) {
                result.error = QString("Unsupported 1D CUBE size at line %1.").arg(lineNumber);
                return result;
            }
            if (lut3DSize > 0) {
                result.error = "Mixed 1D+3D CUBE files are not supported in this version.";
                return result;
            }
            continue;
        }
        if (head == "LUT_3D_SIZE") {
            if (parts.size() != 2) {
                result.error = QString("Invalid LUT_3D_SIZE at line %1.").arg(lineNumber);
                return result;
            }
            bool ok = false;
            lut3DSize = parts.at(1).toInt(&ok);
            if (!ok || lut3DSize < 2 || lut3DSize > 256) {
                result.error = QString("Unsupported CUBE size at line %1.").arg(lineNumber);
                return result;
            }
            if (lut1DSize > 0) {
                result.error = "Mixed 1D+3D CUBE files are not supported in this version.";
                return result;
            }
            continue;
        }

        if (parts.size() != 3) {
            result.error = QString("Unexpected CUBE data at line %1.").arg(lineNumber);
            return result;
        }

        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        if (!parseFloatToken(parts.at(0), &r) ||
            !parseFloatToken(parts.at(1), &g) ||
            !parseFloatToken(parts.at(2), &b)) {
            result.error = QString("Invalid numeric CUBE value at line %1.").arg(lineNumber);
            return result;
        }
        values.append(QVector3D(r, g, b));
    }

    if (lut3DSize < 2 && lut1DSize < 2) {
        result.error = "CUBE file does not declare LUT_3D_SIZE or LUT_1D_SIZE.";
        return result;
    }

    if (lut1DSize >= 2) {
        if (values.size() != lut1DSize) {
            result.error = QString("1D CUBE data count mismatch. Expected %1 entries, found %2.").arg(lut1DSize).arg(values.size());
            return result;
        }

        const int targetSize = std::clamp(lut1DSize, 2, 64);
        Lut3D lut(targetSize);
        const float denom = static_cast<float>(targetSize - 1);
        for (int b = 0; b < targetSize; ++b) {
            for (int g = 0; g < targetSize; ++g) {
                for (int r = 0; r < targetSize; ++r) {
                    lut.setValue(r,
                                 g,
                                 b,
                                 QVector3D(sample1DChannel(values, r / denom, 0),
                                           sample1DChannel(values, g / denom, 1),
                                           sample1DChannel(values, b / denom, 2)));
                }
            }
        }

        result.ok = true;
        result.lut = lut;
        result.metadata["sourceType"] = "1d_cube";
        result.metadata["sourceSize"] = lut1DSize;
        result.metadata["size"] = targetSize;
        result.metadata["entries"] = values.size();
        return result;
    }

    const int lutSize = lut3DSize;
    const int expected = lutSize * lutSize * lutSize;
    if (values.size() != expected) {
        result.error = QString("CUBE data count mismatch. Expected %1 entries, found %2.").arg(expected).arg(values.size());
        return result;
    }

    Lut3D lut(lutSize);
    for (int i = 0; i < values.size(); ++i) {
        const int r = i % lutSize;
        const int g = (i / lutSize) % lutSize;
        const int b = i / (lutSize * lutSize);
        lut.setValue(r, g, b, values.at(i));
    }

    result.ok = true;
    result.lut = lut;
    result.metadata["size"] = lutSize;
    result.metadata["entries"] = values.size();
    return result;
}

LutImportResult LutImportService::importHaldPng(const QString& path)
{
    LutImportResult result;
    result.format = "hald_png";

    QImage image(path);
    if (image.isNull()) {
        result.error = "Failed to open Hald PNG image.";
        return result;
    }
    if (image.width() != image.height()) {
        result.error = "Hald PNG must be square.";
        return result;
    }

    const int level = haldLevelFromSide(image.width());
    if (level == 0) {
        result.error = "Unsupported Hald PNG dimensions. Expected a square image with side length level^3, such as 64 or 512.";
        return result;
    }

    const int lutSize = level * level;
    const int expectedPixels = lutSize * lutSize * lutSize;
    if (image.width() * image.height() != expectedPixels) {
        result.error = "Hald PNG dimensions do not match a complete 3D LUT.";
        return result;
    }

    image = image.convertToFormat(QImage::Format_RGBA8888);
    Lut3D lut(lutSize);
    for (int i = 0; i < expectedPixels; ++i) {
        const int x = i % image.width();
        const int y = i / image.width();
        const QRgb pixel = image.pixel(x, y);
        const int r = i % lutSize;
        const int g = (i / lutSize) % lutSize;
        const int b = i / (lutSize * lutSize);
        lut.setValue(r, g, b, QVector3D(qRed(pixel) / 255.0f, qGreen(pixel) / 255.0f, qBlue(pixel) / 255.0f));
    }

    result.ok = true;
    result.lut = lut;
    result.metadata["haldLevel"] = level;
    result.metadata["size"] = lutSize;
    result.metadata["side"] = image.width();
    return result;
}

} // namespace ikclut
