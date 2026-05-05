#include "core/IkClutExporter.h"
#include "core/ColorPipeline.h"
#include "core/ExecutionPlan.h"
#include "core/Lut3D.h"
#include "core/LutImportService.h"
#include "core/LutQualityAnalyzer.h"
#include "core/MaskEvaluator.h"
#include "core/ProjectSerializer.h"
#include "model/ColorTypes.h"
#include "render/DirectX11PreviewRenderer.h"
#include "render/PreviewRenderer.h"
#include "render/ScopeRenderer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

#include <iostream>

using namespace ikclut;

namespace {

int fail(const QString& message)
{
    std::cerr << message.toStdString() << std::endl;
    return 1;
}

bool writeIdentityCube(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    QTextStream out(&file);
    out << "TITLE \"identity\"\n";
    out << "LUT_3D_SIZE 2\n";
    for (int b = 0; b < 2; ++b) {
        for (int g = 0; g < 2; ++g) {
            for (int r = 0; r < 2; ++r) {
                out << r << " " << g << " " << b << "\n";
            }
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const Lut3D identity = Lut3D::identity(32);
    const QImage ik = IkClutExporter::makeImage(identity);
    if (ik.size() != QSize(1024, 32)) {
        return fail("identity ikClut export has wrong dimensions");
    }
    const ExportValidationResult identityValidation = IkClutExporter::validateImage(ik);
    if (!identityValidation.ok || !identityValidation.identityCornersOk) {
        return fail("identity ikClut export failed validation");
    }
    if (qRed(ik.pixel(31, 0)) != 255 || qGreen(ik.pixel(0, 31)) != 255 || qBlue(ik.pixel(31 * 32, 0)) != 255) {
        return fail("identity ikClut export has wrong channel layout");
    }

    ColorGradeParams curveParams;
    curveParams.masterCurve = {QPointF(0.0, 0.0), QPointF(0.5, 0.72), QPointF(1.0, 1.0)};
    const QVector3D curved = ColorPipeline::applyParams(QVector3D(0.5f, 0.5f, 0.5f), curveParams);
    if (curved.x() <= 0.6f) {
        return fail("master curve did not affect color pipeline");
    }
    ColorGradeParams autoCurveParams;
    autoCurveParams.masterCurve = {QPointF(0.0, 0.0), QPointF(0.2, 0.8), QPointF(1.0, 1.0)};
    autoCurveParams.masterCurveHandles = {CurveHandleMode::Auto, CurveHandleMode::Auto, CurveHandleMode::Auto};
    ColorGradeParams vectorCurveParams = autoCurveParams;
    vectorCurveParams.masterCurveHandles = {CurveHandleMode::Vector, CurveHandleMode::Vector, CurveHandleMode::Vector};
    const QVector3D autoCurved = ColorPipeline::applyParams(QVector3D(0.6f, 0.6f, 0.6f), autoCurveParams);
    const QVector3D vectorCurved = ColorPipeline::applyParams(QVector3D(0.6f, 0.6f, 0.6f), vectorCurveParams);
    if (autoCurved.x() <= vectorCurved.x() + 0.03f) {
        return fail("auto curve handles did not affect color pipeline");
    }

    ColorGradeParams hslParams;
    hslParams.hslHue[0] = 60.0f;
    const QVector3D shifted = ColorPipeline::applyParams(QVector3D(1.0f, 0.0f, 0.0f), hslParams);
    if (shifted.y() <= 0.4f) {
        return fail("HSL hue adjustment did not affect color pipeline");
    }
    ColorGradeParams hueVsParams;
    hueVsParams.hueVsHueCurve = {QPointF(0.0, 0.75), QPointF(1.0, 0.75)};
    const QVector3D hueVsShifted = ColorPipeline::applyParams(QVector3D(1.0f, 0.0f, 0.0f), hueVsParams);
    if (hueVsShifted.y() <= 0.25f) {
        return fail("Hue-vs-Hue curve did not affect color pipeline");
    }
    hueVsParams = ColorGradeParams();
    hueVsParams.hueVsLuminanceCurve = {QPointF(0.0, 0.8), QPointF(1.0, 0.8)};
    const QVector3D hueVsLifted = ColorPipeline::applyParams(QVector3D(0.0f, 0.0f, 1.0f), hueVsParams);
    if (hueVsLifted.x() <= 0.25f || hueVsLifted.y() <= 0.25f) {
        return fail("Hue-vs-Luma curve did not affect color pipeline");
    }
    ColorGradeParams linearInputParams;
    linearInputParams.inputColorSpace = "linear";
    const QVector3D displayMapped = ColorPipeline::applyParams(QVector3D(0.25f, 0.25f, 0.25f), linearInputParams);
    if (displayMapped.x() <= 0.5f || displayMapped.x() >= 0.56f) {
        return fail("linear input color space did not map to display space");
    }
    ColorGradeParams logInputParams;
    logInputParams.inputColorSpace = "slog3";
    const QVector3D logMapped = ColorPipeline::applyParams(QVector3D(0.41f, 0.41f, 0.41f), logInputParams);
    if (logMapped.x() <= 0.42f || logMapped.x() >= 0.52f) {
        return fail(QString("S-Log3 input color space did not decode middle gray as expected: %1").arg(logMapped.x(), 0, 'f', 4));
    }
    ColorGradeParams warperLumaParams;
    warperLumaParams.colorWarpSources = {QPointF(0.0, 1.0)};
    warperLumaParams.colorWarpPoints = {QPointF(0.0, 1.0)};
    warperLumaParams.colorWarpSourceLuma = {0.5f};
    warperLumaParams.colorWarpTargetLuma = {0.85f};
    const QVector3D liftedRed = ColorPipeline::applyParams(QVector3D(1.0f, 0.0f, 0.0f), warperLumaParams);
    if (liftedRed.y() <= 0.45f || liftedRed.z() <= 0.45f) {
        return fail("Warper luma dimension did not affect color pipeline");
    }

    QImage previewSource(2000, 1000, QImage::Format_ARGB32);
    previewSource.fill(qRgb(128, 128, 128));
    PreviewRenderRequest interactiveRequest;
    interactiveRequest.source = previewSource;
    interactiveRequest.transform.kind = ColorTransformKind::ParamPipeline;
    interactiveRequest.transform.params.exposure = 0.25f;
    interactiveRequest.quality = PreviewQuality::Interactive;
    interactiveRequest.maxInteractiveSize = QSize(500, 500);
    interactiveRequest.preferGpu = false;
    const PreviewRenderResult interactivePreview = PreviewRenderer::render(interactiveRequest);
    if (interactivePreview.image.width() > 500 || interactivePreview.image.height() > 500) {
        return fail("interactive preview did not respect max preview size");
    }

    PreviewRenderRequest fullRequest = interactiveRequest;
    fullRequest.quality = PreviewQuality::Full;
    fullRequest.preferGpu = false;
    const PreviewRenderResult fullPreview = PreviewRenderer::render(fullRequest);
    if (fullPreview.image.size() != previewSource.size()) {
        return fail("full preview did not preserve source size");
    }
    const Lut3D resolvedPreviewLut = IkClutExporter::resolveExportLut(fullRequest.transform, fullRequest.transform.params, Lut3D()).resampled(32);
    const QImage expectedPreview = ColorPipeline::applyLutToImage(previewSource, resolvedPreviewLut);
    if (fullPreview.image.pixelColor(0, 0) != expectedPreview.pixelColor(0, 0)) {
        return fail("CPU preview fallback is not using export LUT semantics");
    }
    QImage defaultSource(16, 16, QImage::Format_ARGB32);
    for (int y = 0; y < defaultSource.height(); ++y) {
        for (int x = 0; x < defaultSource.width(); ++x) {
            defaultSource.setPixel(x, y, qRgba(x * 17, y * 17, ((x + y) % 16) * 17, 255));
        }
    }
    PreviewRenderRequest defaultRequest;
    defaultRequest.source = defaultSource;
    defaultRequest.preferGpu = false;
    const PreviewRenderResult defaultPreview = PreviewRenderer::render(defaultRequest);
    if (defaultPreview.image.size() != defaultSource.size()) {
        return fail("default preview changed image size");
    }
    for (int y = 0; y < defaultSource.height(); ++y) {
        for (int x = 0; x < defaultSource.width(); ++x) {
            if (defaultPreview.image.pixelColor(x, y) != defaultSource.pixelColor(x, y)) {
                return fail(QString("default preview is not identity at %1,%2").arg(x).arg(y));
            }
        }
    }
    const Lut3D defaultExport = IkClutExporter::resolveExportLut(ColorTransformSource(), ColorGradeParams(), Lut3D());
    for (const QVector3D& sample : {QVector3D(0.0f, 0.0f, 0.0f),
                                    QVector3D(1.0f, 1.0f, 1.0f),
                                    QVector3D(0.25f, 0.5f, 0.75f),
                                    QVector3D(0.9f, 0.2f, 0.1f)}) {
        const QVector3D out = defaultExport.sample(sample);
        if ((out - sample).length() > 0.0001f) {
            return fail("default export LUT is not identity");
        }
    }

    Lut3D blackLut(2);
    ColorGradeParams mixedParams;
    mixedParams.exposure = 1.0f;
    ColorTransformSource mixedSource;
    mixedSource.kind = ColorTransformKind::ImportedLut;
    mixedSource.params = mixedParams;
    mixedSource.importedLutStrength = 0.25f;
    const QVector3D mixedSample = IkClutExporter::resolveExportLut(mixedSource, mixedParams, blackLut)
                                      .sample(QVector3D(0.5f, 0.5f, 0.5f));
    if (mixedSample.x() < 0.72f || mixedSample.x() > 0.78f) {
        return fail("imported LUT strength did not blend after parameter grading");
    }
    mixedSource.importedLutStrength = 0.0f;
    const QVector3D paramOnlySample = IkClutExporter::resolveExportLut(mixedSource, mixedParams, blackLut)
                                          .sample(QVector3D(0.5f, 0.5f, 0.5f));
    if (paramOnlySample.x() < 0.98f) {
        return fail("zero imported LUT strength should preserve parameter grade");
    }
    const LutQualityReport identityQuality = LutQualityAnalyzer::analyze(Lut3D::identity(32));
    if (!identityQuality.ok || identityQuality.warningCount != 0) {
        return fail("identity LUT quality check should pass without warnings");
    }
    const LutQualityReport blackQuality = LutQualityAnalyzer::analyze(blackLut);
    if (!blackQuality.ok || blackQuality.warningCount == 0) {
        return fail("clipped LUT quality check should produce warnings");
    }
    ColorGradeParams professionalParams;
    professionalParams.logLight = 0.7f;
    const QVector3D logLifted = ColorPipeline::applyParams(QVector3D(0.62f, 0.62f, 0.62f), professionalParams);
    if (logLifted.x() <= 0.68f) {
        return fail("Log wheel light zone did not affect color pipeline");
    }
    professionalParams = ColorGradeParams();
    professionalParams.printerRed = 4.0f;
    const QVector3D printedRed = ColorPipeline::applyParams(QVector3D(0.4f, 0.4f, 0.4f), professionalParams);
    if (printedRed.x() <= printedRed.y() + 0.08f) {
        return fail("Printer lights did not bias the red channel");
    }
    professionalParams = ColorGradeParams();
    professionalParams.skinToneSaturation = 0.8f;
    const QVector3D skinBase = ColorPipeline::applyParams(QVector3D(0.85f, 0.48f, 0.32f), ColorGradeParams());
    const QVector3D skinAdjusted = ColorPipeline::applyParams(QVector3D(0.85f, 0.48f, 0.32f), professionalParams);
    const QVector3D greenBase = ColorPipeline::applyParams(QVector3D(0.1f, 0.8f, 0.1f), ColorGradeParams());
    const QVector3D greenProtected = ColorPipeline::applyParams(QVector3D(0.1f, 0.8f, 0.1f), professionalParams);
    const auto chroma = [](const QVector3D& c) {
        return std::max({c.x(), c.y(), c.z()}) - std::min({c.x(), c.y(), c.z()});
    };
    if (chroma(skinAdjusted) <= chroma(skinBase) + 0.03f || std::abs(greenProtected.y() - greenBase.y()) > 0.08f) {
        return fail("Skin Tone Tool did not target skin-like hues while protecting green");
    }

    QImage gpuPreview;
    QString gpuError;
    const bool gpuOk = DirectX11PreviewRenderer::render(QImage(16, 16, QImage::Format_RGBA8888), Lut3D::identity(16), &gpuPreview, &gpuError);
    if (gpuOk && gpuPreview.size() != QSize(16, 16)) {
        return fail("DirectX preview returned wrong image size");
    }
    QImage scopeProbe(3, 1, QImage::Format_ARGB32);
    scopeProbe.setPixel(0, 0, qRgb(255, 0, 0));
    scopeProbe.setPixel(1, 0, qRgb(255, 255, 0));
    scopeProbe.setPixel(2, 0, qRgb(0, 0, 255));
    const ScopeData scopeData = ScopeRenderer::compute(scopeProbe, 3);
    if (scopeData.vectorscope.size() != 3
        || scopeData.vectorscope.at(0).y() >= 0.0
        || scopeData.vectorscope.at(1).x() >= 0.0
        || scopeData.vectorscope.at(2).x() <= 0.0) {
        return fail("vectorscope channel orientation is wrong");
    }
    if (scopeData.exposureHistogram.size() != 120
        || scopeData.exposureZoneCounts.size() != 6
        || scopeData.exposureZoneCounts.at(0) != 1
        || scopeData.exposureZoneCounts.at(1) != 1
        || scopeData.exposureZoneCounts.at(4) != 1
        || scopeData.exposureMean <= 1.9
        || scopeData.exposureMean >= 2.2
        || scopeData.exposureLowPercent < 33.0
        || scopeData.exposureHighPercent < 33.0) {
        return fail("exposure analysis distribution is wrong");
    }
    const QString tempRoot = QDir::currentPath() + "/core-test-work";
    QDir().mkpath(tempRoot);
    QDir temp(tempRoot);
    if (!temp.exists()) {
        return fail("failed to create temporary dir at " + tempRoot);
    }

    QString ioError;
    const QString cubePath = temp.filePath("identity.cube");
    if (!writeIdentityCube(cubePath, &ioError)) {
        return fail("failed to write test cube at " + cubePath + ": " + ioError);
    }
    const LutImportResult cube = LutImportService::importCube(cubePath);
    if (!cube.ok || cube.lut.size() != 2) {
        return fail("failed to parse standard 3D CUBE");
    }
    const QString exportedCubePath = temp.filePath("exported.cube");
    const ExportResult cubeExport = IkClutExporter::saveCube(Lut3D::identity(17), exportedCubePath);
    if (!cubeExport.ok) {
        return fail("failed to export CUBE: " + cubeExport.error);
    }
    const LutImportResult exportedCube = LutImportService::importCube(exportedCubePath);
    if (!exportedCube.ok || exportedCube.lut.size() != 17) {
        return fail("exported CUBE did not round trip through importer");
    }
    const QString exportedHaldPath = temp.filePath("exported-hald.png");
    const ExportResult haldExport = IkClutExporter::saveHaldPng(Lut3D::identity(16), exportedHaldPath);
    if (!haldExport.ok || haldExport.imageSize != QSize(64, 64)) {
        return fail("failed to export Hald PNG: " + haldExport.error);
    }
    const LutImportResult exportedHald = LutImportService::importHaldPng(exportedHaldPath);
    if (!exportedHald.ok || exportedHald.lut.size() != 16) {
        return fail("exported Hald PNG did not round trip through importer");
    }

    QFile oneD(temp.filePath("oned.cube"));
    if (!oneD.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return fail("failed to write 1D cube: " + oneD.errorString());
    }
    oneD.write("LUT_1D_SIZE 3\n0 0 0\n0.25 0.5 0.75\n1 1 1\n");
    oneD.close();
    const LutImportResult oneDResult = LutImportService::importCube(oneD.fileName());
    if (!oneDResult.ok || oneDResult.lut.size() != 3) {
        return fail("1D CUBE should import as a baked 3D LUT");
    }
    const QVector3D oneDSample = oneDResult.lut.sample(QVector3D(0.5f, 0.5f, 0.5f));
    if (oneDSample.x() < 0.24f || oneDSample.y() < 0.49f || oneDSample.z() < 0.74f) {
        return fail("1D CUBE import did not remap channels independently");
    }
    QFile mixed(temp.filePath("mixed.cube"));
    if (!mixed.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return fail("failed to write mixed cube: " + mixed.errorString());
    }
    mixed.write("LUT_1D_SIZE 2\nLUT_3D_SIZE 2\n0 0 0\n1 1 1\n");
    mixed.close();
    if (LutImportService::importCube(mixed.fileName()).ok) {
        return fail("mixed 1D+3D CUBE should still be rejected");
    }

    QImage hald(64, 64, QImage::Format_ARGB32);
    for (int y = 0; y < hald.height(); ++y) {
        for (int x = 0; x < hald.width(); ++x) {
            const int i = y * hald.width() + x;
            const int size = 16;
            const int r = i % size;
            const int g = (i / size) % size;
            const int b = i / (size * size);
            hald.setPixel(x, y, qRgb(r * 17, g * 17, b * 17));
        }
    }
    const QString haldPath = temp.filePath("hald.png");
    hald.save(haldPath);
    const LutImportResult haldResult = LutImportService::importHaldPng(haldPath);
    if (!haldResult.ok || haldResult.lut.size() != 16) {
        return fail("failed to parse Hald PNG");
    }

    ImageDocument document;
    document.imagePath = "sample.png";
    ColorGradeStage stage;
    stage.stageId = "global-grade";
    stage.label = "Skin Qualifier";
    stage.type = "globalColorGrade";
    stage.blendMode = "color";
    stage.solo = true;
    stage.maskRef.id = "mask-full";
    stage.maskRef.opacity = 0.75f;
    stage.inputTransformRef = "";
    document.pipeline.stages.append(stage);
    MaskAsset mask;
    mask.maskId = "mask-full";
    mask.kind = MaskKind::Full;
    mask.opacity = 0.8f;
    document.masks.append(mask);
    document.nodeGraph.enabled = true;
    document.nodeGraph.outputNodeId = "node-output";
    document.nodeGraph.nodes.append(NodeGraphNode{"node-input", "input", "Input", QPointF(0, 0), true, QJsonObject{}});
    document.nodeGraph.nodes.append(NodeGraphNode{"node-grade", "globalColorGrade", "Grade", QPointF(220, 0), true, paramsToJson(curveParams)});
    document.nodeGraph.nodes.append(NodeGraphNode{"node-output", "output", "Output", QPointF(440, 0), true, QJsonObject{}});
    document.nodeGraph.edges.append(NodeGraphEdge{"edge-1", NodeGraphPortRef{"node-input", "out"}, NodeGraphPortRef{"node-grade", "in"}});
    document.nodeGraph.edges.append(NodeGraphEdge{"edge-2", NodeGraphPortRef{"node-grade", "out"}, NodeGraphPortRef{"node-output", "in"}});
    document.pipeline.params.masterCurve = curveParams.masterCurve;
    document.pipeline.params.masterCurveHandles = {CurveHandleMode::Auto, CurveHandleMode::Vector, CurveHandleMode::Auto};
    document.pipeline.params.inputColorSpace = "logc3";
    document.pipeline.params.hslSaturation[2] = 0.35f;
    document.pipeline.params.colorWarpSources = {QPointF(0.0, 1.0)};
    document.pipeline.params.colorWarpPoints = {QPointF(0.08, 0.8)};
    document.pipeline.params.colorWarpSourceLuma = {0.25f};
    document.pipeline.params.colorWarpTargetLuma = {0.7f};
    document.pipeline.params.importedLutStrength = 0.42f;
    document.pipeline.params.activeImportedLutAssetId = "lut-1";
    document.pipeline.params.logDark = -0.25f;
    document.pipeline.params.hdrHighlight = 0.32f;
    document.pipeline.params.printerBlue = -3.0f;
    document.pipeline.params.skinToneHue = 6.0f;
    document.importedLuts.append(ImportedLutAsset{"lut-1", cubePath, "cube", 2, false, QJsonObject{{"size", 2}}});

    const QString projectPath = temp.filePath("project.ikclutproj");
    QString error;
    if (!ProjectSerializer::saveProject(document, projectPath, &error)) {
        return fail("failed to save project: " + error);
    }
    ImageDocument loaded;
    if (!ProjectSerializer::loadProject(projectPath, &loaded, &error)) {
        return fail("failed to load project: " + error);
    }
    if (loaded.pipeline.stages.isEmpty() || loaded.importedLuts.isEmpty()) {
        return fail("project round trip lost extensibility fields");
    }
    if (loaded.pipeline.stages.first().label != "Skin Qualifier") {
        return fail("project round trip lost adjustment stage label");
    }
    if (loaded.pipeline.stages.first().blendMode != "color" || !loaded.pipeline.stages.first().solo) {
        return fail("project round trip lost adjustment blend or solo settings");
    }
    if (loaded.pipeline.params.masterCurve.size() != 3 || loaded.pipeline.params.hslSaturation[2] < 0.34f) {
        return fail("project round trip lost curve or HSL params");
    }
    if (loaded.pipeline.params.inputColorSpace != "logc3") {
        return fail("project round trip lost input color space");
    }
    if (loaded.pipeline.params.colorWarpSourceLuma.size() != 1
        || loaded.pipeline.params.colorWarpTargetLuma.size() != 1
        || loaded.pipeline.params.colorWarpTargetLuma.first() < 0.69f) {
        return fail("project round trip lost Warper luma params");
    }
    if (loaded.pipeline.params.importedLutStrength < 0.41f || loaded.pipeline.params.activeImportedLutAssetId != "lut-1") {
        return fail("project round trip lost imported LUT mix settings");
    }
    if (loaded.pipeline.params.logDark > -0.24f
        || loaded.pipeline.params.hdrHighlight < 0.31f
        || loaded.pipeline.params.printerBlue > -2.9f
        || loaded.pipeline.params.skinToneHue < 5.9f) {
        return fail("project round trip lost professional grading params");
    }
    if (loaded.pipeline.params.masterCurveHandles.size() != 3
        || loaded.pipeline.params.masterCurveHandles.at(0) != CurveHandleMode::Auto
        || loaded.pipeline.params.masterCurveHandles.at(1) != CurveHandleMode::Vector) {
        return fail("project round trip lost curve handle modes");
    }
    if (loaded.masks.isEmpty() || loaded.pipeline.stages.first().maskRef.id != "mask-full") {
        return fail("project round trip lost mask assets or references");
    }
    if (!loaded.nodeGraph.enabled || loaded.nodeGraph.nodes.size() != 3 || loaded.nodeGraph.edges.size() != 2) {
        return fail("project round trip lost node graph");
    }
    ImageDocument reportDocument = loaded;
    reportDocument.pipeline.params.masterCurve = {QPointF(0.0, 0.0), QPointF(0.45, 0.9), QPointF(1.0, 1.0)};
    reportDocument.pipeline.stages.first().maskRef.id = "report-radial";
    MaskAsset reportMask;
    reportMask.maskId = "report-radial";
    reportMask.kind = MaskKind::Radial;
    reportDocument.masks.append(reportMask);
    const LutQualityReport exportReport = LutQualityAnalyzer::analyzeExport(Lut3D::identity(17), reportDocument, true, "CUBE", "look.cube");
    if (!exportReport.hasSpatialMasks || !exportReport.usesImportedLut || !exportReport.hasStrongCurves || exportReport.warningCount < 3) {
        return fail("export quality report did not flag project-level export risks");
    }
    const ExecutionPlan graphPlan = ExecutionPlanCompiler::compileDocument(loaded);
    if (!graphPlan.fromNodeGraph || graphPlan.steps.size() < 2) {
        return fail("node graph compiler did not produce execution steps");
    }
    ImageDocument reorderedGraphDoc = loaded;
    reorderedGraphDoc.nodeGraph.nodes.clear();
    reorderedGraphDoc.nodeGraph.edges.clear();
    reorderedGraphDoc.nodeGraph.outputNodeId = "node-output";
    reorderedGraphDoc.nodeGraph.nodes.append(NodeGraphNode{"node-input", "input", "Input", QPointF(0, 0), true, QJsonObject{}});
    reorderedGraphDoc.nodeGraph.nodes.append(NodeGraphNode{"stage-a", "globalColorGrade", "A", QPointF(420, 0), true, QJsonObject{}});
    reorderedGraphDoc.nodeGraph.nodes.append(NodeGraphNode{"stage-b", "globalColorGrade", "B", QPointF(220, 0), true, QJsonObject{}});
    reorderedGraphDoc.nodeGraph.nodes.append(NodeGraphNode{"node-output", "output", "Output", QPointF(640, 0), true, QJsonObject{}});
    reorderedGraphDoc.nodeGraph.edges.append(NodeGraphEdge{"edge-1", NodeGraphPortRef{"node-input", "out"}, NodeGraphPortRef{"stage-b", "in"}});
    reorderedGraphDoc.nodeGraph.edges.append(NodeGraphEdge{"edge-2", NodeGraphPortRef{"stage-b", "out"}, NodeGraphPortRef{"stage-a", "in"}});
    reorderedGraphDoc.nodeGraph.edges.append(NodeGraphEdge{"edge-3", NodeGraphPortRef{"stage-a", "out"}, NodeGraphPortRef{"node-output", "in"}});
    const ExecutionPlan reorderedPlan = ExecutionPlanCompiler::compileDocument(reorderedGraphDoc);
    if (reorderedPlan.steps.size() < 3
        || reorderedPlan.steps.at(0).stepId != "stage-b"
        || reorderedPlan.steps.at(1).stepId != "stage-a") {
        return fail("node graph compiler did not follow graph edge order");
    }
    ImageDocument cyclicGraphDoc = loaded;
    cyclicGraphDoc.nodeGraph.nodes.clear();
    cyclicGraphDoc.nodeGraph.edges.clear();
    cyclicGraphDoc.nodeGraph.outputNodeId = "node-output";
    cyclicGraphDoc.nodeGraph.nodes.append(NodeGraphNode{"node-input", "input", "Input", QPointF(0, 0), true, QJsonObject{}});
    cyclicGraphDoc.nodeGraph.nodes.append(NodeGraphNode{"cycle-a", "globalColorGrade", "A", QPointF(220, 0), true, QJsonObject{}});
    cyclicGraphDoc.nodeGraph.nodes.append(NodeGraphNode{"cycle-b", "globalColorGrade", "B", QPointF(440, 0), true, QJsonObject{}});
    cyclicGraphDoc.nodeGraph.nodes.append(NodeGraphNode{"node-output", "output", "Output", QPointF(660, 0), true, QJsonObject{}});
    cyclicGraphDoc.nodeGraph.edges.append(NodeGraphEdge{"edge-1", NodeGraphPortRef{"node-input", "out"}, NodeGraphPortRef{"cycle-a", "in"}});
    cyclicGraphDoc.nodeGraph.edges.append(NodeGraphEdge{"edge-2", NodeGraphPortRef{"cycle-a", "out"}, NodeGraphPortRef{"cycle-b", "in"}});
    cyclicGraphDoc.nodeGraph.edges.append(NodeGraphEdge{"edge-3", NodeGraphPortRef{"cycle-b", "out"}, NodeGraphPortRef{"cycle-a", "in"}});
    const ExecutionPlan cyclicPlan = ExecutionPlanCompiler::compileDocument(cyclicGraphDoc);
    if (!cyclicPlan.diagnostic.contains("Cycle detected")) {
        return fail("node graph compiler did not diagnose graph cycles");
    }
    ImageDocument disconnectedGraphDoc = cyclicGraphDoc;
    disconnectedGraphDoc.nodeGraph.edges.clear();
    disconnectedGraphDoc.nodeGraph.edges.append(NodeGraphEdge{"edge-1", NodeGraphPortRef{"node-input", "out"}, NodeGraphPortRef{"cycle-a", "in"}});
    const ExecutionPlan disconnectedPlan = ExecutionPlanCompiler::compileDocument(disconnectedGraphDoc);
    if (!disconnectedPlan.diagnostic.contains("Output is not connected")) {
        return fail("node graph compiler did not diagnose disconnected output");
    }
    loaded.nodeGraph.enabled = false;
    const ExecutionPlan linearPlan = ExecutionPlanCompiler::compileDocument(loaded);
    if (linearPlan.fromNodeGraph || linearPlan.steps.isEmpty()) {
        return fail("linear pipeline compiler did not produce execution steps");
    }
    const float maskValue = MaskEvaluator::evaluate(loaded.masks, loaded.pipeline.stages.first().maskRef, QPointF(0.5, 0.5));
    if (maskValue < 0.59f || maskValue > 0.61f) {
        return fail("mask evaluator did not combine asset and reference opacity");
    }
    QVector<MaskAsset> localMasks;
    MaskReference localRef;
    localRef.id = "local";
    MaskAsset gradientMask;
    gradientMask.maskId = "local";
    gradientMask.kind = MaskKind::Gradient;
    gradientMask.params = QJsonObject{{"angle", 0.0}, {"position", 0.5}, {"softness", 0.05}};
    localMasks = {gradientMask};
    if (MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.85, 0.5)) < 0.95f
        || MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.15, 0.5)) > 0.05f) {
        return fail("gradient mask evaluator did not follow projection");
    }
    MaskAsset radialMask = gradientMask;
    radialMask.kind = MaskKind::Radial;
    radialMask.params = QJsonObject{{"centerX", 0.5}, {"centerY", 0.5}, {"radius", 0.2}, {"softness", 0.05}};
    localMasks = {radialMask};
    if (MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.5, 0.5)) < 0.95f
        || MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.95, 0.95)) > 0.05f) {
        return fail("radial mask evaluator did not follow distance");
    }
    MaskAsset paintMask = gradientMask;
    paintMask.kind = MaskKind::Paint;
    paintMask.params = QJsonObject{{"radius", 0.05},
                                   {"softness", 0.02},
                                   {"strokes", QJsonArray{
                                       QJsonObject{{"x1", 0.2}, {"y1", 0.5}, {"x2", 0.8}, {"y2", 0.5}, {"radius", 0.05}, {"softness", 0.02}}
                                   }}};
    localMasks = {paintMask};
    if (MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.5, 0.52)) < 0.75f
        || MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.5, 0.75)) > 0.05f) {
        return fail("paint mask evaluator did not follow brush strokes");
    }
    paintMask.params["strokes"] = QJsonArray{
        QJsonObject{{"x1", 0.2}, {"y1", 0.5}, {"x2", 0.8}, {"y2", 0.5}, {"radius", 0.08}, {"softness", 0.02}},
        QJsonObject{{"x1", 0.45}, {"y1", 0.5}, {"x2", 0.55}, {"y2", 0.5}, {"radius", 0.08}, {"softness", 0.02}, {"subtract", true}}
    };
    localMasks = {paintMask};
    if (MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.5, 0.5)) > 0.05f
        || MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.25, 0.5)) < 0.75f) {
        return fail("paint mask subtract strokes did not remove painted areas");
    }
    QImage externalMaskImage(4, 1, QImage::Format_ARGB32);
    externalMaskImage.setPixel(0, 0, qRgba(0, 0, 0, 255));
    externalMaskImage.setPixel(1, 0, qRgba(64, 64, 64, 255));
    externalMaskImage.setPixel(2, 0, qRgba(192, 192, 192, 255));
    externalMaskImage.setPixel(3, 0, qRgba(255, 255, 255, 128));
    const QString externalMaskPath = temp.filePath("external-mask.png");
    if (!externalMaskImage.save(externalMaskPath)) {
        return fail("failed to write external mask image");
    }
    MaskAsset externalMask = gradientMask;
    externalMask.kind = MaskKind::ExternalImage;
    externalMask.params = QJsonObject{{"imagePath", externalMaskPath}};
    localMasks = {externalMask};
    if (MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.0, 0.0)) > 0.01f
        || MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.66, 0.0)) < 0.70f
        || MaskEvaluator::evaluate(localMasks, localRef, QPointF(1.0, 0.0)) > 0.55f) {
        return fail("external image mask evaluator did not sample luma and alpha");
    }
    MaskAsset colorMask = gradientMask;
    colorMask.kind = MaskKind::ColorRange;
    colorMask.params = QJsonObject{{"hue", 0.0}, {"saturation", 1.0}, {"luma", 0.2126}, {"tolerance", 0.18}, {"softness", 0.08}};
    localMasks = {colorMask};
    if (MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.0, 0.0), QVector3D(1.0f, 0.0f, 0.0f)) < 0.8f
        || MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.0, 0.0), QVector3D(0.0f, 1.0f, 0.0f)) > 0.2f) {
        return fail("color range mask evaluator did not isolate target color");
    }
    colorMask.params["samples"] = QJsonArray{
        QJsonObject{{"hue", 1.0 / 3.0}, {"saturation", 1.0}, {"luma", 0.7152}, {"subtract", true}}
    };
    localMasks = {colorMask};
    if (MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.0, 0.0), QVector3D(1.0f, 0.0f, 0.0f)) < 0.8f
        || MaskEvaluator::evaluate(localMasks, localRef, QPointF(0.0, 0.0), QVector3D(0.0f, 1.0f, 0.0f)) > 0.05f) {
        return fail("color range subtract samples did not remove excluded color");
    }
    ColorGradePipeline maskedPipeline;
    ColorGradeParams maskedStageParams;
    maskedStageParams.exposure = 1.0f;
    ColorGradeStage maskedStage;
    maskedStage.stageId = "stage-mask";
    maskedStage.params = paramsToJson(maskedStageParams);
    maskedStage.maskRef.id = "center-mask";
    maskedPipeline.stages.append(maskedStage);
    MaskAsset centerMask;
    centerMask.maskId = "center-mask";
    centerMask.kind = MaskKind::Radial;
    centerMask.params = QJsonObject{{"centerX", 0.5}, {"centerY", 0.5}, {"radius", 0.2}, {"softness", 0.02}};
    const QVector<MaskAsset> centerMasks = {centerMask};
    const QVector3D centerGrade = ColorPipeline::applyPipeline(QVector3D(0.25f, 0.25f, 0.25f), maskedPipeline, centerMasks, QPointF(0.5, 0.5));
    const QVector3D edgeGrade = ColorPipeline::applyPipeline(QVector3D(0.25f, 0.25f, 0.25f), maskedPipeline, centerMasks, QPointF(0.95, 0.95));
    if (centerGrade.x() <= edgeGrade.x() + 0.18f) {
        return fail("per-stage radial mask did not localize pipeline adjustment");
    }
    const QVector3D spatialExportSample = ColorPipeline::bakePipelineLut(maskedPipeline, centerMasks, 16)
                                               .sample(QVector3D(0.25f, 0.25f, 0.25f));
    if (spatialExportSample.x() > 0.32f) {
        return fail("spatial masked stage should not be baked globally into export LUT");
    }
    MaskAsset exportColorMask = colorMask;
    exportColorMask.maskId = "red-key";
    ColorGradeStage colorKeyStage = maskedStage;
    colorKeyStage.maskRef.id = "red-key";
    maskedPipeline.stages = {colorKeyStage};
    const Lut3D colorKeyLut = ColorPipeline::bakePipelineLut(maskedPipeline, QVector<MaskAsset>{exportColorMask}, 16);
    const QVector3D redKeyExport = colorKeyLut.sample(QVector3D(0.5f, 0.0f, 0.0f));
    const QVector3D greenKeyExport = colorKeyLut.sample(QVector3D(0.0f, 0.5f, 0.0f));
    if (redKeyExport.x() <= greenKeyExport.y() + 0.15f) {
        return fail("color range mask should remain exportable in LUT bake");
    }
    ColorGradePipeline blendPipeline;
    ColorGradeStage colorBlendStage;
    ColorGradeParams colorBlendParams;
    colorBlendParams.temperature = 1.0f;
    colorBlendStage.params = paramsToJson(colorBlendParams);
    colorBlendStage.blendMode = "luminosity";
    blendPipeline.stages.append(colorBlendStage);
    const QVector3D luminosityBlend = ColorPipeline::applyPipeline(QVector3D(0.2f, 0.4f, 0.9f), blendPipeline);
    if (luminosityBlend.z() <= luminosityBlend.x()) {
        return fail("luminosity blend mode should preserve source hue dominance");
    }

    ColorGradePipeline nodePipeline;
    ColorGradeStage brightnessNode;
    brightnessNode.stageId = "brightness-node";
    brightnessNode.type = "brightnessContrast";
    brightnessNode.params = QJsonObject{{"brightness", 0.15}, {"contrast", 0.0}};
    nodePipeline.stages.append(brightnessNode);
    const QVector3D brightenedNode = ColorPipeline::applyPipeline(QVector3D(0.25f, 0.25f, 0.25f), nodePipeline);
    if (brightenedNode.x() < 0.39f || brightenedNode.x() > 0.41f) {
        return fail("brightness/contrast node did not apply standalone node params");
    }

    ColorGradeStage mixNode;
    mixNode.stageId = "mix-node";
    mixNode.type = "mixColor";
    mixNode.params = QJsonObject{{"blendType", "multiply"},
                                 {"factor", 1.0},
                                 {"colorB", QJsonArray{0.5, 1.0, 1.0}},
                                 {"useFlowAsA", true},
                                 {"clampResult", true}};
    nodePipeline.stages = {mixNode};
    const QVector3D mixedNode = ColorPipeline::applyPipeline(QVector3D(0.8f, 0.4f, 0.2f), nodePipeline);
    if (mixedNode.x() > 0.41f || mixedNode.y() < 0.39f || mixedNode.z() < 0.19f) {
        return fail("mix color node multiply mode did not combine the flow color with Color B");
    }

    ColorGradeStage correctionNode;
    correctionNode.stageId = "correction-node";
    correctionNode.type = "colorCorrection";
    correctionNode.params = QJsonObject{{"masterSaturation", 0.0}, {"masterValue", 1.0}, {"masterContrast", 0.0}, {"masterGamma", 1.0}};
    nodePipeline.stages = {correctionNode};
    const QVector3D correctedNode = ColorPipeline::applyPipeline(QVector3D(0.9f, 0.2f, 0.2f), nodePipeline);
    if (std::abs(correctedNode.x() - correctedNode.y()) > 0.01f || std::abs(correctedNode.y() - correctedNode.z()) > 0.01f) {
        return fail("color correction node master saturation did not neutralize chroma");
    }

    ColorGradeStage levelsNode;
    levelsNode.stageId = "levels-node";
    levelsNode.type = "levelsColor";
    levelsNode.params = QJsonObject{{"blackPoint", 0.25}, {"whitePoint", 0.75}, {"gamma", 1.0}, {"outputBlack", 0.0}, {"outputWhite", 1.0}};
    nodePipeline.stages = {levelsNode};
    const QVector3D leveledNode = ColorPipeline::applyPipeline(QVector3D(0.5f, 0.25f, 0.75f), nodePipeline);
    if (leveledNode.x() < 0.49f || leveledNode.y() > 0.01f || leveledNode.z() < 0.99f) {
        return fail("levels node did not remap input black and white points");
    }

    ColorGradeStage posterizeNode;
    posterizeNode.stageId = "posterize-node";
    posterizeNode.type = "posterizeColor";
    posterizeNode.params = QJsonObject{{"steps", 2.0}};
    nodePipeline.stages = {posterizeNode};
    const QVector3D posterizedNode = ColorPipeline::applyPipeline(QVector3D(0.2f, 0.6f, 0.9f), nodePipeline);
    if (posterizedNode.x() > 0.01f || posterizedNode.y() < 0.99f || posterizedNode.z() < 0.99f) {
        return fail("posterize node did not quantize channels");
    }

    ColorGradeStage grayscaleNode;
    grayscaleNode.stageId = "grayscale-node";
    grayscaleNode.type = "grayscaleColor";
    grayscaleNode.params = QJsonObject{{"factor", 1.0}};
    nodePipeline.stages = {grayscaleNode};
    const QVector3D grayscale = ColorPipeline::applyPipeline(QVector3D(1.0f, 0.0f, 0.0f), nodePipeline);
    if (std::abs(grayscale.x() - grayscale.y()) > 0.01f || std::abs(grayscale.y() - grayscale.z()) > 0.01f) {
        return fail("grayscale node did not output equal RGB channels");
    }

    ColorGradeStage channelMixerNode;
    channelMixerNode.stageId = "channel-mixer-node";
    channelMixerNode.type = "channelMixer";
    channelMixerNode.params = QJsonObject{{"red", QJsonArray{0.0, 1.0, 0.0}},
                                          {"green", QJsonArray{1.0, 0.0, 0.0}},
                                          {"blue", QJsonArray{0.0, 0.0, 1.0}}};
    nodePipeline.stages = {channelMixerNode};
    const QVector3D swapped = ColorPipeline::applyPipeline(QVector3D(0.2f, 0.7f, 0.4f), nodePipeline);
    if (swapped.x() < 0.69f || swapped.y() > 0.21f || swapped.z() < 0.39f) {
        return fail("channel mixer node did not remap output channels");
    }

    ColorGradeStage cdlNode;
    cdlNode.stageId = "cdl-node";
    cdlNode.type = "ascCdl";
    cdlNode.params = QJsonObject{{"slope", QJsonArray{2.0, 1.0, 1.0}},
                                 {"offset", QJsonArray{0.0, 0.0, 0.0}},
                                 {"power", QJsonArray{1.0, 1.0, 1.0}},
                                 {"saturation", 1.0}};
    nodePipeline.stages = {cdlNode};
    const QVector3D cdl = ColorPipeline::applyPipeline(QVector3D(0.25f, 0.25f, 0.25f), nodePipeline);
    if (cdl.x() < 0.49f || cdl.y() > 0.26f) {
        return fail("ASC CDL node did not apply slope");
    }

    ColorGradeStage rampNode;
    rampNode.stageId = "ramp-node";
    rampNode.type = "colorRamp";
    rampNode.params = QJsonObject{{"blackPoint", 0.0},
                                  {"whitePoint", 1.0},
                                  {"shadowColor", QJsonArray{0.0, 0.0, 1.0}},
                                  {"highlightColor", QJsonArray{1.0, 0.0, 0.0}}};
    nodePipeline.stages = {rampNode};
    const QVector3D ramp = ColorPipeline::applyPipeline(QVector3D(1.0f, 1.0f, 1.0f), nodePipeline);
    if (ramp.x() < 0.99f || ramp.z() > 0.01f) {
        return fail("color ramp node did not map luma to colors");
    }

    ColorGradeStage mixBranchNode;
    mixBranchNode.stageId = "mix-branch-node";
    mixBranchNode.type = "mixColor";
    mixBranchNode.params = QJsonObject{{"blendType", "mix"},
                                       {"factor", 1.0},
                                       {"branchBStages", QJsonArray{
                                           QJsonObject{{"stageId", "gray-branch"}, {"type", "grayscaleColor"}, {"enabled", true}, {"params", QJsonObject{{"factor", 1.0}}}}
                                       }}};
    nodePipeline.stages = {mixBranchNode};
    const QVector3D branchMix = ColorPipeline::applyPipeline(QVector3D(1.0f, 0.0f, 0.0f), nodePipeline);
    if (std::abs(branchMix.x() - branchMix.y()) > 0.01f || std::abs(branchMix.y() - branchMix.z()) > 0.01f) {
        return fail("mix color node did not evaluate embedded B branch");
    }

    QJsonObject brightBranchStage{{"stageId", "bright-branch"},
                                  {"type", "brightnessContrast"},
                                  {"enabled", true},
                                  {"opacity", 0.5},
                                  {"blendMode", "normal"},
                                  {"params", QJsonObject{{"brightness", 0.4}, {"contrast", 0.0}}}};
    QJsonObject invertBranchStage{{"stageId", "invert-branch"},
                                  {"type", "invertColor"},
                                  {"enabled", true},
                                  {"params", QJsonObject{{"factor", 1.0}}}};
    ColorGradeStage parallelMixer;
    parallelMixer.stageId = "parallel-mixer";
    parallelMixer.type = "parallelMixer";
    parallelMixer.params = QJsonObject{{"branches", QJsonArray{QJsonArray{brightBranchStage}, QJsonArray{invertBranchStage}}}};
    nodePipeline.stages = {parallelMixer};
    const QVector3D parallelMixed = ColorPipeline::applyPipeline(QVector3D(0.2f, 0.2f, 0.2f), nodePipeline);
    if (parallelMixed.x() < 0.59f || parallelMixed.x() > 0.61f) {
        return fail("parallel mixer did not average executable branches");
    }
    parallelMixer.params["mix"] = 0.5;
    parallelMixer.params["weights"] = QJsonArray{1.0, 0.0};
    nodePipeline.stages = {parallelMixer};
    const QVector3D weightedParallel = ColorPipeline::applyPipeline(QVector3D(0.2f, 0.2f, 0.2f), nodePipeline);
    if (weightedParallel.x() < 0.29f || weightedParallel.x() > 0.31f) {
        return fail("parallel mixer did not apply branch weights and mix strength");
    }

    ColorGradeStage layerMixer = parallelMixer;
    layerMixer.stageId = "layer-mixer";
    layerMixer.type = "layerMixer";
    layerMixer.params["mix"] = 1.0;
    layerMixer.params["weights"] = QJsonArray{1.0, 1.0};
    nodePipeline.stages = {layerMixer};
    const QVector3D layerMixed = ColorPipeline::applyPipeline(QVector3D(0.2f, 0.2f, 0.2f), nodePipeline);
    if (layerMixed.x() < 0.99f) {
        return fail("layer mixer did not accumulate executable branch deltas");
    }
    layerMixer.params["mix"] = 0.5;
    layerMixer.params["weights"] = QJsonArray{0.0, 1.0};
    nodePipeline.stages = {layerMixer};
    const QVector3D weightedLayer = ColorPipeline::applyPipeline(QVector3D(0.2f, 0.2f, 0.2f), nodePipeline);
    if (weightedLayer.x() < 0.49f || weightedLayer.x() > 0.51f) {
        return fail("layer mixer did not apply branch weights and mix strength");
    }

    Lut3D whiteToBlack(2);
    for (int b = 0; b < 2; ++b) {
        for (int g = 0; g < 2; ++g) {
            for (int r = 0; r < 2; ++r) {
                whiteToBlack.setValue(r, g, b, QVector3D(0.0f, 0.0f, 0.0f));
            }
        }
    }
    ColorGradeStage lutMixNode;
    lutMixNode.stageId = "lut-mix-node";
    lutMixNode.type = "lutMix";
    lutMixNode.params = QJsonObject{{"strength", 1.0}};
    ColorTransformSource lutMixSource;
    lutMixSource.usePipeline = true;
    lutMixSource.pipeline.stages = {lutMixNode};
    const QVector3D lutMixed = ColorPipeline::applyTransform(QVector3D(0.8f, 0.7f, 0.6f), lutMixSource, &whiteToBlack);
    if (lutMixed.x() > 0.01f || lutMixed.y() > 0.01f || lutMixed.z() > 0.01f) {
        return fail("LUT Mix node did not sample the imported LUT");
    }

    return 0;
}
