#include "ui/ParameterControl.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace ikclut {

namespace {

class NoWheelSlider : public QSlider {
public:
    explicit NoWheelSlider(QWidget* parent = nullptr)
        : QSlider(Qt::Horizontal, parent)
    {
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        event->ignore();
    }
};

class NoWheelDoubleSpinBox : public QDoubleSpinBox {
public:
    explicit NoWheelDoubleSpinBox(QWidget* parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        event->ignore();
    }
};

} // namespace

ParameterControl::ParameterControl(const QString& label,
                                   double minimum,
                                   double maximum,
                                   double defaultValue,
                                   double step,
                                   int decimals,
                                   QWidget* parent)
    : QWidget(parent)
    , m_minimum(minimum)
    , m_maximum(maximum)
    , m_defaultValue(defaultValue)
    , m_step(step)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(8);

    m_label = new QLabel(label, this);
    m_label->setMinimumWidth(82);
    m_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    m_slider = new NoWheelSlider(this);
    m_slider->setRange(tickFromValue(m_minimum), tickFromValue(m_maximum));
    m_slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_spinBox = new NoWheelDoubleSpinBox(this);
    m_spinBox->setRange(m_minimum, m_maximum);
    m_spinBox->setDecimals(decimals);
    m_spinBox->setSingleStep(m_step);
    m_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spinBox->setKeyboardTracking(false);
    m_spinBox->setMinimumWidth(68);
    m_spinBox->setAlignment(Qt::AlignRight);

    m_resetButton = new QPushButton("0", this);
    m_resetButton->setToolTip(tr("Reset this parameter"));
    m_resetButton->setFixedWidth(28);
    m_resetButton->setProperty("resetButton", true);

    layout->addWidget(m_label);
    layout->addWidget(m_slider, 1);
    layout->addWidget(m_spinBox);
    layout->addWidget(m_resetButton);

    connect(m_slider, &QSlider::valueChanged, this, [this](int tick) {
        if (m_updating) {
            return;
        }
        updateFromValue(valueFromTick(tick), true);
    });
    connect(m_spinBox, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (m_updating) {
            return;
        }
        updateFromValue(value, true);
    });
    connect(m_resetButton, &QPushButton::clicked, this, [this]() {
        updateFromValue(m_defaultValue, true);
    });

    updateFromValue(defaultValue, false);
}

void ParameterControl::setValue(double value)
{
    updateFromValue(value, false);
}

void ParameterControl::setLabelText(const QString& label)
{
    if (m_label) {
        m_label->setText(label);
    }
}

void ParameterControl::setRange(double minimum, double maximum, double step, int decimals, double defaultValue)
{
    m_minimum = minimum;
    m_maximum = maximum;
    m_step = step;
    m_defaultValue = defaultValue;
    m_updating = true;
    {
        const QSignalBlocker sliderBlocker(m_slider);
        const QSignalBlocker spinBlocker(m_spinBox);
        m_slider->setRange(tickFromValue(m_minimum), tickFromValue(m_maximum));
        m_spinBox->setRange(m_minimum, m_maximum);
        m_spinBox->setDecimals(decimals);
        m_spinBox->setSingleStep(m_step);
    }
    m_updating = false;
    updateFromValue(std::clamp(value(), m_minimum, m_maximum), false);
}

double ParameterControl::value() const
{
    return m_spinBox ? m_spinBox->value() : m_defaultValue;
}

double ParameterControl::defaultValue() const
{
    return m_defaultValue;
}

int ParameterControl::tickFromValue(double value) const
{
    return static_cast<int>(std::lround(value / m_step));
}

double ParameterControl::valueFromTick(int tick) const
{
    return tick * m_step;
}

void ParameterControl::updateFromValue(double value, bool emitChange)
{
    const double clamped = std::clamp(value, m_minimum, m_maximum);
    m_updating = true;
    {
        const QSignalBlocker sliderBlocker(m_slider);
        const QSignalBlocker spinBlocker(m_spinBox);
        m_slider->setValue(tickFromValue(clamped));
        m_spinBox->setValue(clamped);
    }
    m_updating = false;

    if (emitChange) {
        emit valueChanged(clamped);
    }
}

} // namespace ikclut
