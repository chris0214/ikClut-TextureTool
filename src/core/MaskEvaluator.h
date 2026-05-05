#pragma once

#include "model/ColorTypes.h"

#include <QPointF>
#include <QVector3D>

namespace ikclut {

class MaskEvaluator {
public:
    static float evaluate(const QVector<MaskAsset>& masks, const MaskReference& ref, const QPointF& uv);
    static float evaluate(const QVector<MaskAsset>& masks, const MaskReference& ref, const QPointF& uv, const QVector3D& color);
};

} // namespace ikclut
