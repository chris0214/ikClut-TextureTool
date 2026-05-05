#include "core/ExecutionPlan.h"

#include <QHash>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <functional>

namespace ikclut {

ExecutionPlan ExecutionPlanCompiler::compilePipeline(const ColorGradePipeline& pipeline)
{
    ExecutionPlan plan;
    plan.fromNodeGraph = false;
    for (const ColorGradeStage& stage : pipeline.stages) {
        if (!stage.enabled) {
            continue;
        }
        ExecutionStep step;
        step.stepId = stage.stageId;
        step.kind = ExecutionStepKind::Stage;
        step.sourceId = stage.stageId;
        step.type = stage.type;
        step.maskRef = stage.maskRef;
        step.params = stage.params;
        plan.steps.append(step);
    }

    ExecutionStep output;
    output.stepId = "output";
    output.kind = ExecutionStepKind::Output;
    output.sourceId = "linear-pipeline";
    output.type = "output";
    plan.steps.append(output);
    plan.diagnostic = "Compiled linear pipeline.";
    return plan;
}

ExecutionPlan ExecutionPlanCompiler::compileNodeGraph(const NodeGraph& graph)
{
    ExecutionPlan plan;
    plan.fromNodeGraph = true;
    QHash<QString, NodeGraphNode> nodesById;
    for (const NodeGraphNode& node : graph.nodes) {
        nodesById.insert(node.nodeId, node);
    }
    QHash<QString, QStringList> nextByNode;
    for (const NodeGraphEdge& edge : graph.edges) {
        nextByNode[edge.from.nodeId].append(edge.to.nodeId);
    }

    bool hasCycle = false;
    QSet<QString> visiting;
    QSet<QString> visited;
    std::function<void(const QString&)> visit = [&](const QString& nodeId) {
        if (visiting.contains(nodeId)) {
            hasCycle = true;
            return;
        }
        if (visited.contains(nodeId)) {
            return;
        }
        visiting.insert(nodeId);
        visited.insert(nodeId);
        const NodeGraphNode node = nodesById.value(nodeId);
        if (!node.enabled) {
            visiting.remove(nodeId);
            return;
        }
        if (node.type != "input" && node.type != "output") {
            ExecutionStep step;
            step.stepId = node.nodeId;
            step.kind = node.type == "mask" ? ExecutionStepKind::Mask : ExecutionStepKind::Stage;
            step.sourceId = node.nodeId;
            step.type = node.type;
            step.params = node.params;
            plan.steps.append(step);
        }
        QStringList next = nextByNode.value(nodeId);
        std::sort(next.begin(), next.end(), [&](const QString& a, const QString& b) {
            const QPointF pa = nodesById.value(a).position;
            const QPointF pb = nodesById.value(b).position;
            return pa.y() == pb.y() ? pa.x() < pb.x() : pa.y() < pb.y();
        });
        for (const QString& nextId : next) {
            visit(nextId);
        }
        visiting.remove(nodeId);
    };

    visit("node-input");

    ExecutionStep output;
    output.stepId = graph.outputNodeId.isEmpty() ? "output" : graph.outputNodeId;
    output.kind = ExecutionStepKind::Output;
    output.sourceId = graph.outputNodeId;
    output.type = "output";
    plan.steps.append(output);
    const QString outputId = graph.outputNodeId.isEmpty() ? QString("node-output") : graph.outputNodeId;
    QStringList diagnostics;
    diagnostics.append("Compiled node graph.");
    if (hasCycle) {
        diagnostics.append("Cycle detected; cyclic branches were skipped.");
    }
    if (!outputId.isEmpty() && !visited.contains(outputId)) {
        diagnostics.append("Output is not connected to the input flow.");
    }
    plan.diagnostic = diagnostics.join(' ');
    return plan;
}

ExecutionPlan ExecutionPlanCompiler::compileDocument(const ImageDocument& document)
{
    if (document.nodeGraph.enabled && !document.nodeGraph.nodes.isEmpty()) {
        return compileNodeGraph(document.nodeGraph);
    }
    return compilePipeline(document.pipeline);
}

} // namespace ikclut
