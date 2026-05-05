#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <array>
#include <algorithm>

namespace ikclut {

enum class CurveHandleMode {
    Auto,
    Vector
};

struct ColorGradeParams {
    QString inputColorSpace = "srgb";
    float importedLutStrength = 0.0f;
    QString activeImportedLutAssetId;
    float exposure = 0.0f;
    float contrast = 0.0f;
    float saturation = 1.0f;
    float vibrance = 0.0f;
    float temperature = 0.0f;
    float tint = 0.0f;
    float highlights = 0.0f;
    float shadows = 0.0f;
    float whites = 0.0f;
    float blacks = 0.0f;
    float logShadow = 0.0f;
    float logDark = 0.0f;
    float logLight = 0.0f;
    float logHighlight = 0.0f;
    float hdrShadow = 0.0f;
    float hdrDark = 0.0f;
    float hdrLight = 0.0f;
    float hdrHighlight = 0.0f;
    float printerRed = 0.0f;
    float printerGreen = 0.0f;
    float printerBlue = 0.0f;
    float skinToneHue = 0.0f;
    float skinToneSaturation = 0.0f;
    float skinToneLuminance = 0.0f;
    float texture = 0.0f;
    float clarity = 0.0f;
    float dehaze = 0.0f;
    float sharpen = 0.0f;
    float calibrationRedHue = 0.0f;
    float calibrationRedSaturation = 0.0f;
    float calibrationGreenHue = 0.0f;
    float calibrationGreenSaturation = 0.0f;
    float calibrationBlueHue = 0.0f;
    float calibrationBlueSaturation = 0.0f;
    float softClipLow = 0.0f;
    float softClipHigh = 1.0f;
    float softClipLowSoftness = 0.0f;
    float softClipHighSoftness = 0.0f;
    QVector<QPointF> colorWarpSources;
    QVector<QPointF> colorWarpPoints;
    QVector<float> colorWarpSourceLuma;
    QVector<float> colorWarpTargetLuma;
    QVector<bool> colorWarpPinned;

    QVector3D lift = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D gamma = QVector3D(1.0f, 1.0f, 1.0f);
    QVector3D gain = QVector3D(1.0f, 1.0f, 1.0f);
    QVector3D offset = QVector3D(0.0f, 0.0f, 0.0f);

    QVector<QPointF> masterCurve = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
    QVector<QPointF> redCurve = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
    QVector<QPointF> greenCurve = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
    QVector<QPointF> blueCurve = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
    QVector<QPointF> hueVsHueCurve = {QPointF(0.0, 0.5), QPointF(1.0, 0.5)};
    QVector<QPointF> hueVsSaturationCurve = {QPointF(0.0, 0.5), QPointF(1.0, 0.5)};
    QVector<QPointF> hueVsLuminanceCurve = {QPointF(0.0, 0.5), QPointF(1.0, 0.5)};
    QVector<CurveHandleMode> masterCurveHandles = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    QVector<CurveHandleMode> redCurveHandles = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    QVector<CurveHandleMode> greenCurveHandles = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    QVector<CurveHandleMode> blueCurveHandles = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    QVector<CurveHandleMode> hueVsHueCurveHandles = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    QVector<CurveHandleMode> hueVsSaturationCurveHandles = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    QVector<CurveHandleMode> hueVsLuminanceCurveHandles = {CurveHandleMode::Vector, CurveHandleMode::Vector};

