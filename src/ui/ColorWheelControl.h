#pragma once

#include <QImage>
#include <QVector3D>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

namespace ikclut {

class ColorWheelControl : public QWidget {
    Q_OBJECT

public:
    explicit ColorWheelControl(const QString& title,
                               float baseValue,
                               float minimumValue,
                               float maximumValue,
                               float maximumDelta,
                               QWidget* parent = nullptr);

    void setValue(const QVector3D& value);
    QVector3D value() const;

signals:
    void valueChanged(const QVector3D& value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updateFromPoint(const QPointF& point, bool emitChange);
    void handleFromVector(const QVector3D& value);
    QRectF wheelRect() const;
    void rebuildCache(const QSize& size);

    QString m_title;
    float m_baseValue = 0.0f;
    float m_minimumValue = -1.0f;
    float m_maximumValue = 1.0f;
    float m_maximumDelta = 0.1f;
    QPointF m_handle = QPointF(0.0, 0.0);
    QVector3D m_value;
    QImage m_cache;
};

} // namespace ikclut
