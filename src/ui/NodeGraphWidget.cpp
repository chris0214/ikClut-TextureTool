#include "ui/NodeGraphWidget.h"

#include <QContextMenuEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>

namespace ikclut {

namespace {

constexpr double kNodeWidth = 158.0;
constexpr double kNodeMinHeight = 92.0;
constexpr double kPortTop = 44.0;
constexpr double kPortStep = 22.0;

QColor nodeColor(const QString& type)
{
    if (type == "input") return QColor(49, 79, 103);
    if (type == "output") return QColor(97, 75, 42);
    if (type == "mixColor") return QColor(90, 73, 42);
    if (type == "colorCorrection" || type == "brightnessContrast" || type == "hueSaturationValue" || type == "gammaNode" || type == "colorBalance" || type == "rgbCurves" || type == "invertColor" || type == "posterizeColor" || type == "clampColor" || type == "levelsColor" || type == "exposureColor" || type == "thresholdColor" || type == "sepiaColor" || type == "channelMixer" || type == "temperatureTint" || type == "grayscaleColor" || type == "vibranceColor" || type == "softClipColor" || type == "duotoneColor" || type == "ascCdl" || type == "colorRamp" || type == "selectiveColor" || type == "lutMix") return QColor(58, 82, 72);
    if (type == "parallelMixer" || type == "layerMixer") return QColor(87, 63, 109);
    if (type == "mask") return QColor(63, 93, 74);
    return QColor(53, 61, 74);
}

QString nodeTypeLabel(const QString& type)
{
    if (type == "input") return "输入图像";
    if (type == "output") return "输出结果";
    if (type == "globalColorGrade") return "调色";
    if (type == "curveGrade") return "曲线";
    if (type == "hslGrade") return "HSL";
    if (type == "proGrade") return "专业调色";
    if (type == "mixColor") return "颜色混合";
    if (type == "brightnessContrast") return "亮度 / 对比度";
    if (type == "hueSaturationValue") return "色相 / 饱和度 / 明度";
    if (type == "gammaNode") return "伽马";
    if (type == "colorBalance") return "色彩平衡";
    if (type == "rgbCurves") return "RGB 曲线";
    if (type == "colorCorrection") return "颜色校正";
    if (type == "invertColor") return "反相";
    if (type == "posterizeColor") return "色阶化";
    if (type == "clampColor") return "限制范围";
    if (type == "levelsColor") return "色阶";
    if (type == "exposureColor") return "曝光";
    if (type == "thresholdColor") return "阈值";
    if (type == "sepiaColor") return "棕褐色";
    if (type == "channelMixer") return "通道混合器";
    if (type == "temperatureTint") return "色温 / 色调";
    if (type == "grayscaleColor") return "灰度";
    if (type == "vibranceColor") return "自然饱和度";
    if (type == "softClipColor") return "柔和裁切";
    if (type == "duotoneColor") return "双色调";
    if (type == "ascCdl") return "ASC CDL";
    if (type == "colorRamp") return "颜色渐变";
    if (type == "selectiveColor") return "选择性颜色";
    if (type == "lutMix") return "LUT 混合";
    if (type == "parallelMixer") return "并行混合器";
    if (type == "layerMixer") return "图层混合器";
    return "节点";
}

QVector<QPair<QString, QString>> nodeMenuEntries()
{
    return {
        {"调色", "globalColorGrade"}, {"曲线调色", "curveGrade"}, {"HSL 调色", "hslGrade"}, {"专业调色", "proGrade"},
        {"颜色混合", "mixColor"}, {"亮度 / 对比度", "brightnessContrast"}, {"色相 / 饱和度 / 明度", "hueSaturationValue"},
        {"伽马", "gammaNode"}, {"色彩平衡", "colorBalance"}, {"RGB 曲线", "rgbCurves"}, {"颜色校正", "colorCorrection"},
        {"曝光", "exposureColor"}, {"色阶", "levelsColor"}, {"限制范围", "clampColor"}, {"反相", "invertColor"},
        {"色阶化", "posterizeColor"}, {"阈值", "thresholdColor"}, {"棕褐色", "sepiaColor"}, {"通道混合器", "channelMixer"},
        {"色温 / 色调", "temperatureTint"}, {"灰度", "grayscaleColor"}, {"自然饱和度", "vibranceColor"},
        {"柔和裁切", "softClipColor"}, {"双色调", "duotoneColor"}, {"ASC CDL", "ascCdl"}, {"颜色渐变", "colorRamp"},
        {"选择性色彩", "selectiveColor"}, {"LUT 混合", "lutMix"}, {"并行混合器", "parallelMixer"}, {"图层混合器", "layerMixer"},
    };
}

} // namespace

NodeGraphWidget::NodeGraphWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(360);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void NodeGraphWidget::setGraph(const NodeGraph& graph)
{
    m_graph = graph;
    sanitizeConnections();
    ensureReadableLayout();
    update();
}

const NodeGraph& NodeGraphWidget::graph() const
{
    return m_graph;
}

QString NodeGraphWidget::selectedNodeId() const
{
    return m_selectedIndex >= 0 && m_selectedIndex < m_graph.nodes.size()
        ? m_graph.nodes.at(m_selectedIndex).nodeId
        : QString();
}

void NodeGraphWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(12, 13, 16));
    drawGrid(painter);
    painter.save();
    painter.translate(m_pan);
    painter.scale(m_zoom, m_zoom);

    for (const NodeGraphEdge& edge : m_graph.edges) {
        const NodeGraphNode* from = findNode(edge.from.nodeId);
        const NodeGraphNode* to = findNode(edge.to.nodeId);
        if (!from || !to) {
            continue;
        }
        const QColor edgeColor = edge.to.nodeId.contains("mixer", Qt::CaseInsensitive)
            ? QColor(211, 151, 255, 190)
            : QColor(104, 190, 255, 185);
        int inputIndex = 0;
        if (edge.to.portId.startsWith("in")) {
            bool ok = false;
            const int parsed = edge.to.portId.mid(2).toInt(&ok);
            inputIndex = ok ? parsed : 0;
        }
        drawEdge(painter, outputPort(*from), inputPort(*to, inputIndex), edgeColor);
    }

    if (m_connecting && m_connectionStart.isValid() && m_connectionStart.nodeIndex < m_graph.nodes.size()) {
        const NodeGraphNode& startNode = m_graph.nodes.at(m_connectionStart.nodeIndex);
        const QPointF cursorScene = viewToScene(m_connectionDragPoint);
        if (m_connectionStart.output) {
            drawEdge(painter, outputPort(startNode), cursorScene, QColor(255, 202, 116, 190));
        } else {
            drawEdge(painter, cursorScene, inputPort(startNode, m_connectionStart.portIndex), QColor(255, 202, 116, 190));
        }
    }

    for (int i = 0; i < m_graph.nodes.size(); ++i) {
        drawNode(painter, m_graph.nodes.at(i), i == m_selectedIndex || m_selectedNodeIds.contains(m_graph.nodes.at(i).nodeId));
    }
    drawMarquee(painter);

    if (m_graph.nodes.isEmpty()) {
        painter.setPen(QColor(146, 152, 162));
        painter.resetTransform();
        painter.drawText(rect(), Qt::AlignCenter, "从调整堆栈构建节点图后即可开始编辑节点");
    }
    painter.restore();
}