    std::array<float, 8> hslHue = {};
    std::array<float, 8> hslSaturation = {};
    std::array<float, 8> hslLuminance = {};
};

struct MaskReference {
    QString id;
    bool inverted = false;
    float opacity = 1.0f;
};

enum class MaskKind {
    Full,
    Gradient,
    Radial,
    ColorRange,
    Paint,
    ExternalImage
};

struct MaskAsset {
    QString maskId;
    MaskKind kind = MaskKind::Full;
    bool enabled = true;
    bool inverted = false;
    float opacity = 1.0f;
    QJsonObject params;
};

struct ColorGradeStage {
    QString stageId;
    QString label;
    QString type;
    bool enabled = true;
    bool solo = false;
    float opacity = 1.0f;
    QString blendMode = "normal";
    QJsonObject params;
    MaskReference maskRef;
    QString inputTransformRef;
};

struct ColorGradePipeline {
    QVector<ColorGradeStage> stages;
    ColorGradeParams params;
};

struct ImportedLutAsset {
    QString assetId;
    QString sourcePath;
    QString format;
    int lutSize = 0;
    bool cached = false;
    QJsonObject metadata;
};

struct NodeGraphPortRef {
    QString nodeId;
    QString portId;
};

struct NodeGraphEdge {
    QString edgeId;
    NodeGraphPortRef from;
    NodeGraphPortRef to;
};

struct NodeGraphNode {
    QString nodeId;
    QString type;
    QString label;
    QPointF position;
    bool enabled = true;
    QJsonObject params;
};

struct NodeGraph {
    bool enabled = false;
    QString outputNodeId;
    QVector<NodeGraphNode> nodes;
    QVector<NodeGraphEdge> edges;
};

struct ImageDocument {
    QString imagePath;
    QString projectPath;
    ColorGradePipeline pipeline;
    QVector<ImportedLutAsset> importedLuts;
    QVector<MaskAsset> masks;
    NodeGraph nodeGraph;
};

enum class ColorTransformKind {
    ParamPipeline,
    ImportedLut
};

struct ColorTransformSource {
    ColorTransformKind kind = ColorTransformKind::ParamPipeline;
    ColorGradeParams params;
    bool usePipeline = false;
    ColorGradePipeline pipeline;
    QVector<MaskAsset> masks;
    QString importedLutAssetId;
    float importedLutStrength = 1.0f;
};

inline QJsonArray vectorToJson(const QVector3D& v)
{
    return QJsonArray{v.x(), v.y(), v.z()};
}

inline QJsonArray curveToJson(const QVector<QPointF>& curve)
{
    QJsonArray arr;
    for (const QPointF& point : curve) {
        arr.append(QJsonArray{point.x(), point.y()});
    }
    return arr;
}

inline QJsonArray pointsToJson(const QVector<QPointF>& points)
{
    QJsonArray arr;
    for (const QPointF& point : points) {
        arr.append(QJsonArray{point.x(), point.y()});
    }
    return arr;
}

inline QVector<QPointF> curveFromJson(const QJsonValue& value)
{
    QVector<QPointF> curve;
    const QJsonArray arr = value.toArray();
    for (const QJsonValue& item : arr) {
        const QJsonArray point = item.toArray();
        if (point.size() == 2) {
            curve.append(QPointF(std::clamp(point.at(0).toDouble(), 0.0, 1.0),
                                 std::clamp(point.at(1).toDouble(), 0.0, 1.0)));
        }
    }
    if (curve.size() < 2) {
        curve = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
    }
    std::sort(curve.begin(), curve.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x();
    });
    curve.first().setX(0.0);
    curve.last().setX(1.0);
    return curve;
}

inline QVector<QPointF> curveFromJson(const QJsonValue& value, const QVector<QPointF>& fallback)
{
    QVector<QPointF> curve;
    const QJsonArray arr = value.toArray();
    for (const QJsonValue& item : arr) {
        const QJsonArray point = item.toArray();
        if (point.size() == 2) {
            curve.append(QPointF(std::clamp(point.at(0).toDouble(), 0.0, 1.0),
                                 std::clamp(point.at(1).toDouble(), 0.0, 1.0)));
        }
    }
    if (curve.size() < 2) {
        curve = fallback;
    }
    std::sort(curve.begin(), curve.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x();
    });
    curve.first().setX(0.0);
    curve.last().setX(1.0);
    return curve;
}

inline QJsonArray curveHandlesToJson(const QVector<CurveHandleMode>& handles)
{
    QJsonArray arr;
    for (CurveHandleMode handle : handles) {
        arr.append(handle == CurveHandleMode::Auto ? "auto" : "vector");
    }
    return arr;
}

