#include "core/ProjectSerializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace ikclut {

namespace {

QString maskKindToString(MaskKind kind)
{
    switch (kind) {
    case MaskKind::Gradient:
        return "gradient";
    case MaskKind::Radial:
        return "radial";
    case MaskKind::ColorRange:
        return "colorRange";
    case MaskKind::Paint:
        return "paint";
    case MaskKind::ExternalImage:
        return "externalImage";
    case MaskKind::Full:
    default:
        return "full";
    }
}

MaskKind maskKindFromString(const QString& kind)
{
    if (kind == "gradient") {
        return MaskKind::Gradient;
    }
    if (kind == "radial") {
        return MaskKind::Radial;
    }
    if (kind == "colorRange") {
        return MaskKind::ColorRange;
    }
    if (kind == "paint") {
        return MaskKind::Paint;
    }
    if (kind == "externalImage") {
        return MaskKind::ExternalImage;
    }
    return MaskKind::Full;
}

QJsonObject maskRefToJson(const MaskReference& ref)
{
    QJsonObject obj;
    obj["id"] = ref.id;
    obj["inverted"] = ref.inverted;
    obj["opacity"] = ref.opacity;
    return obj;
}

MaskReference maskRefFromJson(const QJsonValue& value)
{
    MaskReference ref;
    if (value.isString()) {
        ref.id = value.toString();
        return ref;
    }
    const QJsonObject obj = value.toObject();
    ref.id = obj.value("id").toString();
    ref.inverted = obj.value("inverted").toBool(false);
    ref.opacity = static_cast<float>(obj.value("opacity").toDouble(1.0));
    return ref;
}

QJsonObject stageToJson(const ColorGradeStage& stage)
{
    QJsonObject obj;
    obj["stageId"] = stage.stageId;
    obj["label"] = stage.label;
    obj["type"] = stage.type;
    obj["enabled"] = stage.enabled;
    obj["solo"] = stage.solo;
    obj["opacity"] = stage.opacity;
    obj["blendMode"] = stage.blendMode;
    obj["params"] = stage.params;
    obj["maskRef"] = maskRefToJson(stage.maskRef);
    obj["inputTransformRef"] = stage.inputTransformRef;
    return obj;
}

ColorGradeStage stageFromJson(const QJsonObject& obj)
{
    ColorGradeStage stage;
    stage.stageId = obj.value("stageId").toString();
    stage.label = obj.value("label").toString();
    stage.type = obj.value("type").toString();
    stage.enabled = obj.value("enabled").toBool(true);
    stage.solo = obj.value("solo").toBool(false);
    stage.opacity = std::clamp(static_cast<float>(obj.value("opacity").toDouble(1.0)), 0.0f, 1.0f);
    stage.blendMode = obj.value("blendMode").toString("normal");
    stage.params = obj.value("params").toObject();
    stage.maskRef = maskRefFromJson(obj.value("maskRef"));
    stage.inputTransformRef = obj.value("inputTransformRef").toString();
    return stage;
}

QJsonObject maskToJson(const MaskAsset& mask)
{
    QJsonObject obj;
    obj["maskId"] = mask.maskId;
    obj["kind"] = maskKindToString(mask.kind);
    obj["enabled"] = mask.enabled;
    obj["inverted"] = mask.inverted;
    obj["opacity"] = mask.opacity;
    obj["params"] = mask.params;
    return obj;
}

MaskAsset maskFromJson(const QJsonObject& obj)
{
    MaskAsset mask;
    mask.maskId = obj.value("maskId").toString();
    mask.kind = maskKindFromString(obj.value("kind").toString());
    mask.enabled = obj.value("enabled").toBool(true);
    mask.inverted = obj.value("inverted").toBool(false);
    mask.opacity = static_cast<float>(obj.value("opacity").toDouble(1.0));
    mask.params = obj.value("params").toObject();
    return mask;
}

QJsonObject portRefToJson(const NodeGraphPortRef& ref)
{
    QJsonObject obj;
    obj["nodeId"] = ref.nodeId;
    obj["portId"] = ref.portId;
    return obj;
}

NodeGraphPortRef portRefFromJson(const QJsonObject& obj)
{
    NodeGraphPortRef ref;
    ref.nodeId = obj.value("nodeId").toString();
    ref.portId = obj.value("portId").toString();
    return ref;
}

QJsonObject graphNodeToJson(const NodeGraphNode& node)
{
    QJsonObject obj;
    obj["nodeId"] = node.nodeId;
    obj["type"] = node.type;
    obj["label"] = node.label;
    obj["position"] = QJsonArray{node.position.x(), node.position.y()};
    obj["enabled"] = node.enabled;
    obj["params"] = node.params;
    return obj;
}

NodeGraphNode graphNodeFromJson(const QJsonObject& obj)
{
    NodeGraphNode node;
    node.nodeId = obj.value("nodeId").toString();
    node.type = obj.value("type").toString();
    node.label = obj.value("label").toString();
    const QJsonArray position = obj.value("position").toArray();
    if (position.size() == 2) {
        node.position = QPointF(position.at(0).toDouble(), position.at(1).toDouble());
    }
    node.enabled = obj.value("enabled").toBool(true);
    node.params = obj.value("params").toObject();
    return node;
}

QJsonObject graphEdgeToJson(const NodeGraphEdge& edge)
{
    QJsonObject obj;
    obj["edgeId"] = edge.edgeId;
    obj["from"] = portRefToJson(edge.from);
    obj["to"] = portRefToJson(edge.to);
    return obj;
}

NodeGraphEdge graphEdgeFromJson(const QJsonObject& obj)
{
    NodeGraphEdge edge;
    edge.edgeId = obj.value("edgeId").toString();
    edge.from = portRefFromJson(obj.value("from").toObject());
    edge.to = portRefFromJson(obj.value("to").toObject());
    return edge;
}

QJsonObject nodeGraphToJson(const NodeGraph& graph)
{
    QJsonObject obj;
    obj["enabled"] = graph.enabled;
    obj["outputNodeId"] = graph.outputNodeId;

    QJsonArray nodes;
    for (const NodeGraphNode& node : graph.nodes) {
        nodes.append(graphNodeToJson(node));
    }
    obj["nodes"] = nodes;

    QJsonArray edges;
    for (const NodeGraphEdge& edge : graph.edges) {
        edges.append(graphEdgeToJson(edge));
    }
    obj["edges"] = edges;
    return obj;
}

NodeGraph nodeGraphFromJson(const QJsonObject& obj)
{
    NodeGraph graph;
    graph.enabled = obj.value("enabled").toBool(false);
    graph.outputNodeId = obj.value("outputNodeId").toString();
    const QJsonArray nodes = obj.value("nodes").toArray();
    for (const QJsonValue& value : nodes) {
        graph.nodes.append(graphNodeFromJson(value.toObject()));
    }
    const QJsonArray edges = obj.value("edges").toArray();
    for (const QJsonValue& value : edges) {
        graph.edges.append(graphEdgeFromJson(value.toObject()));
    }
    return graph;
}

QJsonObject assetToJson(const ImportedLutAsset& asset)
{
    QJsonObject obj;
    obj["assetId"] = asset.assetId;
    obj["sourcePath"] = asset.sourcePath;
    obj["format"] = asset.format;
    obj["lutSize"] = asset.lutSize;
    obj["cached"] = asset.cached;
    obj["metadata"] = asset.metadata;
    return obj;
}

ImportedLutAsset assetFromJson(const QJsonObject& obj)
{
    ImportedLutAsset asset;
    asset.assetId = obj.value("assetId").toString();
    asset.sourcePath = obj.value("sourcePath").toString();
    asset.format = obj.value("format").toString();
    asset.lutSize = obj.value("lutSize").toInt();
    asset.cached = obj.value("cached").toBool(false);
    asset.metadata = obj.value("metadata").toObject();
    return asset;
}

bool writeJson(const QString& path, const QJsonObject& obj, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = "Failed to open file for writing.";
        }
        return false;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

bool readJson(const QString& path, QJsonObject* obj, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = "Failed to open file for reading.";
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = "Invalid JSON file.";
        }
        return false;
    }
    *obj = doc.object();
    return true;
}

} // namespace