void NodeGraphWidget::contextMenuEvent(QContextMenuEvent* event)
{
    const QPointF scenePos = viewToScene(event->pos());
    const int clickedNode = nodeAt(scenePos);
    QString insertAfter;
    if (clickedNode >= 0) {
        m_selectedIndex = clickedNode;
        insertAfter = m_graph.nodes.at(clickedNode).nodeId;
        emit nodeSelected(insertAfter);
    } else {
        insertAfter = selectedNodeId();
    }

    QMenu menu(this);
    QAction* searchNode = menu.addAction("搜索节点...");
    menu.addSeparator();
    QAction* deleteNode = nullptr;
    if (clickedNode >= 0 && canDeleteNode(m_graph.nodes.at(clickedNode))) {
        deleteNode = menu.addAction("删除节点");
        menu.addSeparator();
    }
    QMenu* gradeMenu = menu.addMenu("调整节点");
    QAction* addGrade = gradeMenu->addAction("调色");
    QAction* addCurve = gradeMenu->addAction("曲线调色");
    QAction* addHsl = gradeMenu->addAction("HSL 调色");
    QAction* addPro = gradeMenu->addAction("专业调色");
    QMenu* colorMenu = menu.addMenu("颜色节点");
    QAction* addMixColor = colorMenu->addAction("颜色混合");
    QAction* addBrightness = colorMenu->addAction("亮度 / 对比度");
    QAction* addHueSat = colorMenu->addAction("色相 / 饱和度 / 明度");
    QAction* addGamma = colorMenu->addAction("伽马");
    QAction* addColorBalance = colorMenu->addAction("色彩平衡");
    QAction* addRgbCurves = colorMenu->addAction("RGB 曲线");
    QAction* addColorCorrection = colorMenu->addAction("颜色校正");
    colorMenu->addSeparator();
    QAction* addExposure = colorMenu->addAction("曝光");
    QAction* addLevels = colorMenu->addAction("色阶");
    QAction* addClamp = colorMenu->addAction("限制范围");
    QAction* addInvert = colorMenu->addAction("反相");
    QAction* addPosterize = colorMenu->addAction("色阶化");
    QAction* addThreshold = colorMenu->addAction("阈值");
    QAction* addSepia = colorMenu->addAction("棕褐色");
    QAction* addChannelMixer = colorMenu->addAction("通道混合器");
    QAction* addTemperatureTint = colorMenu->addAction("色温 / 色调");
    QAction* addGrayscale = colorMenu->addAction("灰度");
    QAction* addVibrance = colorMenu->addAction("自然饱和度");
    QAction* addSoftClip = colorMenu->addAction("柔和裁切");
    QAction* addDuotone = colorMenu->addAction("双色调");
    colorMenu->addSeparator();
    QAction* addCdl = colorMenu->addAction("ASC CDL");
    QAction* addColorRamp = colorMenu->addAction("颜色渐变");
    QAction* addSelective = colorMenu->addAction("选择性色彩");
    QAction* addLutMix = colorMenu->addAction("LUT 混合");
    QMenu* mixerMenu = menu.addMenu("混合器节点");
    QAction* addParallelMixer = mixerMenu->addAction("并行混合器");
    QAction* addLayerMixer = mixerMenu->addAction("图层混合器");
    menu.addSeparator();
    QAction* tidy = menu.addAction("整理布局");
    QAction* selected = menu.exec(event->globalPos());
    if (!selected) {
        return;
    }
    if (selected == searchNode) {
        QStringList labels;
        for (const auto& entry : nodeMenuEntries()) {
            labels.append(entry.first);
        }
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, "添加节点", "节点", labels, 0, true, &ok);
        if (!ok || choice.trimmed().isEmpty()) {
            return;
        }
        for (const auto& entry : nodeMenuEntries()) {
            if (entry.first.compare(choice, Qt::CaseInsensitive) == 0 || entry.first.contains(choice, Qt::CaseInsensitive)) {
                emit addNodeRequested(entry.second, scenePos, insertAfter);
                return;
            }
        }
        return;
    }
    if (deleteNode && selected == deleteNode) {
        deleteSelectedNode();
        return;
    }
    if (selected == tidy) {
        ensureReadableLayout();
        emit graphEdited(m_graph);
        update();
        return;
    }
    const QString type = selected == addMixColor ? QString("mixColor")
        : selected == addBrightness ? QString("brightnessContrast")
        : selected == addHueSat ? QString("hueSaturationValue")
        : selected == addGamma ? QString("gammaNode")
        : selected == addColorBalance ? QString("colorBalance")
        : selected == addRgbCurves ? QString("rgbCurves")
        : selected == addColorCorrection ? QString("colorCorrection")
        : selected == addExposure ? QString("exposureColor")
        : selected == addLevels ? QString("levelsColor")
        : selected == addClamp ? QString("clampColor")
        : selected == addInvert ? QString("invertColor")
        : selected == addPosterize ? QString("posterizeColor")
        : selected == addThreshold ? QString("thresholdColor")
        : selected == addSepia ? QString("sepiaColor")
        : selected == addChannelMixer ? QString("channelMixer")
        : selected == addTemperatureTint ? QString("temperatureTint")
        : selected == addGrayscale ? QString("grayscaleColor")
        : selected == addVibrance ? QString("vibranceColor")
        : selected == addSoftClip ? QString("softClipColor")
        : selected == addDuotone ? QString("duotoneColor")
        : selected == addCdl ? QString("ascCdl")
        : selected == addColorRamp ? QString("colorRamp")
        : selected == addSelective ? QString("selectiveColor")
        : selected == addLutMix ? QString("lutMix")
        : selected == addParallelMixer ? QString("parallelMixer")
        : selected == addLayerMixer ? QString("layerMixer")
        : selected == addCurve ? QString("curveGrade")
        : selected == addHsl ? QString("hslGrade")
        : selected == addPro ? QString("proGrade")
        : QString("globalColorGrade");
    emit addNodeRequested(type, scenePos, insertAfter);
}

void NodeGraphWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panDragStart = event->position();
        m_panStart = m_pan;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }
    setFocus();
    const QPointF scenePos = viewToScene(event->position());
    const PortHit hit = portAt(scenePos);
    if (hit.isValid()) {
        m_connecting = true;
        m_connectionStart = hit;
        m_connectionDragPoint = event->position();
        update();
        return;
    }
    m_selectedIndex = nodeAt(scenePos);
    if (m_selectedIndex >= 0) {
        m_selectedNodeIds = {m_graph.nodes.at(m_selectedIndex).nodeId};
        m_dragging = true;
        m_dragOffset = scenePos - m_graph.nodes[m_selectedIndex].position;
        emit nodeSelected(m_graph.nodes[m_selectedIndex].nodeId);
    } else {
        if (event->modifiers() & Qt::ShiftModifier) {
            m_marqueeSelecting = true;
            m_marqueeStart = scenePos;
            m_marqueeEnd = scenePos;
        } else {
            m_selectedNodeIds.clear();
            m_panning = true;
            m_panDragStart = event->position();
            m_panStart = m_pan;
            setCursor(Qt::ClosedHandCursor);
        }
        emit nodeSelected(QString());
    }
    update();
}

void NodeGraphWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        m_pan = m_panStart + (event->position() - m_panDragStart);
        update();
        return;
    }
    if (m_marqueeSelecting) {
        m_marqueeEnd = viewToScene(event->position());
        QRectF rect(m_marqueeStart, m_marqueeEnd);
        rect = rect.normalized();
        m_selectedNodeIds.clear();
        for (const NodeGraphNode& node : m_graph.nodes) {
            if (canDeleteNode(node) && rect.intersects(nodeRect(node))) {
                m_selectedNodeIds.insert(node.nodeId);
            }
        }
        update();
        return;
    }
    if (m_connecting) {
        m_connectionDragPoint = event->position();
        update();
        return;
    }
    if (!m_dragging || m_selectedIndex < 0 || m_selectedIndex >= m_graph.nodes.size()) {
        return;
    }
    QPointF pos = viewToScene(event->position()) - m_dragOffset;
    pos.setX(std::round(pos.x() / 10.0) * 10.0);
    pos.setY(std::round(pos.y() / 10.0) * 10.0);
    m_graph.nodes[m_selectedIndex].position = pos;
    update();
}

void NodeGraphWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_connecting) {
        const PortHit hit = portAt(viewToScene(event->position()));
        if (hit.isValid() && m_connectionStart.isValid() && hit.output != m_connectionStart.output
            && m_connectionStart.nodeIndex < m_graph.nodes.size() && hit.nodeIndex < m_graph.nodes.size()) {
            const PortHit outputHit = m_connectionStart.output ? m_connectionStart : hit;
            const PortHit inputHit = m_connectionStart.output ? hit : m_connectionStart;
            const QString fromNodeId = m_graph.nodes.at(outputHit.nodeIndex).nodeId;
            const QString toNodeId = m_graph.nodes.at(inputHit.nodeIndex).nodeId;
            const QString toPortId = inputPortId(inputHit.portIndex);
            if (toNodeId != fromNodeId) {
                m_graph.edges.erase(std::remove_if(m_graph.edges.begin(), m_graph.edges.end(), [&](const NodeGraphEdge& edge) {
                                        return edge.to.nodeId == toNodeId && edge.to.portId == toPortId;
                                    }),
                                    m_graph.edges.end());
                const QString edgeId = QString("edge-%1-%2-%3").arg(fromNodeId, toNodeId).arg(inputHit.portIndex);
                m_graph.edges.append(NodeGraphEdge{edgeId,
                                                   NodeGraphPortRef{fromNodeId, "out"},
                                                   NodeGraphPortRef{toNodeId, toPortId}});
                sanitizeConnections();
                emit graphEdited(m_graph);
            }
        }
        m_connecting = false;
        m_connectionStart = PortHit();
        update();
        return;
    }
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        emit graphEdited(m_graph);
    }
    if ((event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) && m_panning) {
        m_panning = false;
        unsetCursor();
    }
    if (event->button() == Qt::LeftButton && m_marqueeSelecting) {
        m_marqueeSelecting = false;
        QString selectedId;
        for (const NodeGraphNode& node : m_graph.nodes) {
            if (m_selectedNodeIds.contains(node.nodeId)) {
                selectedId = node.nodeId;
                break;
            }
        }
        emit nodeSelected(selectedId);
        update();
    }
}