inline QVector<CurveHandleMode> curveHandlesFromJson(const QJsonValue& value, int expectedSize)
{
    QVector<CurveHandleMode> handles;
    handles.reserve(expectedSize);
    const QJsonArray arr = value.toArray();
    for (int i = 0; i < expectedSize; ++i) {
        const QString handle = i < arr.size() ? arr.at(i).toString("vector") : QString("vector");
        handles.append(handle == "auto" ? CurveHandleMode::Auto : CurveHandleMode::Vector);
    }
    return handles;
}

inline QVector<QPointF> pointsFromJson(const QJsonValue& value, const QVector<QPointF>& fallback)
{
    QVector<QPointF> points;
    const QJsonArray arr = value.toArray();
    for (const QJsonValue& item : arr) {
        const QJsonArray point = item.toArray();
        if (point.size() == 2) {
            points.append(QPointF(std::clamp(point.at(0).toDouble(), 0.0, 1.0),
                                  std::clamp(point.at(1).toDouble(), 0.0, 1.0)));
        }
    }
    return points.isEmpty() ? fallback : points;
}

inline QJsonArray boolVectorToJson(const QVector<bool>& values)
{
    QJsonArray arr;
    for (bool value : values) {
        arr.append(value);
    }
    return arr;
}

inline QVector<bool> boolVectorFromJson(const QJsonValue& value, int expectedSize)
{
    QVector<bool> values;
    const QJsonArray arr = value.toArray();
    values.reserve(expectedSize);
    for (int i = 0; i < expectedSize; ++i) {
        values.append(i < arr.size() ? arr.at(i).toBool(false) : false);
    }
    return values;
}

inline QJsonArray floatVectorToJson(const QVector<float>& values)
{
    QJsonArray arr;
    for (float value : values) {
        arr.append(value);
    }
    return arr;
}

inline QVector<float> floatVectorFromJson(const QJsonValue& value, int expectedSize, float fallback)
{
    QVector<float> values;
    const QJsonArray arr = value.toArray();
    values.reserve(expectedSize);
    for (int i = 0; i < expectedSize; ++i) {
        const float v = i < arr.size() ? static_cast<float>(arr.at(i).toDouble(fallback)) : fallback;
        values.append(std::clamp(v, 0.0f, 1.0f));
    }
    return values;
}

inline QPointF legacyWarpSourceForIndex(int index, int count, const QPointF& target)
{
    if (count == 18) {
        const int row = std::clamp(index / 6, 0, 2);
        const int col = std::clamp(index % 6, 0, 5);
        const double sat = row == 0 ? 0.25 : row == 1 ? 0.55 : 0.82;
        return QPointF(col / 6.0, sat);
    }
    return target;
}

inline QJsonArray floatArrayToJson(const std::array<float, 8>& values)
{
    QJsonArray arr;
    for (float value : values) {
        arr.append(value);
    }
    return arr;
}

inline std::array<float, 8> floatArrayFromJson(const QJsonValue& value)
{
    std::array<float, 8> values = {};
    const QJsonArray arr = value.toArray();
    for (int i = 0; i < std::min(8, static_cast<int>(arr.size())); ++i) {
        values[static_cast<size_t>(i)] = static_cast<float>(arr.at(i).toDouble());
    }
    return values;
}

inline QVector3D vectorFromJson(const QJsonValue& value, const QVector3D& fallback)
{
    const QJsonArray arr = value.toArray();
    if (arr.size() != 3) {
        return fallback;
    }
    return QVector3D(static_cast<float>(arr.at(0).toDouble(fallback.x())),
                     static_cast<float>(arr.at(1).toDouble(fallback.y())),
                     static_cast<float>(arr.at(2).toDouble(fallback.z())));
}

