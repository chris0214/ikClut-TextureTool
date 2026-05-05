#pragma once

#include <QImage>
#include <QVector>
#include <QWidget>

class QContextMenuEvent;
class QMouseEvent;
class QPaintEvent;

namespace ikclut {

enum class WarperDragHandle {
    None,
    Source,
    Target
};

struct WarperState {
    QVector<QPointF> sources;
    QVector<QPointF> targets;
    QVector<bool> pinned;
    int selectedPoint = -1;
};

class ColorWarperWidget : public QWidget {
    Q_OBJECT

public:
    explicit ColorWarperWidget(QWidget* parent = nullptr);

    void resetWarp();
    QVector<QPointF> points() const;
    QVector<QPointF> sources() const;
    QVector<bool> pinned() const;
    void setPoints(const QVector<QPointF>& points);
    void setWarpPoints(const QVector<QPointF>& sources, const QVector<QPointF>& targets);
    void setWarpPoints(const QVector<QPointF>& sources, const QVector<QPointF>& targets, const QVector<bool>& pinned);
    void setSnapEnabled(bool enabled);
    bool snapEnabled() const;
    void setPinCreationMode(bool enabled);
    bool pinCreationMode() const;
    void addWarpPoint(const QPointF& source, bool pinned = false);
    void deleteSelectedPoint();
    void setSelectedPoint(int index);
    void setSelectedPointPosition(const QPointF& point);
    void setSelectedSourcePosition(const QPointF& point);
    void setSelectedPinned(bool pinned);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    int selectedPoint() const;
    bool selectedPinned() const;

signals:
    void warpChanged(const QVector<QPointF>& points);
    void warpVectorsChanged(const QVector<QPointF>& sources, const QVector<QPointF>& targets, const QVector<bool>& pinned);
    void selectionChanged(int index, const QPointF& point);
    void undoAvailabilityChanged(bool canUndo, bool canRedo);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void rebuildCache(const QSize& size);
    QRectF fieldRect() const;
    QPointF toField(const QPoint& pos) const;
    QPoint toWidget(const QPointF& point) const;
    QPointF toWidgetF(const QPointF& point) const;
    QPointF fitPointToGamut(const QPointF& point) const;
    bool isInGamut(const QPointF& point) const;
    int nearestPoint(const QPoint& pos) const;
    int nearestPoint(const QPoint& pos, WarperDragHandle* handle) const;
    QColor pointColor(const QPointF& point) const;
    bool isPinned(int index) const;
    void pushUndoState();
    void restoreState(const WarperState& state);
    WarperState currentState() const;
    void emitWarpChanged();
    void emitSelectionChanged();

    QVector<QPointF> m_sources;
    QVector<QPointF> m_points;
    QVector<bool> m_pinned;
    QVector<WarperState> m_undoStack;
    QVector<WarperState> m_redoStack;
    int m_selectedPoint = -1;
    int m_dragPoint = -1;
    WarperDragHandle m_dragHandle = WarperDragHandle::None;
    QPoint m_pressPos;
    bool m_snapEnabled = false;
    bool m_pinCreationMode = false;
    bool m_dragUndoSaved = false;
    QImage m_cache;
};

} // namespace ikclut
