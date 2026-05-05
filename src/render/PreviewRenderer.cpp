#include "render/PreviewRenderer.h"

#include "core/ColorPipeline.h"
#include "core/IkClutExporter.h"
#include "core/MaskEvaluator.h"
#include "render/DirectX11PreviewRenderer.h"

#include <algorithm>

namespace ikclut {

namespace {

QImage scaledForInteractive(const QImage& source, const QSize& maxSize)
{
    if (source.isNull() || maxSize.isEmpty()) {
        return source;
    }
    if (source.width() <= maxSize.width() && source.height() <= maxSize.height()) {
        return source;
    }
    return source.scaled(maxSize, Qt::KeepAspectRatio, Qt::FastTransformation);
}

bool hasActiveMask(const PreviewRenderRequest& request)
{
    if (!request.maskRef.id.isEmpty()) {
        return true;
    }
    if (!request.transform.usePipeline) {
        return false;
    }
    for (const ColorGradeStage& stage : request.transform.pipeline.stages) {
        if (!stage.enabled || stage.opacity <= 0.0f || stage.maskRef.id.isEmpty()) {
            continue;
        }
        const auto it = std::find_if(request.masks.begin(), request.masks.end(), [&](const MaskAsset& mask) {
            return mask.maskId == stage.maskRef.id;
        });
        if (it != request.masks.end() && it->enabled && it->kind != MaskKind::Full && it->opacity > 0.0f) {
            return true;
        }
    }
    return false;
}

QImage applyMaskedLutToImage(const QImage& input, const Lut3D& lut, const PreviewRenderRequest& request)
{
    if (input.isNull()) {
        return QImage();
    }
    if (!lut.isValid()) {
        return input.convertToFormat(QImage::Format_ARGB32);
    }

    ColorGradeParams baseParams;
    baseParams.inputColorSpace = request.transform.params.inputColorSpace;
    ColorTransformSource baseSource;
    baseSource.kind = ColorTransformKind::ParamPipeline;
    baseSource.params = baseParams;

    QImage src = input.convertToFormat(QImage::Format_ARGB32);
    QImage out(src.size(), QImage::Format_ARGB32);
    const int width = std::max(1, src.width() - 1);
    const int height = std::max(1, src.height() - 1);
    for (int y = 0; y < src.height(); ++y) {
        const QRgb* inLine = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        QRgb* outLine = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            const QRgb p = inLine[x];
            const QVector3D raw(qRed(p) / 255.0f, qGreen(p) / 255.0f, qBlue(p) / 255.0f);
            const QPointF uv(x / static_cast<double>(width), y / static_cast<double>(height));
            const float mask = request.transform.usePipeline
                ? 1.0f
                : MaskEvaluator::evaluate(request.masks, request.maskRef, uv, raw);
            const QVector3D base = ColorPipeline::applyTransform(raw, baseSource, nullptr);
            QVector3D graded = request.transform.usePipeline
                ? ColorPipeline::applyPipeline(raw, request.transform.pipeline, request.masks, uv)
                : lut.sample(raw);
            if (request.transform.usePipeline
                && request.transform.kind == ColorTransformKind::ImportedLut
                && request.importedLut.isValid()
                && request.transform.importedLutStrength > 0.0f) {
                const float strength = std::clamp(request.transform.importedLutStrength, 0.0f, 1.0f);
                const QVector3D lutted = request.importedLut.sample(graded);
                graded = graded * (1.0f - strength) + lutted * strength;
            }
            const QVector3D c = base * (1.0f - mask) + graded * mask;
            outLine[x] = qRgba(static_cast<int>(std::clamp(c.x(), 0.0f, 1.0f) * 255.0f + 0.5f),
                               static_cast<int>(std::clamp(c.y(), 0.0f, 1.0f) * 255.0f + 0.5f),
                               static_cast<int>(std::clamp(c.z(), 0.0f, 1.0f) * 255.0f + 0.5f),
                               qAlpha(p));
        }
    }
    return out;
}

} // namespace

PreviewRenderResult PreviewRenderer::render(const PreviewRenderRequest& request)
{
    QElapsedTimer timer;
    timer.start();

    PreviewRenderResult result;
    result.quality = request.quality;
    result.sourceSize = request.source.size();

    const QImage working = request.quality == PreviewQuality::Interactive
        ? scaledForInteractive(request.source, request.maxInteractiveSize)
        : request.source;
    result.scaleMs = timer.elapsed();

    const int previewLutSize = request.quality == PreviewQuality::Interactive ? 17 : 32;
    const Lut3D previewLut = request.transform.kind == ColorTransformKind::ImportedLut && request.importedLut.isValid()
        ? (request.transform.usePipeline
              ? ColorPipeline::composePipelineLut(request.transform.pipeline, request.masks, request.importedLut, request.transform.importedLutStrength, previewLutSize)
              : ColorPipeline::composeLut(request.transform.params, request.importedLut, request.transform.importedLutStrength, previewLutSize))
        : (request.transform.usePipeline
              ? ColorPipeline::bakePipelineLut(request.transform.pipeline, request.masks, previewLutSize)
              : ColorPipeline::bakeLut(request.transform.params, previewLutSize));
    result.exportLut = previewLut;
    result.previewLut = previewLut;
    result.lutMs = timer.elapsed() - result.scaleMs;

    const bool masked = hasActiveMask(request);
    if (request.preferGpu && previewLut.isValid() && !masked) {
        const qint64 renderStart = timer.elapsed();
        QString error;
        QString diagnostic;
        QImage gpuImage;
        if (DirectX11PreviewRenderer::render(working, previewLut, &gpuImage, &error, &diagnostic)) {
            result.image = gpuImage;
            result.usedGpu = true;
            result.diagnostic = diagnostic;
            result.renderMs = timer.elapsed() - renderStart;
            result.elapsedMs = timer.elapsed();
            return result;
        }
        result.diagnostic = error;
    }

    const qint64 renderStart = timer.elapsed();
    result.image = masked
        ? applyMaskedLutToImage(working, previewLut, request)
        : ColorPipeline::applyLutToImage(working, previewLut);
    result.renderMs = timer.elapsed() - renderStart;
    result.elapsedMs = timer.elapsed();
    return result;
}

} // namespace ikclut
