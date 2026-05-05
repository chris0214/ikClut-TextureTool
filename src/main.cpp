#include "MainWindow.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QPalette>
#include <QStyleFactory>
#include <QTranslator>

namespace {

void writeStartupLog()
{
    const QString path = QDir::current().filePath("IkClutStudio-startup.log");
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString(Qt::ISODate) << " startup\n";
    }
}

} // namespace

namespace {

bool loadAppTranslations(QApplication& app, QTranslator& translator)
{
    const QString localeName = QLocale::system().name();
    const QString baseName = QStringLiteral("IkClutStudio_") + localeName;

    const QStringList resourceCandidates = {
        QStringLiteral(":/i18n/") + baseName + QStringLiteral(".qm"),
        QStringLiteral(":/i18n/IkClutStudio_") + QLocale(localeName).bcp47Name().replace('-', '_') + QStringLiteral(".qm"),
        QStringLiteral(":/i18n/IkClutStudio_zh_CN.qm")
    };

    for (const QString& path : resourceCandidates) {
        if (translator.load(path)) {
            app.installTranslator(&translator);
            return true;
        }
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList diskCandidates = {
        QDir(appDir).filePath(QStringLiteral("translations/") + baseName + QStringLiteral(".qm")),
        QDir(appDir).filePath(QStringLiteral("translations/IkClutStudio_zh_CN.qm"))
    };

    for (const QString& path : diskCandidates) {
        if (translator.load(path)) {
            app.installTranslator(&translator);
            return true;
        }
    }

    return false;
}

void applyTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(24, 25, 28));
    palette.setColor(QPalette::WindowText, QColor(224, 226, 232));
    palette.setColor(QPalette::Base, QColor(16, 17, 20));
    palette.setColor(QPalette::AlternateBase, QColor(32, 34, 38));
    palette.setColor(QPalette::ToolTipBase, QColor(235, 236, 240));
    palette.setColor(QPalette::ToolTipText, QColor(20, 22, 25));
    palette.setColor(QPalette::Text, QColor(224, 226, 232));
    palette.setColor(QPalette::Button, QColor(38, 40, 45));
    palette.setColor(QPalette::ButtonText, QColor(224, 226, 232));
    palette.setColor(QPalette::BrightText, QColor(255, 80, 80));
    palette.setColor(QPalette::Highlight, QColor(208, 142, 54));
    palette.setColor(QPalette::HighlightedText, QColor(12, 12, 12));
    app.setPalette(palette);

    app.setStyleSheet(R"(
        QMainWindow, QWidget { font-family: "Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI"; font-size: 9pt; }
        QMenuBar, QMenu, QToolBar, QStatusBar { background: #1d1f23; color: #e4e6ec; }
        QToolBar { border-bottom: 1px solid #30343a; spacing: 6px; padding: 4px; }
        QToolButton, QPushButton { background: #2b2e34; border: 1px solid #41464e; padding: 6px 10px; border-radius: 4px; }
        QToolButton:hover, QPushButton:hover { background: #363a42; }
        QPushButton[curveChannelButton="true"],
        QPushButton[curveToolButton="true"],
        QPushButton[warperToolButton="true"] {
            background: #25282e;
            border: 1px solid #404651;
            padding: 6px 10px;
            min-height: 18px;
        }
        QPushButton[curveChannelButton="true"] {
            padding-left: 12px;
            padding-right: 12px;
        }
        QPushButton[curveChannelButton="true"]:checked,
        QPushButton[curveToolButton="true"]:checked,
        QPushButton[warperToolButton="true"]:checked {
            background: #343945;
            border-color: #d08e36;
        }
        QPushButton[curveChannelButton="true"]:hover,
        QPushButton[curveToolButton="true"]:hover,
        QPushButton[warperToolButton="true"]:hover {
            background: #31353d;
        }
        QDoubleSpinBox {
            background: #111318;
            color: #e7e9ee;
            border: 1px solid #3c424b;
            border-radius: 4px;
            padding: 4px 6px;
            selection-background-color: #d08e36;
            selection-color: #101010;
        }
        QDoubleSpinBox:focus { border-color: #d08e36; }
        QPushButton[resetButton="true"] {
            background: #202329;
            color: #aeb5c0;
            border: 1px solid #3a3f47;
            padding: 4px 0;
            border-radius: 4px;
            font-weight: 600;
        }
        QPushButton[resetButton="true"]:hover { color: #ffffff; border-color: #d08e36; }
        QPushButton[collapsibleHeader="true"] {
            background: #202227;
            color: #e1e4ea;
            border: 1px solid #343840;
            border-radius: 3px;
            padding: 7px 9px;
            text-align: left;
            font-weight: 600;
        }
        QPushButton[collapsibleHeader="true"]:hover { background: #292d34; border-color: #454b55; }
        QWidget[collapsibleContent="true"] {
            background: #191b20;
            border-left: 1px solid #343840;
            border-right: 1px solid #343840;
            border-bottom: 1px solid #343840;
        }
        QWidget#responsiveTabBar { background: #1d1f23; border: 1px solid #343840; border-bottom: 0; }
        QStackedWidget#responsiveTabStack { border: 1px solid #343840; background: #18191c; }
        QWidget#warperSidePanel {
            background: #202227;
            border: 1px solid #343840;
            border-radius: 4px;
        }
        QPushButton[tabButton="true"] {
            background: #24272d;
            color: #d9dde5;
            border: 1px solid #343840;
            border-left: 0;
            border-top: 0;
            padding: 6px 8px;
            border-radius: 0;
            text-align: center;
        }
        QPushButton[tabButton="true"]:checked { background: #343840; color: #ffffff; border-bottom-color: #d08e36; }
        QPushButton[tabButton="true"]:hover { background: #30343b; }
        QScrollArea { border: 0; background: transparent; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QGroupBox { border: 1px solid #383d45; margin-top: 12px; padding: 8px; border-radius: 4px; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QSlider::groove:horizontal { height: 4px; background: #454b55; border-radius: 2px; }
        QSlider::handle:horizontal { width: 14px; margin: -6px 0; border-radius: 7px; background: #d08e36; }
    )");
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    writeStartupLog();
    QTranslator appTranslator;
    loadAppTranslations(app, appTranslator);
    applyTheme(app);

    ikclut::MainWindow window;
    window.show();
    return app.exec();
}
