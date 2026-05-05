#pragma once

#include <QWidget>

class QPushButton;
class QVBoxLayout;

namespace ikclut {

class CollapsiblePanel : public QWidget {
    Q_OBJECT

public:
    explicit CollapsiblePanel(const QString& title, QWidget* parent = nullptr);

    QVBoxLayout* contentLayout() const;
    void setExpanded(bool expanded);
    bool isExpanded() const;

private:
    void updateHeader();

    QPushButton* m_header = nullptr;
    QWidget* m_content = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    QString m_title;
    bool m_expanded = true;
};

} // namespace ikclut
