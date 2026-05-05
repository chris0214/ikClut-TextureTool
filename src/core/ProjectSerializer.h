#pragma once

#include "model/ColorTypes.h"

#include <QString>

namespace ikclut {

class ProjectSerializer {
public:
    static bool saveProject(const ImageDocument& document, const QString& path, QString* error);
    static bool loadProject(const QString& path, ImageDocument* document, QString* error);
    static bool savePreset(const ColorGradePipeline& pipeline, const QString& path, QString* error);
    static bool loadPreset(const QString& path, ColorGradePipeline* pipeline, QString* error);

private:
    static QJsonObject pipelineToJson(const ColorGradePipeline& pipeline);
    static ColorGradePipeline pipelineFromJson(const QJsonObject& obj);
};

} // namespace ikclut

