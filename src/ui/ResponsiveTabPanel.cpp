#include "ui/ResponsiveTabPanel.h"

#include <QGridLayout>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace ikclut {

ResponsiveTabPanel::ResponsiveTabPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_tabBar = new QWidget(this);
    m_tabBar->setObjectName("responsiveTabBar");
    m_tabLayout = new QGridLayout(m_tabBar);
    m_tabLayout->setContentsMargins(0, 0, 0, 0);
    m_tabLayout->setHorizontalSpacing(0);
    m_tabLayout->setVerticalSpacing(0);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName("responsiveTabStack");

    root->addWidget(m_tabBar);
    root->addWidget(m_stack, 1);
}

void ResponsiveTabPanel::addPage(QWidget* page, const QString& title)
{
    auto* button = new QPushButton(title, m_tabBar);
    button->setCheckable(true);
    button->setFlat(true);
    button->setProperty("tabButton", true);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setMinimumHeight(30);

    const int index = m_buttons.size();
    connect(button, &QPushButton::clicked, this, [this, index]() {
        setCurrentIndex(index);
    });

    m_buttons.append(button);
    m_stack->addWidget(page);
    rebuildTabs();

    if (m_buttons.size() == 1) {
        setCurrentIndex(0);
    } else {
        updateButtonStates();
    }
}

void ResponsiveTabPanel::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_buttons.size()) {
        return;
    }
    m_currentIndex = index;
    m_stack->setCurrentIndex(index);
    updateButtonStates();
}

int ResponsiveTabPanel::currentIndex() const
{
    return m_currentIndex;
}

void ResponsiveTabPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    const int columns = tabColumnsForWidth(event->size().width());
    if (columns != m_columns) {
        rebuildTabs();
    }
}

void ResponsiveTabPanel::rebuildTabs()
{
    if (!m_tabLayout) {
        return;
    }

    while (QLayoutItem* item = m_tabLayout->takeAt(0)) {
        delete item;
    }

    m_columns = tabColumnsForWidth(width());
    for (int i = 0; i < m_buttons.size(); ++i) {
        const int row = i / m_columns;
        const int column = i % m_columns;
        m_tabLayout->addWidget(m_buttons[i], row, column);
    }

    for (int column = 0; column < m_columns; ++column) {
        m_tabLayout->setColumnStretch(column, 1);
    }
}

int ResponsiveTabPanel::tabColumnsForWidth(int width) const
{
    if (m_buttons.isEmpty()) {
        return 1;
    }

    const int targetTabWidth = 84;
    const int columns = qMax(2, width / targetTabWidth);
    return qBound(2, columns, m_buttons.size());
}

void ResponsiveTabPanel::updateButtonStates()
{
    for (int i = 0; i < m_buttons.size(); ++i) {
        m_buttons[i]->setChecked(i == m_currentIndex);
    }
}

} // namespace ikclut
