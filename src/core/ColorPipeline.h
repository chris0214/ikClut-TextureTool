#pragma once

#include "core/Lut3D.h"
#include "model/ColorTypes.h"

#include <QImage>

namespace ikclut {

class ColorPipeline {
public:
    static QVector3D applyParams(const QVector3D& color, const ColorGradeParams& params);
    static QVector3D applyPipeline(const QVector3D& color, const ColorGradePipeline& pipeline);
    static QVector3D applyPipeline(const QVector3D& color, const ColorGradePipeline& pipeline, const QVector<MaskAsset>& masks, const QPointF& uv);
    static QVector3D applyPipeline(const QVector3D& color, const ColorGradePipeline& pipeline, const QVector<MaskAsset>& masks, const QPointF& uv, const Lut3D* importedLut);
    static QVector3D applyTransform(const QVector3D& color, const ColorTransformSource& source, const Lut3D* importedLut);
    static Lut3D bakeLut(const ColorGradeParams& params, int size);
    static Lut3D bakePipelineLut(const ColorGradePipeline& pipeline, int size);
    static Lut3D bakePipelineLut(const ColorGradePipeline& pipeline, const QVector<MaskAsset>& masks, int size);
    static Lut3D composeLut(const ColorGradeParams& params, const Lut3D& importedLut, float importedLutStrength, int size);
    static Lut3D composePipelineLut(const ColorGradePipeline& pipeline, const Lut3D& importedLut, float importedLutStrength, int size);
    static Lut3D composePipelineLut(const ColorGradePipeline& pipeline, const QVector<MaskAsset>& masks, const Lut3D& importedLut, float importedLutStrength, int size);
    static QImage applyToImage(const QImage& input, const ColorTransformSource& source, const Lut3D* importedLut);
    static QImage applyLutToImage(const QImage& input, const Lut3D& lut);
};

} // namespace ikclut
