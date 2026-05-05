#pragma once

#include "core/Lut3D.h"
#include "model/ColorTypes.h"

#include <QImage>
#include <QElapsedTimer>

namespace ikclut {

enum class PreviewQuality {
    Interactive,
    Full
};

struct PreviewRenderRequest {
    QImage source;
    ColorTransformSource transform;
    Lut3D importedLut;
    QVector<MaskAsset> masks;
    MaskReference maskRef;
    PreviewQuality quality = PreviewQuality::Full;
    QSize maxInteractiveSize = QSize(1280, 720);
    bool preferGpu = true;
};

struct PreviewRenderResult {
    QImage image;
    PreviewQuality quality = PreviewQuality::Full;
    QSize sourceSize;
    Lut3D exportLut;
    Lut3D previewLut;
    bool usedGpu = false;
    QString diagnostic;
    qint64 elapsedMs = 0;
    qint64 scaleMs = 0;
    qint64 lutMs = 0;
    qint64 renderMs = 0;
};

class PreviewRenderer {
public:
    static PreviewRenderResult render(const PreviewRenderRequest& request);
};

} // namespace ikclut