void NodeGraphWidget::mouseDoubleClickEvent(QMouseEvent*)
{
    ensureReadableLayout();
    emit graphEdited(m_graph);
    update();
}

void NodeGraphWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Copy)) {
        if (m_selectedIndex >= 0 && m_selectedIndex < m_graph.nodes.size() && canDeleteNode(m_graph.nodes.at(m_selectedIndex))) {
            m_copiedNode = m_graph.nodes.at(m_selectedIndex);
            m_hasCopiedNode = true;
        }
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        if (m_hasCopiedNode) {
            emit cloneNodeRequested(m_copiedNode, m_copiedNode.position + QPointF(30.0, 30.0));
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelectedNode();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void NodeGraphWidget::wheelEvent(QWheelEvent* event)
{
    const QPointF cursor = event->position();
    const QPointF before = viewToScene(cursor);
    const double factor = event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12;
    m_zoom = std::clamp(m_zoom * factor, 0.15, 4.0);
    m_pan = cursor - before * m_zoom;
    update();
    event->accept();
}

QRectF NodeGraphWidget::nodeRect(const NodeGraphNode& node) const
{
    const double height = std::max(kNodeMinHeight, kPortTop + inputPortCount(node) * kPortStep + 16.0);
    return QRectF(node.position, QSizeF(kNodeWidth, height));
}

QPointF NodeGraphWidget::inputPort(const NodeGraphNode& node, int index) const
{
    const QRectF r = nodeRect(node);
    const double y = r.top() + kPortTop + std::clamp(index, 0, 8) * kPortStep;
    return QPointF(r.left(), y);
}

QPointF NodeGraphWidget::outputPort(const NodeGraphNode& node) const
{
    const QRectF r = nodeRect(node);
    return QPointF(r.right(), r.center().y());
}

QString NodeGraphWidget::inputPortId(int index) const
{
    return QString("in%1").arg(std::max(0, index));
}

int NodeGraphWidget::nodeAt(const QPointF& point) const
{
    for (int i = m_graph.nodes.size() - 1; i >= 0; --i) {
        if (nodeRect(m_graph.nodes.at(i)).contains(point)) {
            return i;
        }
    }
    return -1;
}

NodeGraphWidget::PortHit NodeGraphWidget::portAt(const QPointF& point) const
{
    for (int i = m_graph.nodes.size() - 1; i >= 0; --i) {
        const NodeGraphNode& node = m_graph.nodes.at(i);
        if (outputPortCount(node) > 0 && QLineF(point, outputPort(node)).length() <= 8.0) {
            return PortHit{i, true, 0};
        }
        for (int port = 0; port < inputPortCount(node); ++port) {
            if (QLineF(point, inputPort(node, port)).length() <= 8.0) {
                return PortHit{i, false, port};
            }
        }
    }
    return PortHit();
}

const NodeGraphNode* NodeGraphWidget::findNode(const QString& nodeId) const
{
    const auto it = std::find_if(m_graph.nodes.begin(), m_graph.nodes.end(), [&](const NodeGraphNode& node) {
        return node.nodeId == nodeId;
    });
    return it == m_graph.nodes.end() ? nullptr : &(*it);
}

int NodeGraphWidget::inputPortCount(const NodeGraphNode& node) const
{
    if (node.type == "input") {
        return 0;
    }
    if (node.type == "mixColor") {
        return 2;
    }
    return node.type == "parallelMixer" || node.type == "layerMixer" ? 3 : 1;
}

int NodeGraphWidget::outputPortCount(const NodeGraphNode& node) const
{
    return node.type == "output" ? 0 : 1;
}

QString NodeGraphWidget::portLabel(const NodeGraphNode& node, bool outputPort, int index) const
{
    if (node.type == "input") {
        return outputPort ? "image" : QString();
    }
    if (node.type == "output") {
        return outputPort ? QString() : "result";
    }
    if (node.type == "parallelMixer" || node.type == "layerMixer") {
        return outputPort ? "mix" : QString("in %1").arg(index + 1);
    }
    if (node.type == "mixColor") {
        return outputPort ? "result" : (index == 0 ? "A" : "B");
    }
    return outputPort ? "out" : "in";
}

void NodeGraphWidget::drawGrid(QPainter& painter)
{
    const QPointF topLeft = viewToScene(QPointF(0, 0));
    const QPointF bottomRight = viewToScene(QPointF(width(), height()));
    painter.setPen(QColor(29, 32, 38));
    const int minX = static_cast<int>(std::floor(topLeft.x() / 24.0) * 24.0);
    const int maxX = static_cast<int>(std::ceil(bottomRight.x() / 24.0) * 24.0);
    const int minY = static_cast<int>(std::floor(topLeft.y() / 24.0) * 24.0);
    const int maxY = static_cast<int>(std::ceil(bottomRight.y() / 24.0) * 24.0);
    painter.save();
    painter.translate(m_pan);
    painter.scale(m_zoom, m_zoom);
    for (int x = minX; x <= maxX; x += 24) {
        painter.drawLine(x, minY, x, maxY);
    }
    for (int y = minY; y <= maxY; y += 24) {
        painter.drawLine(minX, y, maxX, y);
    }
    painter.setPen(QColor(38, 42, 50));
    for (int x = static_cast<int>(std::floor(topLeft.x() / 120.0) * 120.0); x <= maxX; x += 120) {
        painter.drawLine(x, minY, x, maxY);
    }
    for (int y = static_cast<int>(std::floor(topLeft.y() / 120.0) * 120.0); y <= maxY; y += 120) {
        painter.drawLine(minX, y, maxX, y);
    }
    painter.restore();
}

void NodeGraphWidget::drawEdge(QPainter& painter, const QPointF& from, const QPointF& to, const QColor& color)
{
    QPainterPath path(from);
    const double tension = std::max(70.0, std::abs(to.x() - from.x()) * 0.45);
    path.cubicTo(from + QPointF(tension, 0.0), to - QPointF(tension, 0.0), to);
    painter.setPen(QPen(QColor(0, 0, 0, 135), 4.8));
    painter.drawPath(path);
    painter.setPen(QPen(color, 2.2));
    painter.drawPath(path);
}

void NodeGraphWidget::drawNode(QPainter& painter, const NodeGraphNode& node, bool selected)
{
    const QRectF r = nodeRect(node);
    const QColor base = nodeColor(node.type);
    painter.setPen(QPen(selected ? QColor(255, 195, 82) : QColor(82, 89, 102), selected ? 2.0 : 1.0));
    painter.setBrush(base);
    painter.drawRoundedRect(r, 6, 6);

    QRectF header = r.adjusted(0, 0, 0, -r.height() + 30);
    painter.setPen(Qt::NoPen);
    painter.setBrush(base.lighter(124));
    painter.drawRoundedRect(header, 8, 8);
    painter.drawRect(QRectF(header.left(), header.bottom() - 8, header.width(), 8));

    painter.setPen(QColor(237, 240, 246));
    painter.setFont(QFont("Microsoft YaHei UI", 8, QFont::DemiBold));
    painter.drawText(header.adjusted(10, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, node.label.isEmpty() ? node.nodeId : node.label);
    painter.setFont(QFont("Microsoft YaHei UI", 7));
    painter.setPen(QColor(191, 199, 210));
    const QString mode = node.params.value("graphMode").toString();
    const QString detail = mode.isEmpty() ? nodeTypeLabel(node.type) : QString("%1 | %2").arg(nodeTypeLabel(node.type), mode);
    painter.drawText(r.adjusted(10, 32, -8, -8), Qt::AlignTop | Qt::AlignLeft, detail);

    painter.setFont(QFont("Microsoft YaHei UI", 7));
    const int inputCount = inputPortCount(node);
    for (int i = 0; i < inputCount; ++i) {
        const QPointF p = inputPort(node, i);
        painter.setBrush(QColor(28, 34, 42));
        painter.setPen(QPen(QColor(116, 190, 255), 1.6));
        painter.drawEllipse(p, 5.6, 5.6);
        painter.setPen(QColor(151, 204, 235));
        painter.drawText(QRectF(p.x() + 10.0, p.y() - 8.0, 68.0, 16.0), Qt::AlignLeft | Qt::AlignVCenter, portLabel(node, false, i));
    }
    if (outputPortCount(node) > 0) {
        painter.setBrush(QColor(34, 28, 20));
        painter.setPen(QPen(QColor(255, 190, 96), 1.6));
        painter.drawEllipse(outputPort(node), 5.6, 5.6);
        painter.setPen(QColor(250, 204, 128));
        painter.drawText(QRectF(outputPort(node).x() - 48.0, outputPort(node).y() - 8.0, 40.0, 16.0),
                         Qt::AlignRight | Qt::AlignVCenter,
                         portLabel(node, true));
    }

    if (!node.enabled) {
        painter.fillRect(r, QColor(8, 8, 8, 110));
        painter.setPen(QColor(230, 130, 120));
        painter.drawText(r, Qt::AlignCenter, "DISABLED");
    }
}

void NodeGraphWidget::drawMarquee(QPainter& painter) const
{
    if (!m_marqueeSelecting) {
        return;
    }
    const QRectF rect(m_marqueeStart, m_marqueeEnd);
    painter.setPen(QPen(QColor(255, 195, 82, 180), 1.0, Qt::DashLine));
    painter.setBrush(QColor(255, 195, 82, 35));
    painter.drawRect(rect.normalized());
}

void NodeGraphWidget::ensureReadableLayout()
{
    if (m_graph.nodes.isEmpty()) {
        return;
    }
    for (int i = 0; i < m_graph.nodes.size(); ++i) {
        QPointF& p = m_graph.nodes[i].position;
        if (p.x() < 20.0 || p.y() < 20.0) {
            p.setX(std::max(30.0, p.x() + 30.0));
            p.setY(std::max(50.0, p.y() + 50.0));
        }
    }
}

void NodeGraphWidget::sanitizeConnections()
{
    QSet<QString> nodeIds;
    QHash<QString, QString> nodeTypes;
    for (const NodeGraphNode& node : m_graph.nodes) {
        nodeIds.insert(node.nodeId);
        nodeTypes.insert(node.nodeId, node.type);
    }

    QSet<QString> seenInputPorts;
    QVector<NodeGraphEdge> cleanedReversed;
    for (auto it = m_graph.edges.crbegin(); it != m_graph.edges.crend(); ++it) {
        NodeGraphEdge edge = *it;
        if (!nodeIds.contains(edge.from.nodeId) || !nodeIds.contains(edge.to.nodeId)) {
            continue;
        }
        if (nodeTypes.value(edge.to.nodeId) == "input" || nodeTypes.value(edge.from.nodeId) == "output") {
            continue;
        }
        if (edge.to.portId == "in") {
            edge.to.portId = "in0";
        }
        const QString inputKey = edge.to.nodeId + "\n" + edge.to.portId;
        if (seenInputPorts.contains(inputKey)) {
            continue;
        }
        seenInputPorts.insert(inputKey);
        cleanedReversed.append(edge);
    }
    m_graph.edges.clear();
    for (auto it = cleanedReversed.crbegin(); it != cleanedReversed.crend(); ++it) {
        m_graph.edges.append(*it);
    }
}

QPointF NodeGraphWidget::viewToScene(const QPointF& point) const
{
    return (point - m_pan) / m_zoom;
}

QPointF NodeGraphWidget::sceneToView(const QPointF& point) const
{
    return point * m_zoom + m_pan;
}

bool NodeGraphWidget::canDeleteNode(const NodeGraphNode& node) const
{
    return node.type != "input" && node.type != "output";
}

void NodeGraphWidget::deleteSelectedNode()
{
    QSet<QString> ids = m_selectedNodeIds;
    if (ids.isEmpty() && m_selectedIndex >= 0 && m_selectedIndex < m_graph.nodes.size()) {
        ids.insert(m_graph.nodes.at(m_selectedIndex).nodeId);
    }
    if (ids.isEmpty()) {
        return;
    }
    for (const NodeGraphNode& node : m_graph.nodes) {
        if (ids.contains(node.nodeId) && !canDeleteNode(node)) {
            ids.remove(node.nodeId);
        }
    }
    if (ids.isEmpty()) {
        return;
    }

    m_graph.edges.erase(std::remove_if(m_graph.edges.begin(), m_graph.edges.end(), [&](const NodeGraphEdge& edge) {
                            return ids.contains(edge.from.nodeId) || ids.contains(edge.to.nodeId);
                        }),
                        m_graph.edges.end());
    m_graph.nodes.erase(std::remove_if(m_graph.nodes.begin(), m_graph.nodes.end(), [&](const NodeGraphNode& node) {
                            return ids.contains(node.nodeId);
                        }),
                        m_graph.nodes.end());

    m_selectedIndex = -1;
    m_selectedNodeIds.clear();
    emit graphEdited(m_graph);
    emit nodeSelected(QString());
    update();
}

} // namespace ikclut
