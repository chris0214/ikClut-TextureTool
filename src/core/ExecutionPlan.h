#pragma once

#include "model/ColorTypes.h"

#include <QString>
#include <QVector>

namespace ikclut {

enum class ExecutionStepKind {
    Stage,
    ImportedLut,
    Mask,
    Output
};

struct ExecutionStep {
    QString stepId;
    ExecutionStepKind kind = ExecutionStepKind::Stage;
    QString sourceId;
    QString type;
    MaskReference maskRef;
    QJsonObject params;
};

struct ExecutionPlan {
    bool fromNodeGraph = false;
    QVector<ExecutionStep> steps;
    QString diagnostic;
};

class ExecutionPlanCompiler {
public:
    static ExecutionPlan compilePipeline(const ColorGradePipeline& pipeline);
    static ExecutionPlan compileNodeGraph(const NodeGraph& graph);
    static ExecutionPlan compileDocument(const ImageDocument& document);
};

} // namespace ikclut

