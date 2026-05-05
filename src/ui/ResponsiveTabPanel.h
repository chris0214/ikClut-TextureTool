#pragma once

#include <QWidget>

class QGridLayout;
class QPushButton;
class QStackedWidget;

namespace ikclut {

class ResponsiveTabPanel : public QWidget {
    Q_OBJECT

public:
    explicit ResponsiveTabPanel(QWidget* parent = nullptr);

    void addPage(QWidget* page, const QString& title);
    void setCurrentIndex(int index);
    int currentIndex() const;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildTabs();
    int tabColumnsForWidth(int width) const;
    void updateButtonStates();

    QWidget* m_tabBar = nullptr;
    QGridLayout* m_tabLayout = nullptr;
    QStackedWidget* m_stack = nullptr;
    QVector<QPushButton*> m_buttons;
    int m_currentIndex = 0;
    int m_columns = 0;
};

} // namespace ikclut