bool ProjectSerializer::saveProject(const ImageDocument& document, const QString& path, QString* error)
{
    QJsonObject obj;
    obj["schema"] = "ikclut-studio-project";
    obj["version"] = 1;
    obj["imagePath"] = document.imagePath;
    obj["pipeline"] = pipelineToJson(document.pipeline);

    QJsonArray importedLuts;
    for (const ImportedLutAsset& asset : document.importedLuts) {
        importedLuts.append(assetToJson(asset));
    }
    obj["importedLuts"] = importedLuts;
    QJsonArray masks;
    for (const MaskAsset& mask : document.masks) {
        masks.append(maskToJson(mask));
    }
    obj["masks"] = masks;
    obj["nodeGraph"] = nodeGraphToJson(document.nodeGraph);
    obj["preview"] = QJsonObject{{"mode", "side-by-side"}};
    obj["export"] = QJsonObject{{"format", "ikclut_png"}, {"width", 1024}, {"height", 32}};
    return writeJson(path, obj, error);
}

bool ProjectSerializer::loadProject(const QString& path, ImageDocument* document, QString* error)
{
    QJsonObject obj;
    if (!readJson(path, &obj, error)) {
        return false;
    }
    if (obj.value("schema").toString() != "ikclut-studio-project") {
        if (error) {
            *error = "Unsupported project file.";
        }
        return false;
    }

    ImageDocument loaded;
    loaded.projectPath = path;
    loaded.imagePath = obj.value("imagePath").toString();
    loaded.pipeline = pipelineFromJson(obj.value("pipeline").toObject());

    const QJsonArray importedLuts = obj.value("importedLuts").toArray();
    for (const QJsonValue& value : importedLuts) {
        loaded.importedLuts.append(assetFromJson(value.toObject()));
    }
    const QJsonArray masks = obj.value("masks").toArray();
    for (const QJsonValue& value : masks) {
        loaded.masks.append(maskFromJson(value.toObject()));
    }
    loaded.nodeGraph = nodeGraphFromJson(obj.value("nodeGraph").toObject());

    *document = loaded;
    return true;
}

