#include "ui/CollapsiblePanel.h"

#include <QPushButton>
#include <QVBoxLayout>

namespace ikclut {

CollapsiblePanel::CollapsiblePanel(const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_title(title)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_header = new QPushButton(this);
    m_header->setCheckable(true);
    m_header->setChecked(true);
    m_header->setProperty("collapsibleHeader", true);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_header, &QPushButton::clicked, this, [this](bool checked) {
        setExpanded(checked);
    });

    m_content = new QWidget(this);
    m_content->setProperty("collapsibleContent", true);
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(10, 8, 10, 10);
    m_contentLayout->setSpacing(6);

    root->addWidget(m_header);
    root->addWidget(m_content);
    updateHeader();
}

QVBoxLayout* CollapsiblePanel::contentLayout() const
{
    return m_contentLayout;
}

void CollapsiblePanel::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_header->setChecked(expanded);
    m_content->setVisible(expanded);
    updateHeader();
}

bool CollapsiblePanel::isExpanded() const
{
    return m_expanded;
}

void CollapsiblePanel::updateHeader()
{
    m_header->setText(QString("%1 %2").arg(m_expanded ? "▾" : "▸", m_title));
}

} // namespace ikclut
