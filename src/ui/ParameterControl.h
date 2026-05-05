#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QSlider;
class QDoubleSpinBox;

namespace ikclut {

class ParameterControl : public QWidget {
    Q_OBJECT

public:
    explicit ParameterControl(const QString& label,
                              double minimum,
                              double maximum,
                              double defaultValue,
                              double step,
                              int decimals,
                              QWidget* parent = nullptr);

    void setValue(double value);
    void setLabelText(const QString& label);
    void setRange(double minimum, double maximum, double step, int decimals, double defaultValue);
    double value() const;
    double defaultValue() const;

signals:
    void valueChanged(double value);

private:
    int tickFromValue(double value) const;
    double valueFromTick(int tick) const;
    void updateFromValue(double value, bool emitChange);

    QLabel* m_label = nullptr;
    QSlider* m_slider = nullptr;
    QDoubleSpinBox* m_spinBox = nullptr;
    QPushButton* m_resetButton = nullptr;
    double m_minimum = 0.0;
    double m_maximum = 1.0;
    double m_defaultValue = 0.0;
    double m_step = 0.01;
    bool m_updating = false;
};

} // namespace ikclut
