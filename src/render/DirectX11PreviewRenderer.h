#pragma once

#include "core/Lut3D.h"

#include <QImage>
#include <QString>

namespace ikclut {

class DirectX11PreviewRenderer {
public:
    static bool render(const QImage& source, const Lut3D& lut, QImage* output, QString* error);
    static bool render(const QImage& source, const Lut3D& lut, QImage* output, QString* error, QString* diagnostic);
};

} // namespace ikclut