inline QJsonObject paramsToJson(const ColorGradeParams& params)
{
    QJsonObject obj;
    obj["inputColorSpace"] = params.inputColorSpace;
    obj["importedLutStrength"] = params.importedLutStrength;
    obj["activeImportedLutAssetId"] = params.activeImportedLutAssetId;
    obj["exposure"] = params.exposure;
    obj["contrast"] = params.contrast;
    obj["saturation"] = params.saturation;
    obj["vibrance"] = params.vibrance;
    obj["temperature"] = params.temperature;
    obj["tint"] = params.tint;
    obj["highlights"] = params.highlights;
    obj["shadows"] = params.shadows;
    obj["whites"] = params.whites;
    obj["blacks"] = params.blacks;
    obj["logShadow"] = params.logShadow;
    obj["logDark"] = params.logDark;
    obj["logLight"] = params.logLight;
    obj["logHighlight"] = params.logHighlight;
    obj["hdrShadow"] = params.hdrShadow;
    obj["hdrDark"] = params.hdrDark;
    obj["hdrLight"] = params.hdrLight;
    obj["hdrHighlight"] = params.hdrHighlight;
    obj["printerRed"] = params.printerRed;
    obj["printerGreen"] = params.printerGreen;
    obj["printerBlue"] = params.printerBlue;
    obj["skinToneHue"] = params.skinToneHue;
    obj["skinToneSaturation"] = params.skinToneSaturation;
    obj["skinToneLuminance"] = params.skinToneLuminance;
    obj["texture"] = params.texture;
    obj["clarity"] = params.clarity;
    obj["dehaze"] = params.dehaze;
    obj["sharpen"] = params.sharpen;
    obj["calibrationRedHue"] = params.calibrationRedHue;
    obj["calibrationRedSaturation"] = params.calibrationRedSaturation;
    obj["calibrationGreenHue"] = params.calibrationGreenHue;
    obj["calibrationGreenSaturation"] = params.calibrationGreenSaturation;
    obj["calibrationBlueHue"] = params.calibrationBlueHue;
    obj["calibrationBlueSaturation"] = params.calibrationBlueSaturation;
    obj["softClipLow"] = params.softClipLow;
    obj["softClipHigh"] = params.softClipHigh;
    obj["softClipLowSoftness"] = params.softClipLowSoftness;
    obj["softClipHighSoftness"] = params.softClipHighSoftness;
    obj["colorWarpSources"] = pointsToJson(params.colorWarpSources);
    obj["colorWarpPoints"] = pointsToJson(params.colorWarpPoints);
    obj["colorWarpSourceLuma"] = floatVectorToJson(params.colorWarpSourceLuma);
    obj["colorWarpTargetLuma"] = floatVectorToJson(params.colorWarpTargetLuma);
    obj["colorWarpPinned"] = boolVectorToJson(params.colorWarpPinned);
    obj["lift"] = vectorToJson(params.lift);
    obj["gamma"] = vectorToJson(params.gamma);
    obj["gain"] = vectorToJson(params.gain);
    obj["offset"] = vectorToJson(params.offset);
    obj["masterCurve"] = curveToJson(params.masterCurve);
    obj["redCurve"] = curveToJson(params.redCurve);
    obj["greenCurve"] = curveToJson(params.greenCurve);
    obj["blueCurve"] = curveToJson(params.blueCurve);
    obj["hueVsHueCurve"] = curveToJson(params.hueVsHueCurve);
    obj["hueVsSaturationCurve"] = curveToJson(params.hueVsSaturationCurve);
    obj["hueVsLuminanceCurve"] = curveToJson(params.hueVsLuminanceCurve);
    obj["masterCurveHandles"] = curveHandlesToJson(params.masterCurveHandles);
    obj["redCurveHandles"] = curveHandlesToJson(params.redCurveHandles);
    obj["greenCurveHandles"] = curveHandlesToJson(params.greenCurveHandles);
    obj["blueCurveHandles"] = curveHandlesToJson(params.blueCurveHandles);
    obj["hueVsHueCurveHandles"] = curveHandlesToJson(params.hueVsHueCurveHandles);
    obj["hueVsSaturationCurveHandles"] = curveHandlesToJson(params.hueVsSaturationCurveHandles);
    obj["hueVsLuminanceCurveHandles"] = curveHandlesToJson(params.hueVsLuminanceCurveHandles);
    obj["hslHue"] = floatArrayToJson(params.hslHue);
    obj["hslSaturation"] = floatArrayToJson(params.hslSaturation);
    obj["hslLuminance"] = floatArrayToJson(params.hslLuminance);
    return obj;
}

