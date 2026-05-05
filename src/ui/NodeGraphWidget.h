#pragma once

#include "model/ColorTypes.h"

#include <QSet>
#include <QWidget>

namespace ikclut {

class NodeGraphWidget : public QWidget {
    Q_OBJECT

public:
    explicit NodeGraphWidget(QWidget* parent = nullptr);

    void setGraph(const NodeGraph& graph);
    const NodeGraph& graph() const;
    QString selectedNodeId() const;

signals:
    void graphEdited(const NodeGraph& graph);
    void nodeSelected(const QString& nodeId);
    void addNodeRequested(const QString& type, const QPointF& position, const QString& insertAfterNodeId);
    void cloneNodeRequested(const NodeGraphNode& node, const QPointF& position);

protected:
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct PortHit {
        int nodeIndex = -1;
        bool output = false;
        int portIndex = 0;

        bool isValid() const { return nodeIndex >= 0; }
    };

    QRectF nodeRect(const NodeGraphNode& node) const;
    QPointF inputPort(const NodeGraphNode& node, int index = 0) const;
    QPointF outputPort(const NodeGraphNode& node) const;
    QString inputPortId(int index) const;
    int nodeAt(const QPointF& point) const;
    PortHit portAt(const QPointF& point) const;
    const NodeGraphNode* findNode(const QString& nodeId) const;
    QString portLabel(const NodeGraphNode& node, bool outputPort, int index = 0) const;
    int inputPortCount(const NodeGraphNode& node) const;
    int outputPortCount(const NodeGraphNode& node) const;
    void drawGrid(QPainter& painter);
    void drawEdge(QPainter& painter, const QPointF& from, const QPointF& to, const QColor& color);
    void drawNode(QPainter& painter, const NodeGraphNode& node, bool selected);
    void ensureReadableLayout();
    void sanitizeConnections();
    QPointF viewToScene(const QPointF& point) const;
    QPointF sceneToView(const QPointF& point) const;
    bool canDeleteNode(const NodeGraphNode& node) const;
    void deleteSelectedNode();
    void drawMarquee(QPainter& painter) const;

    NodeGraph m_graph;
    int m_selectedIndex = -1;
    QSet<QString> m_selectedNodeIds;
    NodeGraphNode m_copiedNode;
    bool m_hasCopiedNode = false;
    double m_zoom = 1.0;
    QPointF m_pan;
    QPointF m_panDragStart;
    QPointF m_panStart;
    QPointF m_marqueeStart;
    QPointF m_marqueeEnd;
    QPointF m_dragOffset;
    PortHit m_connectionStart;
    QPointF m_connectionDragPoint;
    bool m_dragging = false;
    bool m_connecting = false;
    bool m_panning = false;
    bool m_marqueeSelecting = false;
};

} // namespace ikclut