bool ProjectSerializer::savePreset(const ColorGradePipeline& pipeline, const QString& path, QString* error)
{
    QJsonObject obj;
    obj["schema"] = "ikclut-studio-preset";
    obj["version"] = 1;
    obj["pipeline"] = pipelineToJson(pipeline);
    return writeJson(path, obj, error);
}

bool ProjectSerializer::loadPreset(const QString& path, ColorGradePipeline* pipeline, QString* error)
{
    QJsonObject obj;
    if (!readJson(path, &obj, error)) {
        return false;
    }
    if (obj.value("schema").toString() != "ikclut-studio-preset") {
        if (error) {
            *error = "Unsupported preset file.";
        }
        return false;
    }
    *pipeline = pipelineFromJson(obj.value("pipeline").toObject());
    return true;
}

QJsonObject ProjectSerializer::pipelineToJson(const ColorGradePipeline& pipeline)
{
    QJsonObject obj;
    obj["params"] = paramsToJson(pipeline.params);
    QJsonArray stages;
    for (const ColorGradeStage& stage : pipeline.stages) {
        stages.append(stageToJson(stage));
    }
    obj["stages"] = stages;
    return obj;
}

ColorGradePipeline ProjectSerializer::pipelineFromJson(const QJsonObject& obj)
{
    ColorGradePipeline pipeline;
    pipeline.params = paramsFromJson(obj.value("params").toObject());
    const QJsonArray stages = obj.value("stages").toArray();
    for (const QJsonValue& value : stages) {
        pipeline.stages.append(stageFromJson(value.toObject()));
    }
    if (pipeline.stages.isEmpty()) {
        ColorGradeStage stage;
        stage.stageId = "global-grade";
        stage.label = "Base Grade";
        stage.type = "globalColorGrade";
        stage.enabled = true;
        stage.params = paramsToJson(pipeline.params);
        pipeline.stages.append(stage);
    }
    return pipeline;
}

} // namespace ikclut