inline ColorGradeParams paramsFromJson(const QJsonObject& obj)
{
    ColorGradeParams params;
    params.inputColorSpace = obj.value("inputColorSpace").toString(params.inputColorSpace);
    params.importedLutStrength = std::clamp(static_cast<float>(obj.value("importedLutStrength").toDouble(params.importedLutStrength)), 0.0f, 1.0f);
    params.activeImportedLutAssetId = obj.value("activeImportedLutAssetId").toString(params.activeImportedLutAssetId);
    params.exposure = static_cast<float>(obj.value("exposure").toDouble(params.exposure));
    params.contrast = static_cast<float>(obj.value("contrast").toDouble(params.contrast));
    params.saturation = static_cast<float>(obj.value("saturation").toDouble(params.saturation));
    params.vibrance = static_cast<float>(obj.value("vibrance").toDouble(params.vibrance));
    params.temperature = static_cast<float>(obj.value("temperature").toDouble(params.temperature));
    params.tint = static_cast<float>(obj.value("tint").toDouble(params.tint));
    params.highlights = static_cast<float>(obj.value("highlights").toDouble(params.highlights));
    params.shadows = static_cast<float>(obj.value("shadows").toDouble(params.shadows));
    params.whites = static_cast<float>(obj.value("whites").toDouble(params.whites));
    params.blacks = static_cast<float>(obj.value("blacks").toDouble(params.blacks));
    params.logShadow = static_cast<float>(obj.value("logShadow").toDouble(params.logShadow));
    params.logDark = static_cast<float>(obj.value("logDark").toDouble(params.logDark));
    params.logLight = static_cast<float>(obj.value("logLight").toDouble(params.logLight));
    params.logHighlight = static_cast<float>(obj.value("logHighlight").toDouble(params.logHighlight));
    params.hdrShadow = static_cast<float>(obj.value("hdrShadow").toDouble(params.hdrShadow));
    params.hdrDark = static_cast<float>(obj.value("hdrDark").toDouble(params.hdrDark));
    params.hdrLight = static_cast<float>(obj.value("hdrLight").toDouble(params.hdrLight));
    params.hdrHighlight = static_cast<float>(obj.value("hdrHighlight").toDouble(params.hdrHighlight));
    params.printerRed = static_cast<float>(obj.value("printerRed").toDouble(params.printerRed));
    params.printerGreen = static_cast<float>(obj.value("printerGreen").toDouble(params.printerGreen));
    params.printerBlue = static_cast<float>(obj.value("printerBlue").toDouble(params.printerBlue));
    params.skinToneHue = static_cast<float>(obj.value("skinToneHue").toDouble(params.skinToneHue));
    params.skinToneSaturation = static_cast<float>(obj.value("skinToneSaturation").toDouble(params.skinToneSaturation));
    params.skinToneLuminance = static_cast<float>(obj.value("skinToneLuminance").toDouble(params.skinToneLuminance));
    params.texture = static_cast<float>(obj.value("texture").toDouble(params.texture));
    params.clarity = static_cast<float>(obj.value("clarity").toDouble(params.clarity));
    params.dehaze = static_cast<float>(obj.value("dehaze").toDouble(params.dehaze));
    params.sharpen = static_cast<float>(obj.value("sharpen").toDouble(params.sharpen));
    params.calibrationRedHue = static_cast<float>(obj.value("calibrationRedHue").toDouble(params.calibrationRedHue));
    params.calibrationRedSaturation = static_cast<float>(obj.value("calibrationRedSaturation").toDouble(params.calibrationRedSaturation));
    params.calibrationGreenHue = static_cast<float>(obj.value("calibrationGreenHue").toDouble(params.calibrationGreenHue));
    params.calibrationGreenSaturation = static_cast<float>(obj.value("calibrationGreenSaturation").toDouble(params.calibrationGreenSaturation));
    params.calibrationBlueHue = static_cast<float>(obj.value("calibrationBlueHue").toDouble(params.calibrationBlueHue));
    params.calibrationBlueSaturation = static_cast<float>(obj.value("calibrationBlueSaturation").toDouble(params.calibrationBlueSaturation));
    params.softClipLow = static_cast<float>(obj.value("softClipLow").toDouble(params.softClipLow));
    params.softClipHigh = static_cast<float>(obj.value("softClipHigh").toDouble(params.softClipHigh));
    params.softClipLowSoftness = static_cast<float>(obj.value("softClipLowSoftness").toDouble(params.softClipLowSoftness));
    params.softClipHighSoftness = static_cast<float>(obj.value("softClipHighSoftness").toDouble(params.softClipHighSoftness));
    params.colorWarpPoints = pointsFromJson(obj.value("colorWarpPoints"), params.colorWarpPoints);
    params.colorWarpSources = pointsFromJson(obj.value("colorWarpSources"), params.colorWarpSources);
    if (params.colorWarpSources.size() != params.colorWarpPoints.size()) {
        params.colorWarpSources.clear();
        for (int i = 0; i < params.colorWarpPoints.size(); ++i) {
            params.colorWarpSources.append(legacyWarpSourceForIndex(i, params.colorWarpPoints.size(), params.colorWarpPoints.at(i)));
        }
    }
    params.colorWarpSourceLuma = floatVectorFromJson(obj.value("colorWarpSourceLuma"), params.colorWarpPoints.size(), 0.5f);
    params.colorWarpTargetLuma = floatVectorFromJson(obj.value("colorWarpTargetLuma"), params.colorWarpPoints.size(), 0.5f);
    params.colorWarpPinned = boolVectorFromJson(obj.value("colorWarpPinned"), params.colorWarpPoints.size());
    params.lift = vectorFromJson(obj.value("lift"), params.lift);
    params.gamma = vectorFromJson(obj.value("gamma"), params.gamma);
    params.gain = vectorFromJson(obj.value("gain"), params.gain);
    params.offset = vectorFromJson(obj.value("offset"), params.offset);
    params.masterCurve = curveFromJson(obj.value("masterCurve"));
    params.redCurve = curveFromJson(obj.value("redCurve"));
    params.greenCurve = curveFromJson(obj.value("greenCurve"));
    params.blueCurve = curveFromJson(obj.value("blueCurve"));
    params.hueVsHueCurve = curveFromJson(obj.value("hueVsHueCurve"), params.hueVsHueCurve);
    params.hueVsSaturationCurve = curveFromJson(obj.value("hueVsSaturationCurve"), params.hueVsSaturationCurve);
    params.hueVsLuminanceCurve = curveFromJson(obj.value("hueVsLuminanceCurve"), params.hueVsLuminanceCurve);
    params.masterCurveHandles = curveHandlesFromJson(obj.value("masterCurveHandles"), params.masterCurve.size());
    params.redCurveHandles = curveHandlesFromJson(obj.value("redCurveHandles"), params.redCurve.size());
    params.greenCurveHandles = curveHandlesFromJson(obj.value("greenCurveHandles"), params.greenCurve.size());
    params.blueCurveHandles = curveHandlesFromJson(obj.value("blueCurveHandles"), params.blueCurve.size());
    params.hueVsHueCurveHandles = curveHandlesFromJson(obj.value("hueVsHueCurveHandles"), params.hueVsHueCurve.size());
    params.hueVsSaturationCurveHandles = curveHandlesFromJson(obj.value("hueVsSaturationCurveHandles"), params.hueVsSaturationCurve.size());
    params.hueVsLuminanceCurveHandles = curveHandlesFromJson(obj.value("hueVsLuminanceCurveHandles"), params.hueVsLuminanceCurve.size());
    params.hslHue = floatArrayFromJson(obj.value("hslHue"));
    params.hslSaturation = floatArrayFromJson(obj.value("hslSaturation"));
    params.hslLuminance = floatArrayFromJson(obj.value("hslLuminance"));
    return params;
}

} // namespace ikclut
