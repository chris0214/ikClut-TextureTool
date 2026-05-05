#include "MainWindow.h"

#include "core/ColorPipeline.h"
#include "core/ExecutionPlan.h"
#include "core/IkClutExporter.h"
#include "core/LutImportService.h"
#include "core/LutQualityAnalyzer.h"
#include "core/MaskEvaluator.h"
#include "core/ProjectSerializer.h"
#include "render/DirectX11Backend.h"
#include "render/PreviewRenderer.h"
#include "ui/ColorWheelControl.h"
#include "ui/ColorWarperWidget.h"
#include "ui/CollapsiblePanel.h"
#include "ui/CurveWidget.h"
#include "ui/ImageView.h"
#include "ui/Lut3DViewer.h"
#include "ui/NodeGraphWidget.h"
#include "ui/ParameterControl.h"
#include "ui/ResponsiveTabPanel.h"
#include "ui/ScopeWidget.h"

#include <QtConcurrent>
#include <QtMath>

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QAbstractButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineF>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QSet>
#include <QTableWidget>
#include <QTextStream>
#include <QToolBar>
#include <QUuid>
#include <QWheelEvent>

#include <algorithm>
#include <functional>

namespace ikclut {

namespace {
const char* kHslBands[] = {"红色", "橙色", "黄色", "绿色", "青色", "蓝色", "紫色", "品红"};
const char* kLocalMaskId = "local-mask";

class NoWheelComboBox : public QComboBox {
public:
    explicit NoWheelComboBox(QWidget* parent = nullptr)
        : QComboBox(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        event->ignore();
    }
};

bool isParamGradeStageType(const QString& type)
{
    const QString key = type.trimmed();
    return key.isEmpty()
        || key == "globalColorGrade"
        || key == "curveGrade"
        || key == "hslGrade"
        || key == "proGrade";
}

QString nodeStageLabelForType(const QString& type)
{
    if (type == "curveGrade") return "曲线调色";
    if (type == "hslGrade") return "HSL 调色";
    if (type == "proGrade") return "专业调色";
    if (type == "mixColor") return "颜色混合";
    if (type == "brightnessContrast") return "亮度 / 对比度";
    if (type == "hueSaturationValue") return "色相 / 饱和度 / 明度";
    if (type == "gammaNode") return "伽马";
    if (type == "colorBalance") return "色彩平衡";
    if (type == "rgbCurves") return "RGB 曲线";
    if (type == "colorCorrection") return "颜色校正";
    if (type == "invertColor") return "反相";
    if (type == "posterizeColor") return "色阶化";
    if (type == "clampColor") return "限制范围";
    if (type == "levelsColor") return "色阶";
    if (type == "exposureColor") return "曝光";
    if (type == "thresholdColor") return "阈值";
    if (type == "sepiaColor") return "棕褐色";
    if (type == "channelMixer") return "通道混合器";
    if (type == "temperatureTint") return "色温 / 色调";
    if (type == "grayscaleColor") return "灰度";
    if (type == "vibranceColor") return "自然饱和度";
    if (type == "softClipColor") return "柔和裁切";
    if (type == "duotoneColor") return "双色调";
    if (type == "ascCdl") return "ASC CDL";
    if (type == "colorRamp") return "颜色渐变";
    if (type == "selectiveColor") return "选择性色彩";
    if (type == "lutMix") return "LUT 混合";
    if (type == "parallelMixer") return "并行混合器";
    if (type == "layerMixer") return "图层混合器";
    return "调色";
}

QJsonObject defaultNodeParamsForType(const QString& type, const ColorGradeParams& params)
{
    if (type == "mixColor") {
        return QJsonObject{{"blendType", "mix"},
                           {"factor", 1.0},
                           {"colorA", vectorToJson(QVector3D(0.0f, 0.0f, 0.0f))},
                           {"colorB", vectorToJson(QVector3D(1.0f, 1.0f, 1.0f))},
                           {"useFlowAsA", true},
                           {"clampResult", true}};
    }
    if (type == "brightnessContrast") {
        return QJsonObject{{"brightness", 0.0}, {"contrast", 0.0}};
    }
    if (type == "hueSaturationValue") {
        return QJsonObject{{"hue", 0.5}, {"saturation", 1.0}, {"value", 1.0}};
    }
    if (type == "gammaNode") {
        return QJsonObject{{"gamma", 1.0}};
    }
    if (type == "colorBalance") {
        return QJsonObject{{"lift", vectorToJson(QVector3D(0.0f, 0.0f, 0.0f))},
                           {"gamma", vectorToJson(QVector3D(1.0f, 1.0f, 1.0f))},
                           {"gain", vectorToJson(QVector3D(1.0f, 1.0f, 1.0f))}};
    }
    if (type == "rgbCurves") {
        ColorGradeParams curveParams;
        curveParams.masterCurve = params.masterCurve;
        curveParams.redCurve = params.redCurve;
        curveParams.greenCurve = params.greenCurve;
        curveParams.blueCurve = params.blueCurve;
        curveParams.masterCurveHandles = params.masterCurveHandles;
        curveParams.redCurveHandles = params.redCurveHandles;
        curveParams.greenCurveHandles = params.greenCurveHandles;
        curveParams.blueCurveHandles = params.blueCurveHandles;
        QJsonObject obj;
        obj["masterCurve"] = curveToJson(curveParams.masterCurve);
        obj["redCurve"] = curveToJson(curveParams.redCurve);
        obj["greenCurve"] = curveToJson(curveParams.greenCurve);
        obj["blueCurve"] = curveToJson(curveParams.blueCurve);
        obj["masterCurveHandles"] = curveHandlesToJson(curveParams.masterCurveHandles);
        obj["redCurveHandles"] = curveHandlesToJson(curveParams.redCurveHandles);
        obj["greenCurveHandles"] = curveHandlesToJson(curveParams.greenCurveHandles);
        obj["blueCurveHandles"] = curveHandlesToJson(curveParams.blueCurveHandles);
        obj["masterMid"] = 0.5;
        obj["redMid"] = 0.5;
        obj["greenMid"] = 0.5;
        obj["blueMid"] = 0.5;
        return obj;
    }
    if (type == "colorCorrection") {
        return QJsonObject{{"masterHue", 0.0},
                           {"masterSaturation", 1.0},
                           {"masterValue", 1.0},
                           {"masterContrast", 0.0},
                           {"masterGamma", 1.0}};
    }
    if (type == "invertColor") {
        return QJsonObject{{"factor", 1.0}};
    }
    if (type == "posterizeColor") {
        return QJsonObject{{"steps", 8.0}};
    }
    if (type == "clampColor") {
        return QJsonObject{{"minimum", 0.0}, {"maximum", 1.0}};
    }
    if (type == "levelsColor") {
        return QJsonObject{{"blackPoint", 0.0}, {"whitePoint", 1.0}, {"gamma", 1.0}, {"outputBlack", 0.0}, {"outputWhite", 1.0}};
    }
    if (type == "exposureColor") {
        return QJsonObject{{"exposure", 0.0}, {"offset", 0.0}};
    }
    if (type == "thresholdColor") {
        return QJsonObject{{"threshold", 0.5}, {"softness", 0.0}};
    }
    if (type == "sepiaColor") {
        return QJsonObject{{"factor", 1.0}};
    }
    if (type == "channelMixer") {
        return QJsonObject{{"red", vectorToJson(QVector3D(1.0f, 0.0f, 0.0f))},
                           {"green", vectorToJson(QVector3D(0.0f, 1.0f, 0.0f))},
                           {"blue", vectorToJson(QVector3D(0.0f, 0.0f, 1.0f))}};
    }
    if (type == "temperatureTint") {
        return QJsonObject{{"temperature", 0.0}, {"tint", 0.0}};
    }
    if (type == "grayscaleColor") {
        return QJsonObject{{"factor", 1.0}};
    }
    if (type == "vibranceColor") {
        return QJsonObject{{"saturation", 1.0}, {"vibrance", 0.0}};
    }
    if (type == "softClipColor") {
        return QJsonObject{{"low", 0.0}, {"high", 1.0}, {"lowSoftness", 0.0}, {"highSoftness", 0.0}};
    }
    if (type == "duotoneColor") {
        return QJsonObject{{"factor", 1.0},
                           {"shadowColor", vectorToJson(QVector3D(0.05f, 0.05f, 0.08f))},
                           {"highlightColor", vectorToJson(QVector3D(1.0f, 0.92f, 0.78f))}};
    }
    if (type == "ascCdl") {
        return QJsonObject{{"slope", vectorToJson(QVector3D(1.0f, 1.0f, 1.0f))},
                           {"offset", vectorToJson(QVector3D(0.0f, 0.0f, 0.0f))},
                           {"power", vectorToJson(QVector3D(1.0f, 1.0f, 1.0f))},
                           {"saturation", 1.0}};
    }
    if (type == "colorRamp") {
        return QJsonObject{{"blackPoint", 0.0},
                           {"whitePoint", 1.0},
                           {"shadowColor", vectorToJson(QVector3D(0.0f, 0.0f, 0.0f))},
                           {"highlightColor", vectorToJson(QVector3D(1.0f, 1.0f, 1.0f))}};
    }
    if (type == "selectiveColor") {
        return QJsonObject{{"targetHue", 0.0}, {"range", 0.12}, {"hue", 0.0}, {"saturation", 0.0}, {"luminance", 0.0}};
    }
    if (type == "lutMix") {
        return QJsonObject{{"strength", 1.0}};
    }
    if (type == "parallelMixer" || type == "layerMixer") {
        return QJsonObject{{"mix", 1.0}, {"weights", QJsonArray{1.0, 1.0, 1.0}}};
    }
    return paramsToJson(params);
}

QStringList inputColorSpaceKeys()
{
    return {"srgb", "linear", "logc3", "slog3", "vlog"};
}

void addSectionLabel(QVBoxLayout* layout, const QString& text)
{
    auto* label = new QLabel(text);
    label->setStyleSheet("font-weight: 600; color: #dfe3ea; padding-top: 8px;");
    layout->addWidget(label);
}

void configureToolButton(QPushButton* button, const char* propertyName)
{
    button->setProperty(propertyName, true);
    button->setCursor(Qt::PointingHandCursor);
}

QPointF warperPointFromColor(const QColor& color)
{
    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
    color.getHsvF(&hue, &saturation, &value);
    if (hue < 0.0f) {
        hue = 0.0f;
    }
    return QPointF(std::clamp(static_cast<double>(hue), 0.0, 1.0),
                   std::clamp(static_cast<double>(saturation), 0.0, 1.0));
}

float warperLumaFromColor(const QColor& color)
{
    return std::clamp(static_cast<float>(color.redF() * 0.2126 + color.greenF() * 0.7152 + color.blueF() * 0.0722), 0.0f, 1.0f);
}

QString warperPointListLabel(int index, const QPointF& source, const QPointF& target, bool pinned)
{
    const QString title = QString("#%1%2")
        .arg(index + 1, 2, 10, QLatin1Char('0'))
        .arg(pinned ? " [锁定]" : "");
    const QString detail = QString("源 %1, %2  ->  目标 %3, %4")
        .arg(source.x(), 0, 'f', 3)
        .arg(source.y(), 0, 'f', 3)
        .arg(target.x(), 0, 'f', 3)
        .arg(target.y(), 0, 'f', 3);
    return QString("%1\n%2").arg(title, detail);
}

QString maskKindLabel(MaskKind kind)
{
    switch (kind) {
    case MaskKind::Gradient:
        return "渐变";
    case MaskKind::Radial:
        return "径向";
    case MaskKind::ColorRange:
        return "限定器";
    case MaskKind::Paint:
        return "绘制";
    case MaskKind::ExternalImage:
        return "图片";
    case MaskKind::Full:
    default:
        return "无遮罩";
    }
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("IKCLUT Studio");
    resize(1500, 900);
    setMinimumSize(980, 640);
    rebuildDefaultStages();

    m_sourceView = new ImageView(this);
    m_sourceView->setLabel("输入");
    m_resultView = new ImageView(this);
    m_resultView->setLabel("输出");
    connect(m_resultView, &ImageView::maskDragCompleted, this, [this](const QPointF& startUv, const QPointF& endUv) {
        applyMaskDrag(startUv, endUv);
    });
    connect(m_resultView, &ImageView::maskPointSampled, this, [this](const QPointF& uv, const QColor&) {
        const MaskAsset* mask = localMaskForActiveStage();
        if (mask && mask->kind == MaskKind::Radial) {
            setLocalMaskParam("centerX", uv.x());
            setLocalMaskParam("centerY", uv.y());
            refreshLocalMaskUi();
            refreshMaskOverlay();
        }
    });

    m_previewSplitter = new QSplitter(Qt::Horizontal, this);
    m_previewSplitter->addWidget(m_sourceView);
    m_previewSplitter->addWidget(m_resultView);
    m_previewSplitter->setStretchFactor(0, 1);
    m_previewSplitter->setStretchFactor(1, 1);
    m_previewSplitter->setChildrenCollapsible(false);

    m_ikClutStripView = new ImageView(this);
    m_ikClutStripView->setLabel("实时 ikClut 1024 x 32");
    m_ikClutStripView->setMinimumHeight(86);
    m_ikClutStripView->setMaximumHeight(118);
    m_ikClutStripView->setOverlayInfo("当前导出 LUT");

    auto* previewColumn = new QWidget(this);
    auto* previewColumnLayout = new QVBoxLayout(previewColumn);
    previewColumnLayout->setContentsMargins(0, 0, 0, 0);
    previewColumnLayout->setSpacing(8);
    previewColumnLayout->addWidget(m_previewSplitter, 1);
    previewColumnLayout->addWidget(m_ikClutStripView, 0);

    m_scopeWidget = new ScopeWidget(this);
    m_lutViewer = new Lut3DViewer(this);
    m_tabs = new ResponsiveTabPanel(this);
    m_tabs->setMinimumWidth(360);
    m_tabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_tabs->addPage(createBasicPanel(), "调色");
    m_tabs->addPage(createWheelsPanel(), "色轮");
    m_tabs->addPage(createCurvesPanel(), "曲线");
    m_tabs->addPage(createWarperPanel(), "色彩变形");
    m_tabs->addPage(createHslPanel(), "HSL");
    m_tabs->addPage(createNodesPanel(), "节点");
    m_tabs->addPage(createProfessionalPanel(), "专业");
    m_tabs->addPage(m_scopeWidget, "示波器");
    m_tabs->addPage(m_lutViewer, "3D LUT");

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(previewColumn);
    m_mainSplitter->addWidget(m_tabs);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 0);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setSizes({1120, 380});
    setCentralWidget(m_mainSplitter);
    applyResponsiveLayout();

    auto* fileMenu = menuBar()->addMenu("文件");
    fileMenu->addAction("打开图片", QKeySequence::Open, this, &MainWindow::openImage);
    fileMenu->addAction("导入 LUT", this, &MainWindow::importLut);
    fileMenu->addSeparator();
    fileMenu->addAction("导出 ikClut PNG", QKeySequence::Save, this, &MainWindow::exportIkClut);
    fileMenu->addAction("导出 CUBE", this, &MainWindow::exportCube);
    fileMenu->addAction("导出 Hald PNG", this, &MainWindow::exportHald);
    fileMenu->addAction("批量套用 / 导出图片", this, &MainWindow::batchExportImages);
    fileMenu->addSeparator();
    fileMenu->addAction("打开项目", this, &MainWindow::openProject);
    fileMenu->addAction("保存项目", this, &MainWindow::saveProject);
    m_recentProjectsMenu = fileMenu->addMenu("最近项目");
    fileMenu->addSeparator();
    fileMenu->addAction("载入预设", this, &MainWindow::loadPreset);
    fileMenu->addAction("保存预设", this, &MainWindow::savePreset);

    auto* analyzeMenu = menuBar()->addMenu("分析");
    analyzeMenu->addAction("LUT 质量检查", this, &MainWindow::analyzeLutQuality);

    auto* toolbar = addToolBar("主工具栏");
    toolbar->setMovable(false);
    toolbar->addAction("打开", this, &MainWindow::openImage);
    toolbar->addAction("导入 LUT", this, &MainWindow::importLut);
    m_undoAction = toolbar->addAction("撤销", this, &MainWindow::undo);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setToolTip("撤销上一次调色更改");
    m_redoAction = toolbar->addAction("重做", this, &MainWindow::redo);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setToolTip("重做刚撤销的调色更改");
    toolbar->addAction("导出", this, &MainWindow::exportIkClut);
    toolbar->addAction("导出 CUBE", this, &MainWindow::exportCube);
    toolbar->addAction("导出 Hald", this, &MainWindow::exportHald);
    toolbar->addAction("检查 LUT", this, &MainWindow::analyzeLutQuality);
    auto* gpuPreviewAction = toolbar->addAction("GPU 预览");
    gpuPreviewAction->setCheckable(true);
    gpuPreviewAction->setChecked(true);
    connect(gpuPreviewAction, &QAction::toggled, this, [this](bool enabled) {
        m_preferGpuPreview = enabled;
        schedulePreview();
    });
    auto* warningMode = new NoWheelComboBox(toolbar);
    warningMode->addItems({"关闭警告", "阴影裁切", "高光裁切", "色域警告", "假色"});
    toolbar->addWidget(warningMode);
    connect(warningMode, &QComboBox::currentIndexChanged, this, [this](int index) {
        ImageWarningOverlay overlay = ImageWarningOverlay::None;
        if (index == 1) {
            overlay = ImageWarningOverlay::ShadowClip;
        } else if (index == 2) {
            overlay = ImageWarningOverlay::HighlightClip;
        } else if (index == 3) {
            overlay = ImageWarningOverlay::Gamut;
        } else if (index == 4) {
            overlay = ImageWarningOverlay::FalseColor;
        }
        if (m_resultView) {
            m_resultView->setWarningOverlay(overlay);
        }
    });
    m_previewModeCombo = new NoWheelComboBox(toolbar);
    m_previewModeCombo->addItems({"调色后", "调色前", "前后对比", "快照 A", "快照 B"});
    toolbar->addWidget(m_previewModeCombo);
    connect(m_previewModeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        updatePreviewDisplay();
        refreshMaskOverlay();
    });
    toolbar->addAction("保存 A", this, [this]() {
        m_snapshotA = m_previewImage;
        setStatus(m_snapshotA.isNull() ? "没有可保存的预览" : "快照 A 已保存");
    });
    toolbar->addAction("保存 B", this, [this]() {
        m_snapshotB = m_previewImage;
        setStatus(m_snapshotB.isNull() ? "没有可保存的预览" : "快照 B 已保存");
    });
    toolbar->addAction("重置 3D", m_lutViewer, &Lut3DViewer::resetView);
    auto* density = new NoWheelComboBox(toolbar);
    density->addItems({"3D 低密度", "3D 中密度", "3D 高密度"});
    density->setCurrentIndex(1);
    toolbar->addWidget(density);
    connect(density, &QComboBox::currentIndexChanged, this, [this](int index) {
        const int sizes[] = {11, 17, 25};
        m_lutViewer->setDisplaySize(sizes[std::clamp(index, 0, 2)]);
        if (m_transformSource.kind == ColorTransformKind::ParamPipeline) {
            m_lutViewer->setLut(m_currentPreviewLut);
        } else if (m_importedLut.isValid()) {
            m_lutViewer->setLut(m_importedLut);
        }
    });
    auto* lutMode = new NoWheelComboBox(toolbar);
    lutMode->addItems({"叠加", "差异线", "热力图"});
    toolbar->addWidget(lutMode);
    connect(lutMode, &QComboBox::currentIndexChanged, this, [this](int index) {
        switch (index) {
        case 1:
            m_lutViewer->setViewMode(Lut3DViewMode::DeltaLines);
            break;
        case 2:
            m_lutViewer->setViewMode(Lut3DViewMode::Heatmap);
            break;
        default:
            m_lutViewer->setViewMode(Lut3DViewMode::Overlay);
            break;
        }
    });

    m_statusLabel = new QLabel(this);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progress);

    m_previewDebounce.setSingleShot(true);
    m_previewDebounce.setInterval(8);
    connect(&m_previewDebounce, &QTimer::timeout, this, [this]() {
        renderPreview(false);
    });

    m_fullPreviewDebounce.setSingleShot(true);
    m_fullPreviewDebounce.setInterval(700);
    connect(&m_fullPreviewDebounce, &QTimer::timeout, this, [this]() {
        renderPreview(true);
    });

    m_lutDebounce.setSingleShot(true);
    m_lutDebounce.setInterval(160);
    connect(&m_lutDebounce, &QTimer::timeout, this, &MainWindow::refreshLutViewerAsync);

    m_undoCoalesceTimer.setSingleShot(true);
    m_undoCoalesceTimer.setInterval(450);
    connect(&m_undoCoalesceTimer, &QTimer::timeout, this, &MainWindow::finalizeUndoCheckpoint);

    m_autosaveTimer.setInterval(30000);
    connect(&m_autosaveTimer, &QTimer::timeout, this, &MainWindow::autosaveNow);
    m_autosaveTimer.start();

    QString dxDetails;
    const bool dxOk = DirectX11Backend::probe(&dxDetails);
    setStatus(dxOk ? dxDetails : dxDetails + " 预览将使用 Qt 回退渲染。");
    m_lutViewer->setLut(Lut3D::identity(17));
    updateIkClutStrip(Lut3D::identity(IkClutExporter::IkClutSize));
    QSettings settings("IkClutStudio", "IkClutStudio");
    m_recentProjects = settings.value("recentProjects").toStringList();
    rebuildRecentProjectsMenu();
    restoreLastSession();
    resetUndoHistory();
    updateWindowTitle();
}

QWidget* MainWindow::createBasicPanel()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* panel = new QWidget(scroll);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto add = [&](QVBoxLayout* targetLayout,
                   QWidget* parent,
                   ParameterControl** storage,
                   const QString& label,
                   double min,
                   double max,
                   double value,
                   double step,
                   int decimals,
                   auto setter) {
        auto* control = new ParameterControl(label, min, max, value, step, decimals, parent);
        if (storage) {
            *storage = control;
        }
        connect(control, &ParameterControl::valueChanged, this, [this, setter](double v) {
            setter(v);
            markDirty();
            schedulePreview();
        });
        targetLayout->addWidget(control);
    };

    auto* stackPanel = new CollapsiblePanel("调整堆栈", panel);
    auto* stackHint = new QLabel("选择一个图层，在下方调整参数，并可使用不透明度、遮罩、独显或混合模式进行合成。", stackPanel);
    stackHint->setWordWrap(true);
    stackHint->setStyleSheet("color: #9fa7b3; padding-bottom: 4px;");
    stackPanel->contentLayout()->addWidget(stackHint);
    m_adjustmentList = new QListWidget(stackPanel);
    m_adjustmentList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_adjustmentList->setMinimumHeight(118);
    stackPanel->contentLayout()->addWidget(m_adjustmentList);
    auto* stackButtons = new QWidget(stackPanel);
    auto* stackButtonsLayout = new QHBoxLayout(stackButtons);
    stackButtonsLayout->setContentsMargins(0, 0, 0, 0);
    stackButtonsLayout->setSpacing(6);
    auto* addStageButton = new QPushButton("添加", stackButtons);
    auto* duplicateStageButton = new QPushButton("复制", stackButtons);
    auto* removeStageButton = new QPushButton("删除", stackButtons);
    auto* moveUpStageButton = new QPushButton("上移", stackButtons);
    auto* moveDownStageButton = new QPushButton("下移", stackButtons);
    configureToolButton(addStageButton, "curveToolButton");
    configureToolButton(duplicateStageButton, "curveToolButton");
    configureToolButton(removeStageButton, "curveToolButton");
    configureToolButton(moveUpStageButton, "curveToolButton");
    configureToolButton(moveDownStageButton, "curveToolButton");
    stackButtonsLayout->addWidget(addStageButton);
    stackButtonsLayout->addWidget(duplicateStageButton);
    stackButtonsLayout->addWidget(removeStageButton);
    stackButtonsLayout->addWidget(moveUpStageButton);
    stackButtonsLayout->addWidget(moveDownStageButton);
    stackButtonsLayout->addStretch(1);
    stackPanel->contentLayout()->addWidget(stackButtons);
    auto* nameRow = new QWidget(stackPanel);
    auto* nameLayout = new QHBoxLayout(nameRow);
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->setSpacing(8);
    auto* nameLabel = new QLabel("名称", nameRow);
    nameLabel->setMinimumWidth(82);
    m_adjustmentNameEdit = new QLineEdit(nameRow);
    m_adjustmentNameEdit->setPlaceholderText("调整名称");
    nameLayout->addWidget(nameLabel);
    nameLayout->addWidget(m_adjustmentNameEdit, 1);
    stackPanel->contentLayout()->addWidget(nameRow);
    m_adjustmentEnabledCheck = new QCheckBox("启用", stackPanel);
    stackPanel->contentLayout()->addWidget(m_adjustmentEnabledCheck);
    m_adjustmentSoloCheck = new QCheckBox("独显", stackPanel);
    stackPanel->contentLayout()->addWidget(m_adjustmentSoloCheck);
    auto* blendRow = new QWidget(stackPanel);
    auto* blendLayout = new QHBoxLayout(blendRow);
    blendLayout->setContentsMargins(0, 0, 0, 0);
    blendLayout->setSpacing(8);
    auto* blendLabel = new QLabel("混合", blendRow);
    blendLabel->setMinimumWidth(82);
    m_adjustmentBlendModeCombo = new NoWheelComboBox(blendRow);
    m_adjustmentBlendModeCombo->addItem("正常", "normal");
    m_adjustmentBlendModeCombo->addItem("颜色", "color");
    m_adjustmentBlendModeCombo->addItem("明度", "luminosity");
    blendLayout->addWidget(blendLabel);
    blendLayout->addWidget(m_adjustmentBlendModeCombo, 1);
    stackPanel->contentLayout()->addWidget(blendRow);
    m_adjustmentOpacityControl = new ParameterControl("不透明度", 0.0, 1.0, 1.0, 0.01, 2, stackPanel);
    stackPanel->contentLayout()->addWidget(m_adjustmentOpacityControl);
    m_bypassAllButton = new QPushButton("全部旁路", stackPanel);
    m_bypassAllButton->setCheckable(true);
    configureToolButton(m_bypassAllButton, "curveToolButton");
    stackPanel->contentLayout()->addWidget(m_bypassAllButton);
    connect(m_adjustmentList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_document.pipeline.stages.size() && row != m_activeStageIndex) {
            syncActiveStageParams();
            loadActiveStageParams(row);
            refreshParameterWidgets();
            schedulePreview();
        }
    });
    connect(addStageButton, &QPushButton::clicked, this, [this]() {
        addAdjustmentStage();
    });
    connect(duplicateStageButton, &QPushButton::clicked, this, [this]() {
        duplicateActiveAdjustmentStage();
    });
    connect(removeStageButton, &QPushButton::clicked, this, [this]() {
        removeActiveAdjustmentStage();
    });
    connect(moveUpStageButton, &QPushButton::clicked, this, [this]() {
        moveActiveAdjustmentStage(-1);
    });
    connect(moveDownStageButton, &QPushButton::clicked, this, [this]() {
        moveActiveAdjustmentStage(1);
    });
    connect(m_adjustmentNameEdit, &QLineEdit::editingFinished, this, [this]() {
        if (!m_adjustmentNameEdit || m_activeStageIndex < 0 || m_activeStageIndex >= m_document.pipeline.stages.size()) {
            return;
        }
        const QString name = m_adjustmentNameEdit->text().trimmed();
        m_document.pipeline.stages[m_activeStageIndex].label = name;
        refreshAdjustmentStackUi();
        markDirty();
    });
    connect(m_adjustmentEnabledCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_activeStageIndex >= 0 && m_activeStageIndex < m_document.pipeline.stages.size()) {
            m_document.pipeline.stages[m_activeStageIndex].enabled = enabled;
            refreshAdjustmentStackUi();
            markDirty();
            schedulePreview();
        }
    });
    connect(m_adjustmentSoloCheck, &QCheckBox::toggled, this, [this](bool solo) {
        if (m_activeStageIndex >= 0 && m_activeStageIndex < m_document.pipeline.stages.size()) {
            if (solo) {
                clearSoloExcept(m_activeStageIndex);
            }
            m_document.pipeline.stages[m_activeStageIndex].solo = solo;
            refreshAdjustmentStackUi();
            markDirty();
            schedulePreview();
        }
    });
    connect(m_adjustmentBlendModeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_adjustmentBlendModeCombo && m_activeStageIndex >= 0 && m_activeStageIndex < m_document.pipeline.stages.size()) {
            m_document.pipeline.stages[m_activeStageIndex].blendMode = m_adjustmentBlendModeCombo->currentData().toString();
            refreshAdjustmentStackUi();
            markDirty();
            schedulePreview();
        }
    });
    connect(m_bypassAllButton, &QPushButton::toggled, this, [this](bool bypassed) {
        m_adjustmentsBypassed = bypassed;
        refreshAdjustmentStackUi();
        schedulePreview();
    });
    connect(m_adjustmentOpacityControl, &ParameterControl::valueChanged, this, [this](double value) {
        if (m_activeStageIndex >= 0 && m_activeStageIndex < m_document.pipeline.stages.size()) {
            m_document.pipeline.stages[m_activeStageIndex].opacity = static_cast<float>(value);
            refreshAdjustmentStackUi();
            markDirty();
            schedulePreview();
        }
    });
    layout->addWidget(stackPanel);

    auto* colorManagement = new CollapsiblePanel("色彩管理", panel);
    auto* colorHint = new QLabel("用于预览和 LUT 导出的输入转换。普通图片请使用 sRGB。", colorManagement);
    colorHint->setWordWrap(true);
    colorHint->setStyleSheet("color: #9fa7b3; padding-bottom: 4px;");
    colorManagement->contentLayout()->addWidget(colorHint);
    auto* inputRow = new QWidget(colorManagement);
    auto* inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(8);
    auto* inputLabel = new QLabel("输入空间", inputRow);
    inputLabel->setMinimumWidth(82);
    m_inputColorSpaceCombo = new NoWheelComboBox(inputRow);
    m_inputColorSpaceCombo->addItem("sRGB / Rec.709", "srgb");
    m_inputColorSpaceCombo->addItem("线性", "linear");
    m_inputColorSpaceCombo->addItem("ARRI LogC3 EI800", "logc3");
    m_inputColorSpaceCombo->addItem("Sony S-Log3", "slog3");
    m_inputColorSpaceCombo->addItem("Panasonic V-Log", "vlog");
    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(m_inputColorSpaceCombo, 1);
    colorManagement->contentLayout()->addWidget(inputRow);
    connect(m_inputColorSpaceCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!m_inputColorSpaceCombo) {
            return;
        }
        const QString key = m_inputColorSpaceCombo->currentData().toString();
        m_document.pipeline.params.inputColorSpace = key.isEmpty() ? QString("srgb") : key;
        markDirty();
        schedulePreview();
    });
    colorManagement->setExpanded(false);
    layout->addWidget(colorManagement);

    auto* localMask = new CollapsiblePanel("局部遮罩", panel);
    auto* maskHint = new QLabel("当前图层的局部遮罩。径向和渐变遮罩仅保留在项目预览中；颜色范围遮罩可导出到 LUT。", localMask);
    maskHint->setWordWrap(true);
    maskHint->setStyleSheet("color: #9fa7b3; padding-bottom: 4px;");
    localMask->contentLayout()->addWidget(maskHint);
    auto* maskTypeRow = new QWidget(localMask);
    auto* maskTypeLayout = new QHBoxLayout(maskTypeRow);
    maskTypeLayout->setContentsMargins(0, 0, 0, 0);
    maskTypeLayout->setSpacing(8);
    auto* maskTypeLabel = new QLabel("类型", maskTypeRow);
    maskTypeLabel->setMinimumWidth(82);
    m_maskTypeCombo = new NoWheelComboBox(maskTypeRow);
    m_maskTypeCombo->addItem("关闭", static_cast<int>(MaskKind::Full));
    m_maskTypeCombo->addItem("线性渐变", static_cast<int>(MaskKind::Gradient));
    m_maskTypeCombo->addItem("径向", static_cast<int>(MaskKind::Radial));
    m_maskTypeCombo->addItem("颜色范围", static_cast<int>(MaskKind::ColorRange));
    m_maskTypeCombo->addItem("绘制", static_cast<int>(MaskKind::Paint));
    m_maskTypeCombo->addItem("图片", static_cast<int>(MaskKind::ExternalImage));
    maskTypeLayout->addWidget(maskTypeLabel);
    maskTypeLayout->addWidget(m_maskTypeCombo, 1);
    localMask->contentLayout()->addWidget(maskTypeRow);

    m_maskInvertCheck = new QCheckBox("反转遮罩", localMask);
    localMask->contentLayout()->addWidget(m_maskInvertCheck);
    m_maskOverlayCheck = new QCheckBox("显示叠加", localMask);
    m_maskOverlayCheck->setChecked(true);
    localMask->contentLayout()->addWidget(m_maskOverlayCheck);
    m_maskViewCheck = new QCheckBox("遮罩视图", localMask);
    localMask->contentLayout()->addWidget(m_maskViewCheck);
    auto* maskToolRow = new QWidget(localMask);
    auto* maskToolLayout = new QHBoxLayout(maskToolRow);
    maskToolLayout->setContentsMargins(0, 0, 0, 0);
    maskToolLayout->setSpacing(6);
    m_maskEditButton = new QPushButton("在图像上编辑", maskToolRow);
    m_maskEditButton->setCheckable(true);
    m_maskPickColorButton = new QPushButton("添加采样", maskToolRow);
    m_maskPickColorButton->setCheckable(true);
    m_maskSubtractColorButton = new QPushButton("减去", maskToolRow);
    m_maskSubtractColorButton->setCheckable(true);
    m_maskResetButton = new QPushButton("重置遮罩", maskToolRow);
    configureToolButton(m_maskEditButton, "curveToolButton");
    configureToolButton(m_maskPickColorButton, "curveToolButton");
    configureToolButton(m_maskSubtractColorButton, "curveToolButton");
    configureToolButton(m_maskResetButton, "curveToolButton");
    maskToolLayout->addWidget(m_maskEditButton);
    maskToolLayout->addWidget(m_maskPickColorButton);
    maskToolLayout->addWidget(m_maskSubtractColorButton);
    maskToolLayout->addWidget(m_maskResetButton);
    localMask->contentLayout()->addWidget(maskToolRow);
    m_maskClearSamplesButton = new QPushButton("清除限定器采样", localMask);
    configureToolButton(m_maskClearSamplesButton, "curveToolButton");
    localMask->contentLayout()->addWidget(m_maskClearSamplesButton);
    m_maskUndoStrokeButton = new QPushButton("撤销绘制笔画", localMask);
    configureToolButton(m_maskUndoStrokeButton, "curveToolButton");
    localMask->contentLayout()->addWidget(m_maskUndoStrokeButton);
    m_maskLoadImageButton = new QPushButton("载入遮罩图片", localMask);
    configureToolButton(m_maskLoadImageButton, "curveToolButton");
    localMask->contentLayout()->addWidget(m_maskLoadImageButton);
    m_maskOpacityControl = new ParameterControl("不透明度", 0.0, 1.0, 1.0, 0.01, 2, localMask);
    m_maskParamAControl = new ParameterControl("A", 0.0, 1.0, 0.5, 0.01, 2, localMask);
    m_maskParamBControl = new ParameterControl("B", 0.0, 1.0, 0.5, 0.01, 2, localMask);
    m_maskParamCControl = new ParameterControl("C", 0.0, 1.0, 0.5, 0.01, 2, localMask);
    m_maskParamDControl = new ParameterControl("D", 0.0, 1.0, 0.18, 0.01, 2, localMask);
    m_maskParamEControl = new ParameterControl("E", 0.0, 1.0, 0.12, 0.01, 2, localMask);
    localMask->contentLayout()->addWidget(m_maskOpacityControl);
    localMask->contentLayout()->addWidget(m_maskParamAControl);
    localMask->contentLayout()->addWidget(m_maskParamBControl);
    localMask->contentLayout()->addWidget(m_maskParamCControl);
    localMask->contentLayout()->addWidget(m_maskParamDControl);
    localMask->contentLayout()->addWidget(m_maskParamEControl);
    connect(m_maskTypeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        const MaskKind kind = static_cast<MaskKind>(m_maskTypeCombo->currentData().toInt());
        setLocalMaskKind(kind);
        refreshLocalMaskUi();
        markDirty();
        schedulePreview();
    });
    connect(m_maskInvertCheck, &QCheckBox::toggled, this, [this](bool checked) {
        ensureLocalMaskAsset();
        if (MaskAsset* mask = localMaskForActiveStage()) {
            mask->inverted = checked;
        }
        markDirty();
        refreshMaskOverlay();
        schedulePreview();
    });
    connect(m_maskOverlayCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_maskOverlayEnabled = checked;
        refreshMaskOverlay();
    });
    connect(m_maskViewCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_maskViewEnabled = checked;
        refreshMaskOverlay();
    });
    connect(m_maskEditButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            const MaskAsset* mask = localMaskForActiveStage();
            if (!mask || mask->kind == MaskKind::Full || !mask->enabled) {
                setLocalMaskKind(MaskKind::Radial);
                refreshLocalMaskUi();
                markDirty();
                schedulePreview();
            }
        }
        m_maskEditMode = checked;
        if (m_resultView) {
            const MaskAsset* mask = localMaskForActiveStage();
            const QString hint = mask && mask->kind == MaskKind::Paint
                ? (m_maskSubtractSample ? "在输出图像上拖动以擦除绘制遮罩" : "在输出图像上拖动以绘制当前遮罩")
                : QString("在输出图像上拖动以放置当前遮罩");
            m_resultView->setMaskEditMode(checked, checked ? hint : QString());
        }
        refreshMaskOverlay();
    });
    connect(m_maskPickColorButton, &QPushButton::toggled, this, [this](bool checked) {
        setMaskColorPickMode(checked, false);
    });
    connect(m_maskSubtractColorButton, &QPushButton::toggled, this, [this](bool checked) {
        const MaskAsset* mask = localMaskForActiveStage();
        if (mask && mask->kind == MaskKind::Paint) {
            m_maskSubtractSample = checked;
            if (m_maskColorPickMode) {
                setMaskColorPickMode(false);
            }
            if (m_resultView && m_maskEditMode) {
                m_resultView->setMaskEditMode(true,
                                              m_maskSubtractSample
                                                  ? "在输出图像上拖动以擦除绘制遮罩"
                                                  : "在输出图像上拖动以绘制当前遮罩");
            }
            refreshLocalMaskUi();
            return;
        }
        setMaskColorPickMode(checked, true);
    });
    connect(m_maskResetButton, &QPushButton::clicked, this, [this]() {
        resetActiveMask();
        markDirty();
        schedulePreview();
    });
    connect(m_maskClearSamplesButton, &QPushButton::clicked, this, [this]() {
        if (MaskAsset* mask = localMaskForActiveStage()) {
            if (mask->kind == MaskKind::Paint) {
                mask->params["strokes"] = QJsonArray{};
            } else if (mask->kind == MaskKind::ExternalImage) {
                mask->params.remove("imagePath");
            } else {
                mask->params.remove("samples");
            }
            markDirty();
            refreshLocalMaskUi();
            refreshMaskOverlay();
            schedulePreview();
        }
    });
    connect(m_maskUndoStrokeButton, &QPushButton::clicked, this, [this]() {
        if (MaskAsset* mask = localMaskForActiveStage()) {
            if (mask->kind != MaskKind::Paint) {
                return;
            }
            QJsonArray strokes = mask->params.value("strokes").toArray();
            if (strokes.isEmpty()) {
                return;
            }
            strokes.removeLast();
            mask->params["strokes"] = strokes;
            markDirty();
            refreshLocalMaskUi();
            refreshMaskOverlay();
            schedulePreview();
        }
    });
    connect(m_maskLoadImageButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, "载入遮罩图片", QString(), "图片 (*.png *.jpg *.jpeg *.tif *.tiff)");
        if (path.isEmpty()) {
            return;
        }
        setLocalMaskKind(MaskKind::ExternalImage);
        if (MaskAsset* mask = localMaskForActiveStage()) {
            mask->params["imagePath"] = path;
        }
        refreshLocalMaskUi();
        markDirty();
        schedulePreview();
    });
    connect(m_maskOpacityControl, &ParameterControl::valueChanged, this, [this](double value) {
        ensureLocalMaskAsset();
        if (MaskAsset* mask = localMaskForActiveStage()) {
            mask->opacity = static_cast<float>(value);
        }
        markDirty();
        refreshMaskOverlay();
        schedulePreview();
    });
    auto connectMaskParam = [this](ParameterControl* control, int paramIndex) {
        connect(control, &ParameterControl::valueChanged, this, [this, paramIndex](double value) {
            QString key;
            const MaskAsset* mask = localMaskForActiveStage();
            const MaskKind kind = mask ? mask->kind : MaskKind::Full;
            if (kind == MaskKind::Gradient) {
                const QStringList keys = {"angle", "position", "softness"};
                if (paramIndex < keys.size()) key = keys.at(paramIndex);
            } else if (kind == MaskKind::Radial) {
                const QStringList keys = {"centerX", "centerY", "radius", "softness"};
                if (paramIndex < keys.size()) key = keys.at(paramIndex);
            } else if (kind == MaskKind::ColorRange) {
                const QStringList keys = {"hue", "saturation", "luma", "tolerance", "softness"};
                if (paramIndex < keys.size()) key = keys.at(paramIndex);
            } else if (kind == MaskKind::Paint) {
                const QStringList keys = {"brushRadius", "softness"};
                if (paramIndex < keys.size()) key = keys.at(paramIndex);
            }
            if (!key.isEmpty()) {
                setLocalMaskParam(key, value);
                markDirty();
                refreshMaskOverlay();
                schedulePreview();
            }
        });
    };
    connectMaskParam(m_maskParamAControl, 0);
    connectMaskParam(m_maskParamBControl, 1);
    connectMaskParam(m_maskParamCControl, 2);
    connectMaskParam(m_maskParamDControl, 3);
    connectMaskParam(m_maskParamEControl, 4);
    localMask->setExpanded(false);
    localMask->hide();

    auto* basic = new CollapsiblePanel("基础", panel);
    add(basic->contentLayout(), basic, &m_temperatureControl, "色温", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.temperature = static_cast<float>(v); });
    add(basic->contentLayout(), basic, &m_tintControl, "色调", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.tint = static_cast<float>(v); });
    add(basic->contentLayout(), basic, &m_exposureControl, "曝光", -3.0, 3.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.exposure = static_cast<float>(v); });
    add(basic->contentLayout(), basic, &m_contrastControl, "对比度", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.contrast = static_cast<float>(v); });
    add(basic->contentLayout(), basic, &m_highlightsControl, "高光", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.highlights = static_cast<float>(v); });
    add(basic->contentLayout(), basic, &m_shadowsControl, "阴影", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.shadows = static_cast<float>(v); });
    add(basic->contentLayout(), basic, &m_whitesControl, "白场", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.whites = static_cast<float>(v); });
    add(basic->contentLayout(), basic, &m_blacksControl, "黑场", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.blacks = static_cast<float>(v); });
    layout->addWidget(basic);

    auto* lutMix = new CollapsiblePanel("导入 LUT 混合", panel);
    auto* lutHint = new QLabel("外部 LUT 会在当前调色之后应用，并可在 0-100% 之间混合。", lutMix);
    lutHint->setWordWrap(true);
    lutHint->setStyleSheet("color: #9fa7b3; padding-bottom: 4px;");
    lutMix->contentLayout()->addWidget(lutHint);
    m_lutStrengthControl = new ParameterControl("强度", 0.0, 1.0, 0.0, 0.01, 2, lutMix);
    connect(m_lutStrengthControl, &ParameterControl::valueChanged, this, [this](double value) {
        m_document.pipeline.params.importedLutStrength = static_cast<float>(value);
        syncTransformSource();
        markDirty();
        schedulePreview();
    });
    m_lutStrengthControl->setEnabled(false);
    lutMix->contentLayout()->addWidget(m_lutStrengthControl);
    m_clearImportedLutButton = new QPushButton("移除导入 LUT", lutMix);
    configureToolButton(m_clearImportedLutButton, "curveToolButton");
    connect(m_clearImportedLutButton, &QPushButton::clicked, this, &MainWindow::clearImportedLut);
    lutMix->contentLayout()->addWidget(m_clearImportedLutButton);
    lutMix->setExpanded(false);
    layout->addWidget(lutMix);

    auto* presence = new CollapsiblePanel("质感", panel);
    add(presence->contentLayout(), presence, &m_textureControl, "纹理", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.texture = static_cast<float>(v); });
    add(presence->contentLayout(), presence, &m_clarityControl, "清晰度", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.clarity = static_cast<float>(v); });
    add(presence->contentLayout(), presence, &m_dehazeControl, "去雾", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.dehaze = static_cast<float>(v); });
    add(presence->contentLayout(), presence, &m_vibranceControl, "自然饱和度", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.vibrance = static_cast<float>(v); });
    add(presence->contentLayout(), presence, &m_saturationControl, "饱和度", 0.0, 2.5, 1.0, 0.01, 2, [this](double v) { m_document.pipeline.params.saturation = static_cast<float>(v); });
    layout->addWidget(presence);

    auto* detail = new CollapsiblePanel("细节", panel);
    add(detail->contentLayout(), detail, &m_sharpenControl, "锐化", 0.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.sharpen = static_cast<float>(v); });
    detail->setExpanded(false);
    layout->addWidget(detail);

    auto* calibration = new CollapsiblePanel("校准", panel);
    add(calibration->contentLayout(), calibration, &m_calibrationRedHueControl, "红色色相", -60.0, 60.0, 0.0, 1.0, 0, [this](double v) { m_document.pipeline.params.calibrationRedHue = static_cast<float>(v); });
    add(calibration->contentLayout(), calibration, &m_calibrationRedSaturationControl, "红色饱和", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.calibrationRedSaturation = static_cast<float>(v); });
    add(calibration->contentLayout(), calibration, &m_calibrationGreenHueControl, "绿色色相", -60.0, 60.0, 0.0, 1.0, 0, [this](double v) { m_document.pipeline.params.calibrationGreenHue = static_cast<float>(v); });
    add(calibration->contentLayout(), calibration, &m_calibrationGreenSaturationControl, "绿色饱和", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.calibrationGreenSaturation = static_cast<float>(v); });
    add(calibration->contentLayout(), calibration, &m_calibrationBlueHueControl, "蓝色色相", -60.0, 60.0, 0.0, 1.0, 0, [this](double v) { m_document.pipeline.params.calibrationBlueHue = static_cast<float>(v); });
    add(calibration->contentLayout(), calibration, &m_calibrationBlueSaturationControl, "蓝色饱和", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.calibrationBlueSaturation = static_cast<float>(v); });
    calibration->setExpanded(false);
    layout->addWidget(calibration);

    layout->addStretch(1);
    scroll->setWidget(panel);
    return scroll;
}

QWidget* MainWindow::createWheelsPanel()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* panel = new QWidget(scroll);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto makeWheelCard = [&](const QString& title,
                             ColorWheelControl*& wheel,
                             ParameterControl*& xControl,
                             ParameterControl*& yControl,
                             ParameterControl*& zControl,
                             QVector3D ColorGradeParams::*member,
                             const QVector3D& baseValue,
                             const QVector3D& minimum,
                             const QVector3D& maximum,
                             const QVector3D& deltaMax) {
        auto* box = new CollapsiblePanel(title, panel);
        auto* wheelRow = new QWidget(box);
        auto* wheelLayout = new QVBoxLayout(wheelRow);
        wheelLayout->setContentsMargins(0, 0, 0, 0);
        wheelLayout->setSpacing(8);

        wheel = new ColorWheelControl(title, baseValue.x(), minimum.x(), maximum.x(), deltaMax.x(), wheelRow);
        wheel->setMinimumHeight(178);
        wheel->setMaximumHeight(220);
        wheel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        wheelLayout->addWidget(wheel);

        auto* valuesColumn = new QWidget(wheelRow);
        auto* valuesLayout = new QVBoxLayout(valuesColumn);
        valuesLayout->setContentsMargins(0, 0, 0, 0);
        valuesLayout->setSpacing(6);
        xControl = new ParameterControl("R", minimum.x(), maximum.x(), baseValue.x(), 0.01, 2, valuesColumn);
        yControl = new ParameterControl("G", minimum.y(), maximum.y(), baseValue.y(), 0.01, 2, valuesColumn);
        zControl = new ParameterControl("B", minimum.z(), maximum.z(), baseValue.z(), 0.01, 2, valuesColumn);
        valuesLayout->addWidget(xControl);
        valuesLayout->addWidget(yControl);
        valuesLayout->addWidget(zControl);
        wheelLayout->addWidget(valuesColumn);

        auto syncVector = [this, wheel, member, xControl, yControl, zControl](const QVector3D& vec) {
            m_document.pipeline.params.*member = vec;
            QSignalBlocker bx(xControl);
            QSignalBlocker by(yControl);
            QSignalBlocker bz(zControl);
            xControl->setValue(vec.x());
            yControl->setValue(vec.y());
            zControl->setValue(vec.z());
            QSignalBlocker bw(wheel);
            wheel->setValue(vec);
            markDirty();
            schedulePreview();
        };

        connect(wheel, &ColorWheelControl::valueChanged, this, syncVector);
        connect(xControl, &ParameterControl::valueChanged, this, [syncVector, yControl, zControl](double value) {
            syncVector(QVector3D(static_cast<float>(value),
                                 static_cast<float>(yControl->value()),
                                 static_cast<float>(zControl->value())));
        });
        connect(yControl, &ParameterControl::valueChanged, this, [syncVector, xControl, zControl](double value) {
            syncVector(QVector3D(static_cast<float>(xControl->value()),
                                 static_cast<float>(value),
                                 static_cast<float>(zControl->value())));
        });
        connect(zControl, &ParameterControl::valueChanged, this, [syncVector, xControl, yControl](double value) {
            syncVector(QVector3D(static_cast<float>(xControl->value()),
                                 static_cast<float>(yControl->value()),
                                 static_cast<float>(value)));
        });

        box->contentLayout()->addWidget(wheelRow);
        return box;
    };

    auto* lift = makeWheelCard("提升", m_liftWheel, m_wheelControls[0], m_wheelControls[1], m_wheelControls[2],
                               &ColorGradeParams::lift, QVector3D(0.0f, 0.0f, 0.0f),
                               QVector3D(-0.1f, -0.1f, -0.1f), QVector3D(0.1f, 0.1f, 0.1f), QVector3D(0.1f, 0.1f, 0.1f));
    auto* gamma = makeWheelCard("伽马", m_gammaWheel, m_wheelControls[3], m_wheelControls[4], m_wheelControls[5],
                                &ColorGradeParams::gamma, QVector3D(1.0f, 1.0f, 1.0f),
                                QVector3D(0.5f, 0.5f, 0.5f), QVector3D(1.5f, 1.5f, 1.5f), QVector3D(0.5f, 0.5f, 0.5f));
    auto* gain = makeWheelCard("增益", m_gainWheel, m_wheelControls[6], m_wheelControls[7], m_wheelControls[8],
                               &ColorGradeParams::gain, QVector3D(1.0f, 1.0f, 1.0f),
                               QVector3D(0.5f, 0.5f, 0.5f), QVector3D(2.0f, 2.0f, 2.0f), QVector3D(1.0f, 1.0f, 1.0f));
    auto* offset = makeWheelCard("偏移", m_offsetWheel, m_wheelControls[9], m_wheelControls[10], m_wheelControls[11],
                                 &ColorGradeParams::offset, QVector3D(0.0f, 0.0f, 0.0f),
                                 QVector3D(-0.1f, -0.1f, -0.1f), QVector3D(0.1f, 0.1f, 0.1f), QVector3D(0.1f, 0.1f, 0.1f));

    layout->addWidget(lift);
    layout->addWidget(gamma);
    layout->addWidget(gain);
    layout->addWidget(offset);
    layout->addStretch(1);
    scroll->setWidget(panel);
    return scroll;
}

QWidget* MainWindow::createCurvesPanel()
{
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* header = new QWidget(panel);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);
    auto makeChannelButton = [&](QPushButton*& storage, const QString& text, const QColor& color, int index) {
        auto* button = new QPushButton(text, header);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        configureToolButton(button, "curveChannelButton");
        button->setStyleSheet(QString("QPushButton[curveChannelButton=\"true\"] { color: %1; font-weight: 600; }")
                                  .arg(color.name()));
        connect(button, &QPushButton::clicked, this, [this, index]() {
            setActiveCurveChannel(index);
        });
        headerLayout->addWidget(button);
        storage = button;
        return button;
    };
    makeChannelButton(m_curveMasterButton, "主曲线", QColor(225, 225, 225), 0);
    makeChannelButton(m_curveRedButton, "红", QColor(244, 89, 89), 1);
    makeChannelButton(m_curveGreenButton, "绿", QColor(112, 211, 118), 2);
    makeChannelButton(m_curveBlueButton, "蓝", QColor(92, 166, 255), 3);
    if (m_curveMasterButton) {
        m_curveMasterButton->setChecked(true);
    }
    layout->addWidget(header);

    auto* quickTools = new QWidget(panel);
    auto* quickLayout = new QHBoxLayout(quickTools);
    quickLayout->setContentsMargins(0, 0, 0, 0);
    quickLayout->setSpacing(6);
    m_curveAutoHandleButton = new QPushButton("自动", quickTools);
    m_curveVectorHandleButton = new QPushButton("直线", quickTools);
    auto* fitButton = new QPushButton("适配", quickTools);
    auto* smoothButton = new QPushButton("平滑", quickTools);
    auto* resetButton = new QPushButton("重置当前", quickTools);
    m_curveAutoHandleButton->setCheckable(true);
    m_curveVectorHandleButton->setCheckable(true);
    m_curveAutoHandleButton->setAutoExclusive(true);
    m_curveVectorHandleButton->setAutoExclusive(true);
    configureToolButton(m_curveAutoHandleButton, "curveToolButton");
    configureToolButton(m_curveVectorHandleButton, "curveToolButton");
    configureToolButton(fitButton, "curveToolButton");
    configureToolButton(smoothButton, "curveToolButton");
    configureToolButton(resetButton, "curveToolButton");
    quickLayout->addWidget(m_curveAutoHandleButton);
    quickLayout->addWidget(m_curveVectorHandleButton);
    quickLayout->addWidget(fitButton);
    quickLayout->addWidget(smoothButton);
    quickLayout->addWidget(resetButton);
    quickLayout->addStretch(1);
    layout->addWidget(quickTools);

    m_curveWidget = new CurveWidget(panel);
    m_curveWidget->setCurve(m_document.pipeline.params.masterCurve);
    m_curveWidget->setHandleModes(m_document.pipeline.params.masterCurveHandles);
    m_curveWidget->setChannelColor(QColor(225, 225, 225));
    layout->addWidget(m_curveWidget, 1);

    m_curvePointTable = new QTableWidget(panel);
    m_curvePointTable->setColumnCount(3);
    m_curvePointTable->setHorizontalHeaderLabels({"输入", "输出", "手柄"});
    m_curvePointTable->verticalHeader()->setVisible(false);
    m_curvePointTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_curvePointTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_curvePointTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_curvePointTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_curvePointTable->setMaximumHeight(142);
    m_curvePointTable->setAlternatingRowColors(true);
    layout->addWidget(m_curvePointTable);

    auto* buttonRow = new QWidget(panel);
    auto* buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    auto* resetAllButton = new QPushButton("全部重置", buttonRow);
    configureToolButton(resetAllButton, "curveToolButton");
    buttonLayout->addWidget(resetAllButton);
    layout->addWidget(buttonRow);

    auto* softClip = new CollapsiblePanel("柔和裁切", panel);
    auto* softLayout = softClip->contentLayout();
    auto addSoft = [&](ParameterControl*& storage,
                       const QString& label,
                       double min,
                       double max,
                       double value,
                       double step,
                       int decimals,
                       auto setter) {
        storage = new ParameterControl(label, min, max, value, step, decimals, softClip);
        connect(storage, &ParameterControl::valueChanged, this, [this, setter](double v) {
            setter(v);
            markDirty();
            schedulePreview();
        });
        softLayout->addWidget(storage);
    };
    addSoft(m_softClipLowControl, "低端", 0.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.softClipLow = static_cast<float>(v); });
    addSoft(m_softClipHighControl, "高端", 0.0, 1.0, 1.0, 0.01, 2, [this](double v) { m_document.pipeline.params.softClipHigh = static_cast<float>(v); });
    addSoft(m_softClipLowSoftnessControl, "低端柔和", 0.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.softClipLowSoftness = static_cast<float>(v); });
    addSoft(m_softClipHighSoftnessControl, "高端柔和", 0.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.softClipHighSoftness = static_cast<float>(v); });
    softClip->setExpanded(false);
    layout->addWidget(softClip);

    connect(m_curveMasterButton, &QPushButton::clicked, this, [this]() {
        if (m_curveWidget) m_curveWidget->setChannelColor(QColor(225, 225, 225));
    });
    connect(m_curveRedButton, &QPushButton::clicked, this, [this]() {
        if (m_curveWidget) m_curveWidget->setChannelColor(QColor(244, 89, 89));
    });
    connect(m_curveGreenButton, &QPushButton::clicked, this, [this]() {
        if (m_curveWidget) m_curveWidget->setChannelColor(QColor(112, 211, 118));
    });
    connect(m_curveBlueButton, &QPushButton::clicked, this, [this]() {
        if (m_curveWidget) m_curveWidget->setChannelColor(QColor(92, 166, 255));
    });
    connect(m_curveWidget, &CurveWidget::curveChanged, this, [this](const QVector<QPointF>& curve) {
        setCurveForChannel(m_currentCurveChannel, curve);
        refreshCurvePointTable();
        markDirty();
        schedulePreview();
    });
    connect(m_curveWidget, &CurveWidget::handleModesChanged, this, [this](const QVector<CurveHandleMode>& handles) {
        setCurveHandlesForChannel(m_currentCurveChannel, handles);
        refreshCurvePointTable();
        refreshCurveHandleButtons();
        markDirty();
        schedulePreview();
    });
    connect(m_curveWidget, &CurveWidget::selectedPointChanged, this, [this](int index, CurveHandleMode) {
        selectCurvePointTableRow(index);
        refreshCurveHandleButtons();
    });
    connect(m_curvePointTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item) {
            applyCurvePointTableEdit(item->row(), item->column());
        }
    });
    connect(m_curvePointTable, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        if (m_curveWidget) {
            m_curveWidget->setSelectedPoint(currentRow);
        }
    });
    connect(m_curveAutoHandleButton, &QPushButton::clicked, this, [this]() {
        if (m_curveWidget) {
            m_curveWidget->setSelectedHandleMode(CurveHandleMode::Auto);
        }
    });
    connect(m_curveVectorHandleButton, &QPushButton::clicked, this, [this]() {
        if (m_curveWidget) {
            m_curveWidget->setSelectedHandleMode(CurveHandleMode::Vector);
        }
    });
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        resetCurveChannel(m_currentCurveChannel);
        markDirty();
        schedulePreview();
    });
    connect(fitButton, &QPushButton::clicked, this, [this]() {
        if (!m_curveWidget) {
            return;
        }
        QVector<QPointF> fitted = m_curveWidget->curve();
        if (fitted.size() >= 2) {
            fitted.first().setY(0.0);
            fitted.last().setY(1.0);
            m_curveWidget->setCurve(fitted);
        }
    });
    connect(smoothButton, &QPushButton::clicked, this, [this]() {
        if (!m_curveWidget) {
            return;
        }
        QVector<QPointF> curve = m_curveWidget->curve();
        for (int i = 1; i + 1 < curve.size(); ++i) {
            curve[i].setY((curve[i - 1].y() + curve[i].y() + curve[i + 1].y()) / 3.0);
        }
        m_curveWidget->setCurve(curve);
    });
    connect(resetAllButton, &QPushButton::clicked, this, [this]() {
        resetAllCurves();
        markDirty();
        schedulePreview();
    });

    refreshCurveHandleButtons();
    refreshCurvePointTable();
    return panel;
}

QWidget* MainWindow::createWarperPanel()
{
    auto* panel = new QWidget(this);
    auto* root = new QHBoxLayout(panel);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    auto* canvasColumn = new QWidget(panel);
    auto* canvasLayout = new QVBoxLayout(canvasColumn);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    canvasLayout->setSpacing(8);

    auto* title = new QLabel("色彩变形 - 色相 / 饱和度", canvasColumn);
    title->setStyleSheet("font-weight: 700; color: #e7eaf0;");
    canvasLayout->addWidget(title);

    m_colorWarper = new ColorWarperWidget(canvasColumn);
    m_colorWarper->setMinimumSize(520, 320);
    m_colorWarper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    canvasLayout->addWidget(m_colorWarper, 1);
    root->addWidget(canvasColumn, 1);

    auto* tools = new QWidget(panel);
    tools->setObjectName("warperSidePanel");
    tools->setMinimumWidth(230);
    tools->setMaximumWidth(280);
    auto* toolsLayout = new QVBoxLayout(tools);
    toolsLayout->setContentsMargins(10, 10, 10, 10);
    toolsLayout->setSpacing(8);

    auto* toolsTitle = new QLabel("工具", tools);
    toolsTitle->setStyleSheet("font-weight: 700; color: #e7eaf0;");
    toolsLayout->addWidget(toolsTitle);

    auto* toolRows = new QWidget(tools);
    auto* toolRowsLayout = new QVBoxLayout(toolRows);
    toolRowsLayout->setContentsMargins(0, 0, 0, 0);
    toolRowsLayout->setSpacing(6);
    auto* toolRowPrimary = new QWidget(toolRows);
    auto* toolRowPrimaryLayout = new QHBoxLayout(toolRowPrimary);
    toolRowPrimaryLayout->setContentsMargins(0, 0, 0, 0);
    toolRowPrimaryLayout->setSpacing(6);
    auto* toolRowSecondary = new QWidget(toolRows);
    auto* toolRowSecondaryLayout = new QHBoxLayout(toolRowSecondary);
    toolRowSecondaryLayout->setContentsMargins(0, 0, 0, 0);
    toolRowSecondaryLayout->setSpacing(6);
    auto* warperReset = new QPushButton("重置", toolRowPrimary);
    auto* warperUndo = new QPushButton("撤销", toolRowPrimary);
    auto* warperRedo = new QPushButton("重做", toolRowPrimary);
    m_warperDeleteButton = new QPushButton("删除", toolRowSecondary);
    m_warperPinButton = new QPushButton("锁定", toolRowSecondary);
    auto* warperSnap = new QPushButton("吸附", toolRowSecondary);
    configureToolButton(warperReset, "warperToolButton");
    configureToolButton(warperUndo, "warperToolButton");
    configureToolButton(warperRedo, "warperToolButton");
    configureToolButton(m_warperDeleteButton, "warperToolButton");
    configureToolButton(m_warperPinButton, "warperToolButton");
    configureToolButton(warperSnap, "warperToolButton");
    warperUndo->setEnabled(false);
    warperRedo->setEnabled(false);
    m_warperDeleteButton->setEnabled(false);
    m_warperPinButton->setCheckable(true);
    warperSnap->setCheckable(true);
    warperSnap->setChecked(false);
    toolRowPrimaryLayout->addWidget(warperReset);
    toolRowPrimaryLayout->addWidget(warperUndo);
    toolRowPrimaryLayout->addWidget(warperRedo);
    toolRowSecondaryLayout->addWidget(m_warperDeleteButton);
    toolRowSecondaryLayout->addWidget(m_warperPinButton);
    toolRowSecondaryLayout->addWidget(warperSnap);
    toolRowsLayout->addWidget(toolRowPrimary);
    toolRowsLayout->addWidget(toolRowSecondary);
    toolsLayout->addWidget(toolRows);

    m_warperPickImageButton = new QPushButton("从图像拾取", tools);
    configureToolButton(m_warperPickImageButton, "warperToolButton");
    m_warperPickImageButton->setCheckable(true);
    toolsLayout->addWidget(m_warperPickImageButton);

    m_warperCoordinateMode = new NoWheelComboBox(tools);
    m_warperCoordinateMode->addItems({"色相 / 饱和度", "平面 X/Y"});
    toolsLayout->addWidget(m_warperCoordinateMode);

    m_warperSelectionLabel = new QLabel("未选择点", tools);
    m_warperSelectionLabel->setStyleSheet("color: #aeb5c0; padding-top: 8px;");
    toolsLayout->addWidget(m_warperSelectionLabel);

    auto* pointListLabel = new QLabel("控制点", tools);
    pointListLabel->setStyleSheet("font-weight: 700; color: #dfe3ea; padding-top: 6px;");
    toolsLayout->addWidget(pointListLabel);
    m_warperPointList = new QListWidget(tools);
    m_warperPointList->setMinimumHeight(140);
    m_warperPointList->setSelectionMode(QAbstractItemView::SingleSelection);
    toolsLayout->addWidget(m_warperPointList);

    auto* sourceLabel = new QLabel("源", tools);
    sourceLabel->setStyleSheet("font-weight: 700; color: #dfe3ea; padding-top: 6px;");
    toolsLayout->addWidget(sourceLabel);
    m_warperSourceXControl = new ParameterControl("色相", 0.0, 1.0, 0.0, 0.001, 3, tools);
    m_warperSourceYControl = new ParameterControl("饱和度", 0.0, 1.0, 0.65, 0.001, 3, tools);
    m_warperSourceLumaControl = new ParameterControl("亮度", 0.0, 1.0, 0.5, 0.001, 3, tools);
    toolsLayout->addWidget(m_warperSourceXControl);
    toolsLayout->addWidget(m_warperSourceYControl);
    toolsLayout->addWidget(m_warperSourceLumaControl);

    auto* targetLabel = new QLabel("目标", tools);
    targetLabel->setStyleSheet("font-weight: 700; color: #dfe3ea; padding-top: 6px;");
    toolsLayout->addWidget(targetLabel);
    m_warperTargetXControl = new ParameterControl("色相", 0.0, 1.0, 0.0, 0.001, 3, tools);
    m_warperTargetYControl = new ParameterControl("饱和度", 0.0, 1.0, 0.65, 0.001, 3, tools);
    m_warperTargetLumaControl = new ParameterControl("亮度", 0.0, 1.0, 0.5, 0.001, 3, tools);
    toolsLayout->addWidget(m_warperTargetXControl);
    toolsLayout->addWidget(m_warperTargetYControl);
    toolsLayout->addWidget(m_warperTargetLumaControl);

    auto* hint = new QLabel("点击色域图采样源颜色，可用列表管理控制点，也可以直接从输入图像拾取。", tools);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #7f8794; padding-top: 8px;");
    toolsLayout->addWidget(hint);
    toolsLayout->addStretch(1);
    root->addWidget(tools);

    connect(m_colorWarper, &ColorWarperWidget::warpVectorsChanged, this, [this](const QVector<QPointF>& sources,
                                                                                const QVector<QPointF>& targets,
                                                                                const QVector<bool>& pinned) {
        reconcileWarperLuma(sources, targets);
        m_document.pipeline.params.colorWarpSources = sources;
        m_document.pipeline.params.colorWarpPoints = targets;
        m_document.pipeline.params.colorWarpPinned = pinned;
        m_warperSelectedSources = sources;
        m_warperSelectedTargets = targets;
        m_warperSelectedPinned = pinned;
        markDirty();
        schedulePreview();
    });
    connect(m_colorWarper, &ColorWarperWidget::selectionChanged, this, [this](int index, const QPointF&) {
        m_warperSelectedIndex = index;
        refreshWarperSelectionUi();
    });
    connect(m_colorWarper, &ColorWarperWidget::undoAvailabilityChanged, this, [warperUndo, warperRedo](bool canUndo, bool canRedo) {
        warperUndo->setEnabled(canUndo);
        warperRedo->setEnabled(canRedo);
    });
    connect(warperReset, &QPushButton::clicked, this, [this]() {
        if (m_colorWarper) {
            m_colorWarper->resetWarp();
        }
    });
    connect(warperUndo, &QPushButton::clicked, this, [this]() {
        if (m_colorWarper) {
            m_colorWarper->undo();
        }
    });
    connect(warperRedo, &QPushButton::clicked, this, [this]() {
        if (m_colorWarper) {
            m_colorWarper->redo();
        }
    });
    connect(m_warperDeleteButton, &QPushButton::clicked, this, [this]() {
        if (m_colorWarper) {
            m_colorWarper->deleteSelectedPoint();
        }
    });
    connect(m_warperPinButton, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_colorWarper) {
            return;
        }
        if (m_colorWarper->selectedPoint() >= 0) {
            const int index = m_colorWarper->selectedPoint();
            m_colorWarper->setSelectedPinned(checked);
            if (checked
                && index < m_document.pipeline.params.colorWarpSourceLuma.size()
                && index < m_document.pipeline.params.colorWarpTargetLuma.size()) {
                m_document.pipeline.params.colorWarpTargetLuma[index] = m_document.pipeline.params.colorWarpSourceLuma.at(index);
            }
        } else {
            m_colorWarper->setPinCreationMode(checked);
            refreshWarperSelectionUi();
        }
    });
    connect(warperSnap, &QPushButton::toggled, this, [this](bool enabled) {
        if (m_colorWarper) {
            m_colorWarper->setSnapEnabled(enabled);
        }
    });
    connect(m_warperPickImageButton, &QPushButton::toggled, this, [this](bool enabled) {
        setWarperImagePickMode(enabled);
    });
    connect(m_warperPointList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_colorWarper) {
            return;
        }
        if (row < 0 || row >= m_warperSelectedTargets.size()) {
            m_colorWarper->setSelectedPoint(-1);
            return;
        }
        m_colorWarper->setSelectedPoint(row);
    });
    connect(m_sourceView, &ImageView::imageSampled, this, [this](const QPoint& pixel, const QColor& color) {
        if (m_maskColorPickMode) {
            applyMaskColorSample(color);
            setStatus(QString("已为颜色范围采样输入像素 %1, %2").arg(pixel.x()).arg(pixel.y()));
        } else {
            tryAddWarperPointFromColor(color);
            setStatus(QString("已为色彩变形采样输入像素 %1, %2").arg(pixel.x()).arg(pixel.y()));
        }
    });
    auto updateSelectedSource = [this]() {
        if (!m_colorWarper || !m_warperSourceXControl || !m_warperSourceYControl || m_colorWarper->selectedPoint() < 0) {
            return;
        }
        m_colorWarper->setSelectedSourcePosition(QPointF(m_warperSourceXControl->value(),
                                                         m_warperSourceYControl->value()));
    };
    auto updateSelectedTarget = [this]() {
        if (!m_colorWarper || !m_warperTargetXControl || !m_warperTargetYControl || m_colorWarper->selectedPoint() < 0) {
            return;
        }
        m_colorWarper->setSelectedPointPosition(QPointF(m_warperTargetXControl->value(),
                                                        m_warperTargetYControl->value()));
    };
    auto updateSelectedSourceLuma = [this]() {
        if (!m_warperSourceLumaControl || m_warperSelectedIndex < 0 || m_warperSelectedIndex >= m_document.pipeline.params.colorWarpSourceLuma.size()) {
            return;
        }
        const float value = static_cast<float>(m_warperSourceLumaControl->value());
        m_document.pipeline.params.colorWarpSourceLuma[m_warperSelectedIndex] = value;
        if (m_warperSelectedIndex < m_document.pipeline.params.colorWarpPinned.size()
            && m_document.pipeline.params.colorWarpPinned.at(m_warperSelectedIndex)
            && m_warperSelectedIndex < m_document.pipeline.params.colorWarpTargetLuma.size()) {
            m_document.pipeline.params.colorWarpTargetLuma[m_warperSelectedIndex] = value;
        }
        refreshWarperSelectionUi();
        markDirty();
        schedulePreview();
    };
    auto updateSelectedTargetLuma = [this]() {
        if (!m_warperTargetLumaControl || m_warperSelectedIndex < 0 || m_warperSelectedIndex >= m_document.pipeline.params.colorWarpTargetLuma.size()) {
            return;
        }
        const float value = static_cast<float>(m_warperTargetLumaControl->value());
        m_document.pipeline.params.colorWarpTargetLuma[m_warperSelectedIndex] = value;
        if (m_warperSelectedIndex < m_document.pipeline.params.colorWarpPinned.size()
            && m_document.pipeline.params.colorWarpPinned.at(m_warperSelectedIndex)
            && m_warperSelectedIndex < m_document.pipeline.params.colorWarpSourceLuma.size()) {
            m_document.pipeline.params.colorWarpSourceLuma[m_warperSelectedIndex] = value;
        }
        refreshWarperSelectionUi();
        markDirty();
        schedulePreview();
    };
    connect(m_warperSourceXControl, &ParameterControl::valueChanged, this, updateSelectedSource);
    connect(m_warperSourceYControl, &ParameterControl::valueChanged, this, updateSelectedSource);
    connect(m_warperSourceLumaControl, &ParameterControl::valueChanged, this, updateSelectedSourceLuma);
    connect(m_warperTargetXControl, &ParameterControl::valueChanged, this, updateSelectedTarget);
    connect(m_warperTargetYControl, &ParameterControl::valueChanged, this, updateSelectedTarget);
    connect(m_warperTargetLumaControl, &ParameterControl::valueChanged, this, updateSelectedTargetLuma);
    connect(m_warperCoordinateMode, &QComboBox::currentIndexChanged, this, [this](int) {
        refreshWarperSelectionUi();
    });

    m_warperSelectedSources = m_document.pipeline.params.colorWarpSources;
    m_warperSelectedTargets = m_document.pipeline.params.colorWarpPoints;
    m_warperSelectedPinned = m_document.pipeline.params.colorWarpPinned;
    refreshWarperPointList();
    refreshWarperSelectionUi();
    return panel;
}

void MainWindow::refreshWarperPointList()
{
    if (!m_warperPointList) {
        return;
    }

    QSignalBlocker blocker(m_warperPointList);
    m_warperPointList->clear();
    if (m_warperSelectedSources.isEmpty() || m_warperSelectedTargets.isEmpty()) {
        auto* item = new QListWidgetItem("还没有色彩变形控制点", m_warperPointList);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
        return;
    }

    const int count = std::min(m_warperSelectedSources.size(), m_warperSelectedTargets.size());
    for (int i = 0; i < count; ++i) {
        const bool pinned = i < m_warperSelectedPinned.size() && m_warperSelectedPinned.at(i);
        const float sourceLuma = i < m_document.pipeline.params.colorWarpSourceLuma.size()
            ? m_document.pipeline.params.colorWarpSourceLuma.at(i)
            : 0.5f;
        const float targetLuma = i < m_document.pipeline.params.colorWarpTargetLuma.size()
            ? m_document.pipeline.params.colorWarpTargetLuma.at(i)
            : sourceLuma;
        auto* item = new QListWidgetItem(
            warperPointListLabel(i, m_warperSelectedSources.at(i), m_warperSelectedTargets.at(i), pinned)
                + QString("\n亮度 %1  ->  %2").arg(sourceLuma, 0, 'f', 3).arg(targetLuma, 0, 'f', 3),
            m_warperPointList);
        item->setData(Qt::UserRole, i);
    }
    if (m_warperSelectedIndex >= 0 && m_warperSelectedIndex < count) {
        m_warperPointList->setCurrentRow(m_warperSelectedIndex);
    }
}

void MainWindow::refreshWarperSelectionUi()
{
    const bool planeMode = m_warperCoordinateMode && m_warperCoordinateMode->currentIndex() == 1;
    const QString xLabel = planeMode ? "X" : "色相";
    const QString yLabel = planeMode ? "Y" : "饱和度";
    const double emptyY = planeMode ? 0.0 : 0.65;

    auto configurePair = [&](ParameterControl* xControl, ParameterControl* yControl) {
        if (xControl) {
            xControl->setLabelText(xLabel);
            xControl->setRange(0.0, 1.0, 0.001, 3, 0.0);
        }
        if (yControl) {
            yControl->setLabelText(yLabel);
            yControl->setRange(0.0, 1.0, 0.001, 3, emptyY);
        }
    };
    configurePair(m_warperSourceXControl, m_warperSourceYControl);
    configurePair(m_warperTargetXControl, m_warperTargetYControl);

    auto configureLuma = [](ParameterControl* control) {
        if (control) {
            control->setLabelText("亮度");
            control->setRange(0.0, 1.0, 0.001, 3, 0.5);
        }
    };
    configureLuma(m_warperSourceLumaControl);
    configureLuma(m_warperTargetLumaControl);

    const bool hasSelection = m_warperSelectedIndex >= 0
        && m_warperSelectedIndex < m_warperSelectedSources.size()
        && m_warperSelectedIndex < m_warperSelectedTargets.size();
    const QPointF source = hasSelection ? m_warperSelectedSources.at(m_warperSelectedIndex) : QPointF(0.0, emptyY);
    const QPointF target = hasSelection ? m_warperSelectedTargets.at(m_warperSelectedIndex) : QPointF(0.0, emptyY);
    const double sourceLuma = hasSelection && m_warperSelectedIndex < m_document.pipeline.params.colorWarpSourceLuma.size()
        ? m_document.pipeline.params.colorWarpSourceLuma.at(m_warperSelectedIndex)
        : 0.5;
    const double targetLuma = hasSelection && m_warperSelectedIndex < m_document.pipeline.params.colorWarpTargetLuma.size()
        ? m_document.pipeline.params.colorWarpTargetLuma.at(m_warperSelectedIndex)
        : sourceLuma;
    const bool pinned = hasSelection
        && m_warperSelectedIndex < m_warperSelectedPinned.size()
        && m_warperSelectedPinned.at(m_warperSelectedIndex);

    auto applyControl = [&](ParameterControl* control, bool enabled, double value) {
        if (!control) {
            return;
        }
        QSignalBlocker blocker(control);
        control->setEnabled(enabled);
        control->setValue(value);
    };
    applyControl(m_warperSourceXControl, hasSelection, source.x());
    applyControl(m_warperSourceYControl, hasSelection, source.y());
    applyControl(m_warperSourceLumaControl, hasSelection, sourceLuma);
    applyControl(m_warperTargetXControl, hasSelection, target.x());
    applyControl(m_warperTargetYControl, hasSelection, target.y());
    applyControl(m_warperTargetLumaControl, hasSelection, targetLuma);
    refreshWarperPointList();

    if (m_warperSelectionLabel) {
        if (hasSelection) {
            m_warperSelectionLabel->setText(pinned
                ? QString("控制点 %1（已锁定）").arg(m_warperSelectedIndex + 1)
                : QString("控制点 %1").arg(m_warperSelectedIndex + 1));
        } else if (m_warperImagePickMode) {
            m_warperSelectionLabel->setText("未选择点（点击输入图像添加）");
        } else if (m_colorWarper && m_colorWarper->pinCreationMode()) {
            m_warperSelectionLabel->setText("未选择点（锁定创建模式）");
        } else {
            m_warperSelectionLabel->setText("未选择点");
        }
    }

    if (m_warperDeleteButton) {
        m_warperDeleteButton->setEnabled(hasSelection);
    }
    if (m_warperPinButton) {
        QSignalBlocker blocker(m_warperPinButton);
        m_warperPinButton->setChecked(hasSelection
            ? pinned
            : (m_colorWarper && m_colorWarper->pinCreationMode()));
    }
    if (m_warperPickImageButton) {
        QSignalBlocker blocker(m_warperPickImageButton);
        m_warperPickImageButton->setChecked(m_warperImagePickMode);
    }
}

void MainWindow::setWarperImagePickMode(bool enabled)
{
    bool finalEnabled = enabled;
    if (finalEnabled && m_sourceImage.isNull()) {
        finalEnabled = false;
        setStatus("请先载入输入图像，再采样色彩变形控制点");
    }

    if (finalEnabled && m_maskColorPickMode) {
        setMaskColorPickMode(false);
    }
    m_warperImagePickMode = finalEnabled;
    if (m_sourceView) {
        m_sourceView->setPickMode(finalEnabled,
                                  finalEnabled ? "点击输入图像以创建色彩变形控制点" : QString());
    }
    refreshWarperSelectionUi();
}

void MainWindow::tryAddWarperPointFromColor(const QColor& color)
{
    if (!m_colorWarper) {
        return;
    }
    const QPointF point = warperPointFromColor(color);
    const float luma = warperLumaFromColor(color);
    const bool pinned = m_colorWarper->pinCreationMode();
    m_colorWarper->addWarpPoint(point, pinned);
    const int index = m_colorWarper->selectedPoint();
    if (index >= 0) {
        while (m_document.pipeline.params.colorWarpSourceLuma.size() <= index) {
            m_document.pipeline.params.colorWarpSourceLuma.append(0.5f);
        }
        while (m_document.pipeline.params.colorWarpTargetLuma.size() <= index) {
            m_document.pipeline.params.colorWarpTargetLuma.append(0.5f);
        }
        m_document.pipeline.params.colorWarpSourceLuma[index] = luma;
        m_document.pipeline.params.colorWarpTargetLuma[index] = luma;
        refreshWarperSelectionUi();
        markDirty();
        schedulePreview();
    }
}

void MainWindow::reconcileWarperLuma(const QVector<QPointF>& sources, const QVector<QPointF>& targets)
{
    QVector<float>& sourceLuma = m_document.pipeline.params.colorWarpSourceLuma;
    QVector<float>& targetLuma = m_document.pipeline.params.colorWarpTargetLuma;
    const int count = std::min(sources.size(), targets.size());

    auto normalize = [](QVector<float>& values, int expectedSize) {
        while (values.size() < expectedSize) {
            values.append(0.5f);
        }
        while (values.size() > expectedSize) {
            values.removeLast();
        }
        for (float& value : values) {
            value = std::clamp(value, 0.0f, 1.0f);
        }
    };

    if (sourceLuma.size() == count && targetLuma.size() == count) {
        normalize(sourceLuma, count);
        normalize(targetLuma, count);
        return;
    }

    const QVector<QPointF> previousSources = m_document.pipeline.params.colorWarpSources;
    const QVector<QPointF> previousTargets = m_document.pipeline.params.colorWarpPoints;
    const QVector<float> previousSourceLuma = sourceLuma;
    const QVector<float> previousTargetLuma = targetLuma;
    QVector<float> nextSourceLuma;
    QVector<float> nextTargetLuma;
    nextSourceLuma.reserve(count);
    nextTargetLuma.reserve(count);

    for (int i = 0; i < count; ++i) {
        int matched = -1;
        if (i < previousSources.size() && i < previousTargets.size()) {
            matched = i;
        }
        for (int old = 0; old < previousSources.size() && old < previousTargets.size(); ++old) {
            if (QLineF(sources.at(i), previousSources.at(old)).length() < 0.0001
                && QLineF(targets.at(i), previousTargets.at(old)).length() < 0.0001) {
                matched = old;
                break;
            }
        }
        nextSourceLuma.append(matched >= 0 && matched < previousSourceLuma.size() ? previousSourceLuma.at(matched) : 0.5f);
        nextTargetLuma.append(matched >= 0 && matched < previousTargetLuma.size() ? previousTargetLuma.at(matched) : nextSourceLuma.last());
    }
    sourceLuma = nextSourceLuma;
    targetLuma = nextTargetLuma;
    normalize(sourceLuma, count);
    normalize(targetLuma, count);
}

void MainWindow::refreshCurveHandleButtons()
{
    if (!m_curveAutoHandleButton || !m_curveVectorHandleButton || !m_curveWidget) {
        return;
    }
    const CurveHandleMode mode = m_curveWidget->selectedHandleMode();
    {
        QSignalBlocker blocker(m_curveAutoHandleButton);
        m_curveAutoHandleButton->setChecked(mode == CurveHandleMode::Auto);
    }
    {
        QSignalBlocker blocker(m_curveVectorHandleButton);
        m_curveVectorHandleButton->setChecked(mode == CurveHandleMode::Vector);
    }
}

QWidget* MainWindow::createHslPanel()
{
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(4, 4, 4, 4);

    auto* resetAll = new QPushButton("重置全部 HSL", content);
    contentLayout->addWidget(resetAll);
    connect(resetAll, &QPushButton::clicked, this, [this]() {
        resetAllHsl();
        refreshParameterWidgets();
        markDirty();
        schedulePreview();
    });

    auto* hueVsPanel = new CollapsiblePanel("色相关系曲线", content);
    auto addHueVsCurve = [&](int index, const QString& title, const QColor& color) {
        auto* label = new QLabel(title, hueVsPanel);
        label->setStyleSheet("font-weight: 700; color: #dfe3ea; padding-top: 4px;");
        hueVsPanel->contentLayout()->addWidget(label);
        auto* widget = new CurveWidget(hueVsPanel);
        widget->setMinimumHeight(136);
        widget->setMaximumHeight(180);
        widget->setNeutralLineY(0.5);
        m_hueVsCurveWidgets[static_cast<size_t>(index)] = widget;
        if (index == 0) {
            configureHueVsCurve(widget, m_document.pipeline.params.hueVsHueCurve, m_document.pipeline.params.hueVsHueCurveHandles, color);
        } else if (index == 1) {
            configureHueVsCurve(widget, m_document.pipeline.params.hueVsSaturationCurve, m_document.pipeline.params.hueVsSaturationCurveHandles, color);
        } else {
            configureHueVsCurve(widget, m_document.pipeline.params.hueVsLuminanceCurve, m_document.pipeline.params.hueVsLuminanceCurveHandles, color);
        }
        hueVsPanel->contentLayout()->addWidget(widget);

        auto* row = new QWidget(hueVsPanel);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->addStretch(1);
        auto* reset = new QPushButton("重置", row);
        configureToolButton(reset, "curveToolButton");
        rowLayout->addWidget(reset);
        hueVsPanel->contentLayout()->addWidget(row);

        connect(widget, &CurveWidget::curveChanged, this, [this, index](const QVector<QPointF>& curve) {
            if (index == 0) {
                m_document.pipeline.params.hueVsHueCurve = curve;
            } else if (index == 1) {
                m_document.pipeline.params.hueVsSaturationCurve = curve;
            } else {
                m_document.pipeline.params.hueVsLuminanceCurve = curve;
            }
            markDirty();
            schedulePreview();
        });
        connect(widget, &CurveWidget::handleModesChanged, this, [this, index](const QVector<CurveHandleMode>& handles) {
            if (index == 0) {
                m_document.pipeline.params.hueVsHueCurveHandles = handles;
            } else if (index == 1) {
                m_document.pipeline.params.hueVsSaturationCurveHandles = handles;
            } else {
                m_document.pipeline.params.hueVsLuminanceCurveHandles = handles;
            }
            markDirty();
            schedulePreview();
        });
        connect(reset, &QPushButton::clicked, this, [this, index]() {
            resetHueVsCurve(index);
            markDirty();
            schedulePreview();
        });
    };
    addHueVsCurve(0, "色相 -> 色相", QColor(236, 157, 73));
    addHueVsCurve(1, "色相 -> 饱和度", QColor(88, 204, 147));
    addHueVsCurve(2, "色相 -> 亮度", QColor(108, 168, 255));
    hueVsPanel->setExpanded(true);
    contentLayout->addWidget(hueVsPanel);

    auto addBand = [&](int band) {
        auto* box = new QGroupBox(kHslBands[band], content);
        auto* bandLayout = new QVBoxLayout(box);
        bandLayout->setContentsMargins(10, 14, 10, 10);
        bandLayout->setSpacing(6);
        auto addSlider = [&](const QString& label,
                             std::array<float, 8> ColorGradeParams::*member,
                             double min,
                             double max,
                             double step,
                             int decimals) {
            auto* control = new ParameterControl(label, min, max, 0.0, step, decimals, box);
            const int offset = label == "色相" ? 0 : (label == "饱和度" ? 8 : 16);
            m_hslControls[static_cast<size_t>(offset + band)] = control;
            connect(control, &ParameterControl::valueChanged, this, [this, band, member](double value) {
                (m_document.pipeline.params.*member)[static_cast<size_t>(band)] = static_cast<float>(value);
                markDirty();
                schedulePreview();
            });
            bandLayout->addWidget(control);
        };
        addSlider("色相", &ColorGradeParams::hslHue, -180.0, 180.0, 1.0, 0);
        addSlider("饱和度", &ColorGradeParams::hslSaturation, -1.0, 1.0, 0.01, 2);
        addSlider("亮度", &ColorGradeParams::hslLuminance, -1.0, 1.0, 0.01, 2);
        auto* resetBand = new QPushButton("重置此色段", box);
        connect(resetBand, &QPushButton::clicked, this, [this, band]() {
            resetHslBand(band);
            refreshParameterWidgets();
            markDirty();
            schedulePreview();
        });
        bandLayout->addWidget(resetBand);
        contentLayout->addWidget(box);
    };

    for (int i = 0; i < 8; ++i) {
        addBand(i);
    }
    contentLayout->addStretch(1);
    scroll->setWidget(content);
    layout->addWidget(scroll);
    return panel;
}

QWidget* MainWindow::createNodesPanel()
{
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* hint = new QLabel("从调整堆栈构建可执行的颜色节点图。串行图决定处理顺序，颜色工具节点可从节点菜单直接插入。", panel);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #9fa7b3;");
    layout->addWidget(hint);

    m_nodeGraphEnabledCheck = new QCheckBox("使用节点图调色", panel);
    layout->addWidget(m_nodeGraphEnabledCheck);

    auto* modeRow = new QWidget(panel);
    auto* modeLayout = new QHBoxLayout(modeRow);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(8);
    auto* modeLabel = new QLabel("布局", modeRow);
    modeLabel->setMinimumWidth(82);
    m_nodeGraphModeCombo = new NoWheelComboBox(modeRow);
    m_nodeGraphModeCombo->addItem("串行", "serial");
    m_nodeGraphModeCombo->addItem("并行", "parallel");
    m_nodeGraphModeCombo->addItem("图层混合器", "layerMixer");
    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(m_nodeGraphModeCombo, 1);
    layout->addWidget(modeRow);

    auto* buildButton = new QPushButton("从堆栈构建节点图", panel);
    configureToolButton(buildButton, "curveToolButton");
    layout->addWidget(buildButton);

    m_nodeGraphDiagnosticLabel = new QLabel(panel);
    m_nodeGraphDiagnosticLabel->setWordWrap(true);
    m_nodeGraphDiagnosticLabel->setStyleSheet("color: #9fa7b3; background: #171a20; border: 1px solid #2d3440; padding: 6px;");
    layout->addWidget(m_nodeGraphDiagnosticLabel);

    m_nodeGraphWidget = new NodeGraphWidget(panel);
    layout->addWidget(m_nodeGraphWidget, 1);

    m_nodePropertyPanel = new CollapsiblePanel("选中节点", panel);
    m_nodePropertyTitle = new QLabel("选择一个节点以编辑参数", m_nodePropertyPanel);
    m_nodePropertyTitle->setStyleSheet("color: #d9dde5; font-weight: 600;");
    m_nodePropertyContentLayout = static_cast<CollapsiblePanel*>(m_nodePropertyPanel)->contentLayout();
    m_nodePropertyContentLayout->addWidget(m_nodePropertyTitle);
    m_nodePropertyPanel->setVisible(false);
    layout->addWidget(m_nodePropertyPanel);

    connect(m_nodeGraphEnabledCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        m_document.nodeGraph.enabled = enabled;
        refreshNodeGraphUi();
        markDirty();
        schedulePreview();
    });
    connect(buildButton, &QPushButton::clicked, this, [this]() {
        const QString mode = m_nodeGraphModeCombo ? m_nodeGraphModeCombo->currentData().toString() : QString("serial");
        rebuildNodeGraphFromPipeline(mode.isEmpty() ? QString("serial") : mode);
        markDirty();
    });
    connect(m_nodeGraphWidget, &NodeGraphWidget::graphEdited, this, [this](const NodeGraph& graph) {
        m_document.nodeGraph = graph;
        QSet<QString> graphNodeIds;
        for (const NodeGraphNode& node : m_document.nodeGraph.nodes) {
            graphNodeIds.insert(node.nodeId);
        }
        if (!m_selectedNodeGraphNodeId.isEmpty() && !graphNodeIds.contains(m_selectedNodeGraphNodeId)) {
            m_selectedNodeGraphNodeId.clear();
        }
        m_document.pipeline.stages.erase(std::remove_if(m_document.pipeline.stages.begin(), m_document.pipeline.stages.end(), [&](const ColorGradeStage& stage) {
                                             return !graphNodeIds.contains(stage.stageId);
                                         }),
                                         m_document.pipeline.stages.end());
        if (m_document.pipeline.stages.isEmpty()) {
            rebuildDefaultStages();
            rebuildNodeGraphFromPipeline(m_nodeGraphModeCombo ? m_nodeGraphModeCombo->currentData().toString() : QString("serial"));
        }
        m_activeStageIndex = std::clamp(m_activeStageIndex, 0, std::max(0, static_cast<int>(m_document.pipeline.stages.size()) - 1));
        loadActiveStageParams(m_activeStageIndex);
        refreshParameterWidgets();
        refreshNodeGraphUi();
        markDirty();
        schedulePreview();
    });
    connect(m_nodeGraphWidget, &NodeGraphWidget::nodeSelected, this, [this](const QString& nodeId) {
        m_selectedNodeGraphNodeId = nodeId;
        selectStageById(nodeId);
        refreshNodePropertyUi();
    });
    connect(m_nodeGraphWidget, &NodeGraphWidget::addNodeRequested, this, [this](const QString& type, const QPointF& position, const QString& insertAfterNodeId) {
        addNodeGraphStage(type, position, insertAfterNodeId);
    });
    connect(m_nodeGraphWidget, &NodeGraphWidget::cloneNodeRequested, this, [this](const NodeGraphNode& sourceNode, const QPointF& position) {
        cloneNodeGraphStage(sourceNode, position);
    });

    refreshNodeGraphUi();
    return panel;
}

QWidget* MainWindow::createProfessionalPanel()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* panel = new QWidget(scroll);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto add = [&](QVBoxLayout* targetLayout,
                   QWidget* parent,
                   ParameterControl** storage,
                   const QString& label,
                   double min,
                   double max,
                   double value,
                   double step,
                   int decimals,
                   auto setter) {
        auto* control = new ParameterControl(label, min, max, value, step, decimals, parent);
        if (storage) {
            *storage = control;
        }
        connect(control, &ParameterControl::valueChanged, this, [this, setter](double v) {
            setter(v);
            markDirty();
            schedulePreview();
        });
        targetLayout->addWidget(control);
    };

    auto* logPanel = new CollapsiblePanel("Log 色轮", panel);
    auto* logHint = new QLabel("现代明度分区：阴影、暗部、亮部、高光。这些调整按亮度加权，并可干净导出到 LUT。", logPanel);
    logHint->setWordWrap(true);
    logHint->setStyleSheet("color: #9fa7b3;");
    logPanel->contentLayout()->addWidget(logHint);
    add(logPanel->contentLayout(), logPanel, &m_logShadowControl, "阴影", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.logShadow = static_cast<float>(v); });
    add(logPanel->contentLayout(), logPanel, &m_logDarkControl, "暗部", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.logDark = static_cast<float>(v); });
    add(logPanel->contentLayout(), logPanel, &m_logLightControl, "亮部", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.logLight = static_cast<float>(v); });
    add(logPanel->contentLayout(), logPanel, &m_logHighlightControl, "高光", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.logHighlight = static_cast<float>(v); });
    layout->addWidget(logPanel);

    auto* hdrPanel = new CollapsiblePanel("HDR 色轮", panel);
    add(hdrPanel->contentLayout(), hdrPanel, &m_hdrShadowControl, "阴影", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.hdrShadow = static_cast<float>(v); });
    add(hdrPanel->contentLayout(), hdrPanel, &m_hdrDarkControl, "暗部", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.hdrDark = static_cast<float>(v); });
    add(hdrPanel->contentLayout(), hdrPanel, &m_hdrLightControl, "亮部", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.hdrLight = static_cast<float>(v); });
    add(hdrPanel->contentLayout(), hdrPanel, &m_hdrHighlightControl, "高光", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.hdrHighlight = static_cast<float>(v); });
    hdrPanel->setExpanded(false);
    layout->addWidget(hdrPanel);

    auto* printerPanel = new CollapsiblePanel("印片灯", panel);
    auto* printerHint = new QLabel("RGB 印片灯点数。约 1 点等于 0.025 的通道偏移，适合严谨地做色彩平衡。", printerPanel);
    printerHint->setWordWrap(true);
    printerHint->setStyleSheet("color: #9fa7b3;");
    printerPanel->contentLayout()->addWidget(printerHint);
    add(printerPanel->contentLayout(), printerPanel, &m_printerRedControl, "红", -12.0, 12.0, 0.0, 1.0, 0, [this](double v) { m_document.pipeline.params.printerRed = static_cast<float>(v); });
    add(printerPanel->contentLayout(), printerPanel, &m_printerGreenControl, "绿", -12.0, 12.0, 0.0, 1.0, 0, [this](double v) { m_document.pipeline.params.printerGreen = static_cast<float>(v); });
    add(printerPanel->contentLayout(), printerPanel, &m_printerBlueControl, "蓝", -12.0, 12.0, 0.0, 1.0, 0, [this](double v) { m_document.pipeline.params.printerBlue = static_cast<float>(v); });
    printerPanel->setExpanded(false);
    layout->addWidget(printerPanel);

    auto* skinPanel = new CollapsiblePanel("肤色工具", panel);
    auto* skinHint = new QLabel("针对常见肤色色相，同时保护画面其余区域。建议配合矢量示波器肤色线使用。", skinPanel);
    skinHint->setWordWrap(true);
    skinHint->setStyleSheet("color: #9fa7b3;");
    skinPanel->contentLayout()->addWidget(skinHint);
    add(skinPanel->contentLayout(), skinPanel, &m_skinHueControl, "色相", -30.0, 30.0, 0.0, 1.0, 0, [this](double v) { m_document.pipeline.params.skinToneHue = static_cast<float>(v); });
    add(skinPanel->contentLayout(), skinPanel, &m_skinSaturationControl, "饱和度", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.skinToneSaturation = static_cast<float>(v); });
    add(skinPanel->contentLayout(), skinPanel, &m_skinLuminanceControl, "亮度", -1.0, 1.0, 0.0, 0.01, 2, [this](double v) { m_document.pipeline.params.skinToneLuminance = static_cast<float>(v); });
    layout->addWidget(skinPanel);

    layout->addStretch(1);
    scroll->setWidget(panel);
    return scroll;
}

void MainWindow::openImage()
{
    const QString path = QFileDialog::getOpenFileName(this, "打开图片", QString(), "图片 (*.png *.jpg *.jpeg *.tif *.tiff)");
    if (path.isEmpty()) {
        return;
    }

    updateBusy(true, "正在载入图片...");
    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, path]() {
        watcher->deleteLater();
        const QImage image = watcher->result();
        if (image.isNull()) {
            updateBusy(false, "图片载入失败");
            QMessageBox::warning(this, "打开图片", "无法载入图片。");
            return;
        }
        m_document.imagePath = path;
        m_sourceImage = image;
        m_cachedBeforeScaled = QImage();
        m_cachedMaskPreview = QImage();
        m_sourceView->setImage(m_sourceImage);
        if (m_warperImagePickMode) {
            m_sourceView->setPickMode(true, "点击输入图像以创建色彩变形控制点");
        }
        updateBusy(false, "图片已载入");
        markDirty();
        resetUndoHistory();
        schedulePreview();
    });
    watcher->setFuture(QtConcurrent::run([path]() {
        return QImage(path);
    }));
}

void MainWindow::importLut()
{
    const QString path = QFileDialog::getOpenFileName(this, "导入 LUT", QString(), "LUT 文件 (*.cube *.png)");
    if (path.isEmpty()) {
        return;
    }

    updateBusy(true, "正在导入 LUT...");
    auto* watcher = new QFutureWatcher<LutImportResult>(this);
    connect(watcher, &QFutureWatcher<LutImportResult>::finished, this, [this, watcher, path]() {
        watcher->deleteLater();
        const LutImportResult result = watcher->result();
        if (!result.ok) {
            updateBusy(false, "LUT 导入失败");
            QMessageBox::warning(this, "导入 LUT", result.error);
            return;
        }
        finalizeUndoCheckpoint();
        m_importedLut = result.lut;
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_document.pipeline.params.activeImportedLutAssetId = id;
        m_document.pipeline.params.importedLutStrength = 1.0f;
        syncTransformSource();
        m_document.importedLuts.append(ImportedLutAsset{id, path, result.format, result.lut.size(), true, result.metadata});
        refreshParameterWidgets();
        refreshLutViewerAsync();
        updateBusy(false, "LUT 已导入");
        markDirty();
        schedulePreview();
    });
    watcher->setFuture(QtConcurrent::run([path]() {
        return LutImportService::importFile(path);
    }));
}

void MainWindow::clearImportedLut()
{
    if (!m_importedLut.isValid()
        && m_document.pipeline.params.activeImportedLutAssetId.isEmpty()
        && m_document.pipeline.params.importedLutStrength <= 0.0f
        && m_document.importedLuts.isEmpty()) {
        setStatus("没有可移除的导入 LUT");
        return;
    }

    finalizeUndoCheckpoint();
    m_importedLut = Lut3D();
    m_document.importedLuts.clear();
    m_document.pipeline.params.activeImportedLutAssetId.clear();
    m_document.pipeline.params.importedLutStrength = 0.0f;
    syncTransformSource();
    refreshParameterWidgets();
    refreshLutViewerAsync();
    markDirty();
    schedulePreview();
    setStatus("导入 LUT 已移除");
}

void MainWindow::analyzeLutQuality()
{
    syncTransformSource();
    const ColorTransformSource source = m_transformSource;
    const ColorGradeParams params = m_document.pipeline.params;
    const Lut3D imported = m_importedLut;
    updateBusy(true, "正在分析 LUT 质量...");

    auto* watcher = new QFutureWatcher<LutQualityReport>(this);
    connect(watcher, &QFutureWatcher<LutQualityReport>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        const LutQualityReport report = watcher->result();
        updateBusy(false, report.warningCount == 0 ? "LUT 质量检查通过" : "LUT 质量检查发现警告");
        const QMessageBox::Icon icon = report.warningCount == 0 ? QMessageBox::Information : QMessageBox::Warning;
        QMessageBox message(icon, "LUT 质量检查", report.summaryText(), QMessageBox::Ok, this);
        message.exec();
    });
    watcher->setFuture(QtConcurrent::run([source, params, imported]() {
        const Lut3D lut = IkClutExporter::resolveExportLut(source, params, imported);
        return LutQualityAnalyzer::analyze(lut);
    }));
}

void MainWindow::exportIkClut()
{
    const QString path = QFileDialog::getSaveFileName(this, "导出 ikClut PNG", "clut.png", "PNG 图片 (*.png)");
    if (path.isEmpty()) {
        return;
    }

    warnIfExportOmitsSpatialMasks();
    syncTransformSource();
    if (!showExportQualityReport("ikClut PNG", path)) {
        return;
    }
    const ColorTransformSource source = m_transformSource;
    const ColorGradeParams params = m_document.pipeline.params;
    const Lut3D imported = m_importedLut;
    updateBusy(true, "正在导出 ikClut...");
    auto* watcher = new QFutureWatcher<ExportResult>(this);
    connect(watcher, &QFutureWatcher<ExportResult>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        const ExportResult result = watcher->result();
        if (!result.ok) {
            updateBusy(false, "导出失败");
            QMessageBox::warning(this, "导出 ikClut", result.error);
            return;
        }
        updateBusy(false, QString("已导出 %1x%2 ikClut PNG").arg(result.imageSize.width()).arg(result.imageSize.height()));
    });
    watcher->setFuture(QtConcurrent::run([source, params, imported, path]() {
        const Lut3D lut = IkClutExporter::resolveExportLut(source, params, imported);
        return IkClutExporter::savePng(lut, path);
    }));
}

void MainWindow::exportCube()
{
    const QString path = QFileDialog::getSaveFileName(this, "导出 CUBE LUT", "look.cube", "CUBE LUT (*.cube)");
    if (path.isEmpty()) {
        return;
    }

    warnIfExportOmitsSpatialMasks();
    syncTransformSource();
    if (!showExportQualityReport("CUBE", path)) {
        return;
    }
    const ColorTransformSource source = m_transformSource;
    const ColorGradeParams params = m_document.pipeline.params;
    const Lut3D imported = m_importedLut;
    updateBusy(true, "正在导出 CUBE...");
    auto* watcher = new QFutureWatcher<ExportResult>(this);
    connect(watcher, &QFutureWatcher<ExportResult>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        const ExportResult result = watcher->result();
        if (!result.ok) {
            updateBusy(false, "CUBE 导出失败");
            QMessageBox::warning(this, "导出 CUBE", result.error);
            return;
        }
        updateBusy(false, result.diagnostic.isEmpty() ? "CUBE 已导出" : result.diagnostic);
    });
    watcher->setFuture(QtConcurrent::run([source, params, imported, path]() {
        const Lut3D lut = IkClutExporter::resolveExportLut(source, params, imported);
        return IkClutExporter::saveCube(lut, path);
    }));
}

void MainWindow::exportHald()
{
    const QString path = QFileDialog::getSaveFileName(this, "导出 Hald PNG", "hald.png", "PNG 图片 (*.png)");
    if (path.isEmpty()) {
        return;
    }

    warnIfExportOmitsSpatialMasks();
    syncTransformSource();
    if (!showExportQualityReport("Hald PNG", path)) {
        return;
    }
    const ColorTransformSource source = m_transformSource;
    const ColorGradeParams params = m_document.pipeline.params;
    const Lut3D imported = m_importedLut;
    updateBusy(true, "正在导出 Hald PNG...");
    auto* watcher = new QFutureWatcher<ExportResult>(this);
    connect(watcher, &QFutureWatcher<ExportResult>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        const ExportResult result = watcher->result();
        if (!result.ok) {
            updateBusy(false, "Hald 导出失败");
            QMessageBox::warning(this, "导出 Hald PNG", result.error);
            return;
        }
        updateBusy(false, result.diagnostic.isEmpty() ? "Hald PNG 已导出" : result.diagnostic);
    });
    watcher->setFuture(QtConcurrent::run([source, params, imported, path]() {
        const Lut3D lut = IkClutExporter::resolveExportLut(source, params, imported);
        return IkClutExporter::saveHaldPng(lut, path);
    }));
}

void MainWindow::batchExportImages()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, "批量套用当前调色", QString(), "图片 (*.png *.jpg *.jpeg *.tif *.tiff)");
    if (paths.isEmpty()) {
        return;
    }
    const QString outputDir = QFileDialog::getExistingDirectory(this, "选择批量导出文件夹");
    if (outputDir.isEmpty()) {
        return;
    }
    syncTransformSource();
    if (!showExportQualityReport("Batch Image Export", outputDir)) {
        return;
    }
    const ColorTransformSource source = m_transformSource;
    const Lut3D imported = m_importedLut;
    updateBusy(true, "正在批量导出图片...");
    auto* watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        const QStringList errors = watcher->result();
        if (!errors.isEmpty()) {
            updateBusy(false, QString("批量导出完成，但有 %1 个错误").arg(errors.size()));
            QMessageBox::warning(this, "批量导出", errors.join("\n"));
            return;
        }
        updateBusy(false, "批量导出完成");
    });
    watcher->setFuture(QtConcurrent::run([paths, outputDir, source, imported]() {
        QStringList errors;
        int index = 1;
        for (const QString& path : paths) {
            const QImage image(path);
            if (image.isNull()) {
                errors.append(QString("无法读取 %1").arg(path));
                continue;
            }
            QImage out(image.size(), QImage::Format_RGBA8888);
            const QImage src = image.convertToFormat(QImage::Format_RGBA8888);
            const int width = std::max(1, src.width() - 1);
            const int height = std::max(1, src.height() - 1);
            for (int y = 0; y < src.height(); ++y) {
                const QRgb* inLine = reinterpret_cast<const QRgb*>(src.constScanLine(y));
                QRgb* outLine = reinterpret_cast<QRgb*>(out.scanLine(y));
                for (int x = 0; x < src.width(); ++x) {
                    const QRgb p = inLine[x];
                    const QVector3D raw(qRed(p) / 255.0f, qGreen(p) / 255.0f, qBlue(p) / 255.0f);
                    QVector3D c = source.usePipeline
                        ? ColorPipeline::applyPipeline(raw, source.pipeline, source.masks, QPointF(x / static_cast<double>(width), y / static_cast<double>(height)), &imported)
                        : ColorPipeline::applyParams(raw, source.params);
                    if (source.kind == ColorTransformKind::ImportedLut && imported.isValid() && source.importedLutStrength > 0.0f) {
                        const float strength = std::clamp(source.importedLutStrength, 0.0f, 1.0f);
                        const QVector3D lutted = imported.sample(c);
                        c = c * (1.0f - strength) + lutted * strength;
                    }
                    outLine[x] = qRgba(static_cast<int>(std::clamp(c.x(), 0.0f, 1.0f) * 255.0f + 0.5f),
                                       static_cast<int>(std::clamp(c.y(), 0.0f, 1.0f) * 255.0f + 0.5f),
                                       static_cast<int>(std::clamp(c.z(), 0.0f, 1.0f) * 255.0f + 0.5f),
                                       qAlpha(p));
                }
            }
            const QFileInfo info(path);
            const QString outPath = QDir(outputDir).filePath(QString("%1_%2.png").arg(info.completeBaseName()).arg(index++, 3, 10, QLatin1Char('0')));
            if (!out.save(outPath)) {
                errors.append(QString("无法写入 %1").arg(outPath));
            }
        }
        return errors;
    }));
}

void MainWindow::saveProject()
{
    const QString initialPath = m_document.projectPath.isEmpty() ? "project.ikclutproj" : m_document.projectPath;
    const QString path = QFileDialog::getSaveFileName(this, "保存项目", initialPath, "IKCLUT 项目 (*.ikclutproj)");
    if (path.isEmpty()) {
        return;
    }
    saveProjectPath(path);
}

void MainWindow::openProject()
{
    if (!maybeSaveChanges()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, "打开项目", QString(), "IKCLUT 项目 (*.ikclutproj)");
    if (path.isEmpty()) {
        return;
    }
    openProjectPath(path);
}

void MainWindow::savePreset()
{
    const QString path = QFileDialog::getSaveFileName(this, "保存预设", "preset.ikclutpreset", "IKCLUT 预设 (*.ikclutpreset)");
    if (path.isEmpty()) {
        return;
    }
    syncStageParams();
    QString error;
    if (!ProjectSerializer::savePreset(m_document.pipeline, path, &error)) {
        QMessageBox::warning(this, "保存预设", error);
        return;
    }
    setStatus("预设已保存");
}

void MainWindow::loadPreset()
{
    const QString path = QFileDialog::getOpenFileName(this, "载入预设", QString(), "IKCLUT 预设 (*.ikclutpreset)");
    if (path.isEmpty()) {
        return;
    }
    ColorGradePipeline pipeline;
    QString error;
    if (!ProjectSerializer::loadPreset(path, &pipeline, &error)) {
        QMessageBox::warning(this, "载入预设", error);
        return;
    }
    m_document.pipeline = pipeline;
    m_activeStageIndex = 0;
    loadActiveStageParams(0);
    m_transformSource.kind = ColorTransformKind::ParamPipeline;
    m_transformSource.params = m_document.pipeline.params;
    refreshParameterWidgets();
    refreshNodeGraphUi();
    markDirty();
    resetUndoHistory();
    schedulePreview();
}

void MainWindow::schedulePreview()
{
    syncActiveStageParams();
    syncTransformSource();
    ++m_lutGeneration;
    const bool interactiveMaskView = m_maskViewEnabled || m_maskOverlayEnabled || m_maskEditMode;
    if (interactiveMaskView) {
        refreshMaskOverlay();
    }
    if (!m_maskEditMode && !m_maskColorPickMode) {
        m_lutDebounce.start();
    }
    m_previewDebounce.start();
    m_fullPreviewDebounce.start();
}

void MainWindow::syncTransformSource()
{
    syncActiveStageParams();
    syncNodeGraphFromPipelineStages();
    stripLocalMasksFromDocument();
    m_transformSource.params = m_document.pipeline.params;
    m_transformSource.pipeline = pipelineFromNodeGraph();
    m_transformSource.pipeline.params = m_document.pipeline.params;
    m_transformSource.masks.clear();
    m_transformSource.usePipeline = true;
    m_transformSource.importedLutAssetId = m_document.pipeline.params.activeImportedLutAssetId;
    m_transformSource.importedLutStrength = std::clamp(m_document.pipeline.params.importedLutStrength, 0.0f, 1.0f);
    if (m_adjustmentsBypassed) {
        ColorGradeParams baseParams;
        baseParams.inputColorSpace = m_document.pipeline.params.inputColorSpace;
        m_transformSource.params = baseParams;
        m_transformSource.pipeline = ColorGradePipeline();
        m_transformSource.pipeline.params = baseParams;
        m_transformSource.masks.clear();
        m_transformSource.importedLutStrength = 0.0f;
        m_transformSource.usePipeline = true;
    }
    m_transformSource.kind = m_importedLut.isValid() && m_transformSource.importedLutStrength > 0.0f
        ? ColorTransformKind::ImportedLut
        : ColorTransformKind::ParamPipeline;
}

void MainWindow::stripLocalMasksFromDocument()
{
    for (ColorGradeStage& stage : m_document.pipeline.stages) {
        stage.maskRef = MaskReference();
    }
    m_document.masks.clear();
    m_maskOverlayEnabled = false;
    m_maskViewEnabled = false;
    m_maskEditMode = false;
    m_maskColorPickMode = false;
    m_maskSubtractSample = false;
    m_cachedMaskPreview = QImage();
    m_cachedMaskPreviewSize = QSize();
    m_cachedMaskPreviewKey.clear();
    if (m_resultView) {
        m_resultView->setMaskEditMode(false);
        m_resultView->setMaskGuide(MaskKind::Full, QJsonObject(), false);
        m_resultView->setMaskOverlay(QImage());
    }
}

void MainWindow::renderPreview(bool fullQuality)
{
    if (m_sourceImage.isNull()) {
        return;
    }

    const quint64 generation = fullQuality ? ++m_fullPreviewGeneration : ++m_previewGeneration;
    const QImage source = m_sourceImage;
    const ColorTransformSource transform = m_transformSource;
    const Lut3D imported = m_importedLut;
    const QVector<MaskAsset> masks = m_document.masks;
    const MaskReference maskRef = activeMaskReference();
    const PreviewQuality quality = fullQuality ? PreviewQuality::Full : PreviewQuality::Interactive;
    const bool preferGpu = m_preferGpuPreview;
    if (fullQuality) {
        updateBusy(true, "正在渲染完整预览...");
    } else {
        setStatus("正在渲染交互预览...");
    }

    auto* watcher = new QFutureWatcher<PreviewRenderResult>(this);
    connect(watcher, &QFutureWatcher<PreviewRenderResult>::finished, this, [this, watcher, generation, fullQuality]() {
        watcher->deleteLater();
        if (fullQuality) {
            if (generation != m_fullPreviewGeneration) {
                return;
            }
        } else if (generation != m_previewGeneration) {
            return;
        }

        const PreviewRenderResult result = watcher->result();
        m_previewImage = result.image;
        const QString qualityText = result.quality == PreviewQuality::Full ? "完整" : "交互";
        const QString backend = result.usedGpu ? "GPU" : "CPU";
        m_resultView->setOverlayInfo(QString("%1 | %2 | %3 ms").arg(qualityText, backend).arg(result.elapsedMs));
        updatePreviewDisplay();
        if (result.quality == PreviewQuality::Full) {
            updateBusy(false, QString("完整预览已完成（%1，LUT %2 ms，渲染 %3 ms）%4").arg(backend).arg(result.lutMs).arg(result.renderMs).arg(result.diagnostic));
        } else {
            setStatus(QString("交互预览已完成（%1，LUT %2 ms，渲染 %3 ms）%4").arg(backend).arg(result.lutMs).arg(result.renderMs).arg(result.diagnostic));
        }
        if (result.quality == PreviewQuality::Full) {
            if (result.exportLut.isValid()) {
                m_currentPreviewLut = result.exportLut;
                if (m_transformSource.kind == ColorTransformKind::ParamPipeline) {
                    m_lutViewer->setLut(result.previewLut.isValid() ? result.previewLut : result.exportLut);
                    updateIkClutStrip(result.exportLut);
                }
            }
            refreshMaskOverlay();
            scheduleScopes(m_previewImage);
        } else {
            refreshMaskOverlay();
        }
    });
    watcher->setFuture(QtConcurrent::run([source, transform, imported, masks, maskRef, quality, preferGpu]() {
        PreviewRenderRequest request;
        request.source = source;
        request.transform = transform;
        request.importedLut = imported;
        request.masks = masks;
        request.maskRef = maskRef;
        request.quality = quality;
        request.maxInteractiveSize = QSize(1280, 720);
        request.preferGpu = preferGpu;
        return PreviewRenderer::render(request);
    }));
}

void MainWindow::updatePreviewDisplay()
{
    if (!m_resultView) {
        return;
    }
    const int mode = m_previewModeCombo ? m_previewModeCombo->currentIndex() : 0;
    if (mode == 1 && !m_sourceImage.isNull()) {
        const QSize targetSize = m_previewImage.isNull() ? m_sourceImage.size() : m_previewImage.size();
        if (m_cachedBeforeScaled.isNull() || m_cachedBeforeScaledSize != targetSize) {
            m_cachedBeforeScaled = m_sourceImage.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
            m_cachedBeforeScaledSize = targetSize;
        }
        const QImage before = m_cachedBeforeScaled;
        m_resultView->setImage(before);
        m_resultView->setOverlayInfo("调色前");
    } else if (mode == 2 && !m_previewImage.isNull()) {
        m_resultView->setImage(makeSplitComparisonImage(m_previewImage));
        m_resultView->setOverlayInfo("前后对比");
    } else if (mode == 3 && !m_snapshotA.isNull()) {
        m_resultView->setImage(m_snapshotA);
        m_resultView->setOverlayInfo("快照 A");
    } else if (mode == 4 && !m_snapshotB.isNull()) {
        m_resultView->setImage(m_snapshotB);
        m_resultView->setOverlayInfo("快照 B");
    } else {
        m_resultView->setImage(m_previewImage);
    }
}

QImage MainWindow::makeSplitComparisonImage(const QImage& after)
{
    if (after.isNull() || m_sourceImage.isNull()) {
        return after;
    }
    if (m_cachedBeforeScaled.isNull() || m_cachedBeforeScaledSize != after.size()) {
        m_cachedBeforeScaled = m_sourceImage.scaled(after.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
        m_cachedBeforeScaledSize = after.size();
    }
    QImage before = m_cachedBeforeScaled.convertToFormat(QImage::Format_RGBA8888);
    QImage result = after.convertToFormat(QImage::Format_RGBA8888);
    const int split = result.width() / 2;
    for (int y = 0; y < result.height(); ++y) {
        const QRgb* beforeLine = reinterpret_cast<const QRgb*>(before.constScanLine(y));
        QRgb* outLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < split; ++x) {
            outLine[x] = beforeLine[x];
        }
    }
    QPainter painter(&result);
    painter.setPen(QPen(QColor(255, 255, 255, 220), 2.0));
    painter.drawLine(split, 0, split, result.height());
    painter.setFont(QFont("Segoe UI", 9, QFont::DemiBold));
    painter.setPen(QColor(235, 238, 244));
    painter.drawText(QRect(10, 10, split - 20, 22), Qt::AlignLeft | Qt::AlignVCenter, "调色前");
    painter.drawText(QRect(split + 10, 10, result.width() - split - 20, 22), Qt::AlignRight | Qt::AlignVCenter, "调色后");
    return result;
}

bool MainWindow::documentHasSpatialMasks() const
{
    for (const ColorGradeStage& stage : m_document.pipeline.stages) {
        if (stage.maskRef.id.isEmpty()) {
            continue;
        }
        const auto it = std::find_if(m_document.masks.begin(), m_document.masks.end(), [&](const MaskAsset& mask) {
            return mask.maskId == stage.maskRef.id;
        });
        if (it == m_document.masks.end() || !it->enabled) {
            continue;
        }
        if (it->kind == MaskKind::Gradient || it->kind == MaskKind::Radial || it->kind == MaskKind::Paint || it->kind == MaskKind::ExternalImage) {
            return true;
        }
    }
    return false;
}

void MainWindow::warnIfExportOmitsSpatialMasks()
{
    if (!documentHasSpatialMasks()) {
        return;
    }
    QMessageBox::information(this,
                             "导出 LUT",
                             "径向、渐变、绘制和图片遮罩属于空间调整，无法表达在 .cube 或 LUT PNG 导出中。它们仍会保留在项目预览里。颜色范围遮罩仍会烘焙进 LUT。");
}

bool MainWindow::showExportQualityReport(const QString& exportFormat, const QString& outputPath)
{
    syncStageParams();
    const Lut3D lut = currentLutForExport();
    LutQualityReport report = LutQualityAnalyzer::analyzeExport(lut,
                                                                m_document,
                                                                m_importedLut.isValid() && m_document.pipeline.params.importedLutStrength > 0.0f,
                                                                exportFormat,
                                                                outputPath);

    QMessageBox message(report.warningCount == 0 ? QMessageBox::Information : QMessageBox::Warning,
                        "导出质量报告",
                        report.summaryText(),
                        QMessageBox::Cancel,
                        this);
    QPushButton* exportButton = message.addButton("继续导出", QMessageBox::AcceptRole);
    QPushButton* saveReportButton = message.addButton("保存报告...", QMessageBox::ActionRole);
    message.setDefaultButton(exportButton);
    message.exec();
    if (message.clickedButton() == saveReportButton) {
        const QString reportPath = QFileDialog::getSaveFileName(this, "保存导出质量报告", "export-quality-report.txt", "文本文件 (*.txt)");
        if (!reportPath.isEmpty()) {
            QFile file(reportPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                QTextStream out(&file);
                out << report.summaryText();
                setStatus("质量报告已保存");
            } else {
                QMessageBox::warning(this, "保存报告", file.errorString());
            }
        }
        return showExportQualityReport(exportFormat, outputPath);
    }
    return message.clickedButton() == exportButton;
}

void MainWindow::setActiveCurveChannel(int index)
{
    if (!m_curveWidget) {
        return;
    }
    if (m_curveMasterButton && m_curveRedButton && m_curveGreenButton && m_curveBlueButton) {
        QAbstractButton* active = m_curveMasterButton;
        if (index == 1) active = m_curveRedButton;
        else if (index == 2) active = m_curveGreenButton;
        else if (index == 3) active = m_curveBlueButton;
        if (active) {
            active->setChecked(true);
        }
    }
    m_currentCurveChannel = std::clamp(index, 0, 3);
    QSignalBlocker blocker(m_curveWidget);
    m_curveWidget->setCurve(curveForChannel(m_currentCurveChannel));
    m_curveWidget->setHandleModes(curveHandlesForChannel(m_currentCurveChannel));
    if (m_currentCurveChannel == 1) {
        m_curveWidget->setChannelColor(QColor(244, 89, 89));
    } else if (m_currentCurveChannel == 2) {
        m_curveWidget->setChannelColor(QColor(112, 211, 118));
    } else if (m_currentCurveChannel == 3) {
        m_curveWidget->setChannelColor(QColor(92, 166, 255));
    } else {
        m_curveWidget->setChannelColor(QColor(225, 225, 225));
    }
    refreshCurveHandleButtons();
    refreshCurvePointTable();
}

void MainWindow::setCurveForChannel(int index, const QVector<QPointF>& curve)
{
    switch (index) {
    case 1:
        m_document.pipeline.params.redCurve = curve;
        break;
    case 2:
        m_document.pipeline.params.greenCurve = curve;
        break;
    case 3:
        m_document.pipeline.params.blueCurve = curve;
        break;
    default:
        m_document.pipeline.params.masterCurve = curve;
        break;
    }
}

void MainWindow::setCurveHandlesForChannel(int index, const QVector<CurveHandleMode>& handles)
{
    switch (index) {
    case 1:
        m_document.pipeline.params.redCurveHandles = handles;
        break;
    case 2:
        m_document.pipeline.params.greenCurveHandles = handles;
        break;
    case 3:
        m_document.pipeline.params.blueCurveHandles = handles;
        break;
    default:
        m_document.pipeline.params.masterCurveHandles = handles;
        break;
    }
}

QVector<QPointF> MainWindow::curveForChannel(int index) const
{
    switch (index) {
    case 1:
        return m_document.pipeline.params.redCurve;
    case 2:
        return m_document.pipeline.params.greenCurve;
    case 3:
        return m_document.pipeline.params.blueCurve;
    default:
        return m_document.pipeline.params.masterCurve;
    }
}

QVector<CurveHandleMode> MainWindow::curveHandlesForChannel(int index) const
{
    switch (index) {
    case 1:
        return m_document.pipeline.params.redCurveHandles;
    case 2:
        return m_document.pipeline.params.greenCurveHandles;
    case 3:
        return m_document.pipeline.params.blueCurveHandles;
    default:
        return m_document.pipeline.params.masterCurveHandles;
    }
}

void MainWindow::refreshCurvePointTable()
{
    if (!m_curvePointTable) {
        return;
    }

    const QVector<QPointF> curve = curveForChannel(m_currentCurveChannel);
    const QVector<CurveHandleMode> handles = curveHandlesForChannel(m_currentCurveChannel);
    const int selected = m_curveWidget ? m_curveWidget->selectedPoint() : -1;
    QSignalBlocker blocker(m_curvePointTable);
    m_curvePointTable->setRowCount(curve.size());

    auto setTableItem = [&](int row, int column, const QString& text, bool editable) {
        auto* item = m_curvePointTable->item(row, column);
        if (!item) {
            item = new QTableWidgetItem();
            m_curvePointTable->setItem(row, column, item);
        }
        item->setText(text);
        item->setTextAlignment(Qt::AlignCenter);
        Qt::ItemFlags flags = item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        if (editable) {
            flags |= Qt::ItemIsEditable;
        } else {
            flags &= ~Qt::ItemIsEditable;
        }
        item->setFlags(flags);
    };

    for (int i = 0; i < curve.size(); ++i) {
        const bool endpointX = i == 0 || i == curve.size() - 1;
        setTableItem(i, 0, QString::number(curve.at(i).x(), 'f', 4), !endpointX);
        setTableItem(i, 1, QString::number(curve.at(i).y(), 'f', 4), true);
        const bool autoHandle = i < handles.size() && handles.at(i) == CurveHandleMode::Auto;
        setTableItem(i, 2, autoHandle ? "自动" : "直线", true);
    }

    if (selected >= 0 && selected < m_curvePointTable->rowCount()) {
        m_curvePointTable->setCurrentCell(selected, 1);
        m_curvePointTable->selectRow(selected);
    } else {
        m_curvePointTable->clearSelection();
        m_curvePointTable->setCurrentItem(nullptr);
    }
}

void MainWindow::applyCurvePointTableEdit(int row, int column)
{
    if (!m_curvePointTable || !m_curveWidget || row < 0 || column < 0) {
        return;
    }

    QVector<QPointF> curve = curveForChannel(m_currentCurveChannel);
    QVector<CurveHandleMode> handles = curveHandlesForChannel(m_currentCurveChannel);
    if (row >= curve.size()) {
        refreshCurvePointTable();
        return;
    }
    while (handles.size() < curve.size()) {
        handles.append(CurveHandleMode::Vector);
    }
    while (handles.size() > curve.size()) {
        handles.removeLast();
    }

    QTableWidgetItem* item = m_curvePointTable->item(row, column);
    if (!item) {
        return;
    }

    if (column == 0 || column == 1) {
        bool ok = false;
        const double value = item->text().toDouble(&ok);
        if (!ok) {
            refreshCurvePointTable();
            return;
        }
        QPointF point = curve.at(row);
        if (column == 0) {
            if (row == 0) {
                point.setX(0.0);
            } else if (row == curve.size() - 1) {
                point.setX(1.0);
            } else {
                point.setX(std::clamp(value, 0.0, 1.0));
            }
        } else {
            point.setY(std::clamp(value, 0.0, 1.0));
        }
        curve[row] = point;
    } else if (column == 2) {
        const QString handleText = item->text().trimmed().toLower();
        handles[row] = handleText.startsWith("a", Qt::CaseInsensitive) || handleText.startsWith(QStringLiteral("自")) ? CurveHandleMode::Auto : CurveHandleMode::Vector;
    } else {
        return;
    }

    {
        QSignalBlocker blocker(m_curveWidget);
        m_curveWidget->setCurve(curve);
        m_curveWidget->setHandleModes(handles);
        const int lastRow = static_cast<int>(m_curveWidget->curve().size()) - 1;
        m_curveWidget->setSelectedPoint(std::clamp(row, 0, lastRow));
    }
    setCurveForChannel(m_currentCurveChannel, m_curveWidget->curve());
    setCurveHandlesForChannel(m_currentCurveChannel, m_curveWidget->handleModes());
    refreshCurveHandleButtons();
    refreshCurvePointTable();
    markDirty();
    schedulePreview();
}

void MainWindow::selectCurvePointTableRow(int index)
{
    if (!m_curvePointTable) {
        return;
    }
    QSignalBlocker blocker(m_curvePointTable);
    if (index >= 0 && index < m_curvePointTable->rowCount()) {
        m_curvePointTable->setCurrentCell(index, 1);
        m_curvePointTable->selectRow(index);
    } else {
        m_curvePointTable->clearSelection();
        m_curvePointTable->setCurrentItem(nullptr);
    }
}

void MainWindow::resetCurveChannel(int index)
{
    const QVector<QPointF> identity = {QPointF(0.0, 0.0), QPointF(1.0, 1.0)};
    const QVector<CurveHandleMode> identityHandles = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    setCurveForChannel(index, identity);
    setCurveHandlesForChannel(index, identityHandles);
    if (m_curveWidget) {
        if (m_currentCurveChannel == index) {
            m_curveWidget->setCurve(identity);
            m_curveWidget->setHandleModes(identityHandles);
            refreshCurveHandleButtons();
            refreshCurvePointTable();
        }
    }
}

void MainWindow::resetAllCurves()
{
    for (int i = 0; i < 4; ++i) {
        resetCurveChannel(i);
    }
}

void MainWindow::resetHslBand(int band)
{
    if (band < 0 || band >= 8) {
        return;
    }
    const size_t index = static_cast<size_t>(band);
    m_document.pipeline.params.hslHue[index] = 0.0f;
    m_document.pipeline.params.hslSaturation[index] = 0.0f;
    m_document.pipeline.params.hslLuminance[index] = 0.0f;
}

void MainWindow::resetAllHsl()
{
    for (int i = 0; i < 8; ++i) {
        resetHslBand(i);
    }
}

QString MainWindow::adjustmentStageLabel(int index) const
{
    if (index < 0 || index >= m_document.pipeline.stages.size()) {
        return QString();
    }
    const ColorGradeStage& stage = m_document.pipeline.stages.at(index);
    const QString name = stage.label.trimmed().isEmpty()
        ? (index == 0 ? QString("基础调色") : QString("调整 %1").arg(index))
        : stage.label.trimmed();
    QString maskText = "无遮罩";
    const auto maskIt = std::find_if(m_document.masks.begin(), m_document.masks.end(), [&](const MaskAsset& mask) {
        return mask.maskId == stage.maskRef.id;
    });
    if (maskIt != m_document.masks.end() && maskIt->enabled && maskIt->kind != MaskKind::Full) {
        maskText = maskKindLabel(maskIt->kind);
    }
    return QString("%1  %2%3  %4%  %5  %6")
        .arg(stage.enabled ? "开 " : "关")
        .arg(stage.solo ? "独显 " : "")
        .arg(name)
        .arg(static_cast<int>(std::round(std::clamp(stage.opacity, 0.0f, 1.0f) * 100.0f)))
        .arg(stage.blendMode.isEmpty() ? "normal" : stage.blendMode)
        .arg(maskText);
}

void MainWindow::refreshAdjustmentStackUi()
{
    if (!m_adjustmentList) {
        return;
    }
    if (m_document.pipeline.stages.isEmpty()) {
        rebuildDefaultStages();
    }

    {
        QSignalBlocker blocker(m_adjustmentList);
        m_adjustmentList->clear();
        for (int i = 0; i < m_document.pipeline.stages.size(); ++i) {
            auto* item = new QListWidgetItem(adjustmentStageLabel(i), m_adjustmentList);
            item->setData(Qt::UserRole, i);
            if (!m_document.pipeline.stages.at(i).enabled) {
                item->setForeground(QColor(130, 136, 146));
            } else if (m_document.pipeline.stages.at(i).solo) {
                item->setForeground(QColor(255, 211, 118));
            }
        }
        m_activeStageIndex = std::clamp(m_activeStageIndex, 0, std::max(0, static_cast<int>(m_document.pipeline.stages.size()) - 1));
        m_adjustmentList->setCurrentRow(m_activeStageIndex);
    }

    const bool hasStage = m_activeStageIndex >= 0 && m_activeStageIndex < m_document.pipeline.stages.size();
    if (m_adjustmentNameEdit) {
        QSignalBlocker blocker(m_adjustmentNameEdit);
        m_adjustmentNameEdit->setEnabled(hasStage);
        m_adjustmentNameEdit->setText(hasStage ? m_document.pipeline.stages.at(m_activeStageIndex).label : QString());
        if (hasStage) {
        m_adjustmentNameEdit->setPlaceholderText(m_activeStageIndex == 0 ? "基础调色" : QString("调整 %1").arg(m_activeStageIndex));
        }
    }
    if (m_adjustmentEnabledCheck) {
        QSignalBlocker blocker(m_adjustmentEnabledCheck);
        m_adjustmentEnabledCheck->setEnabled(hasStage);
        m_adjustmentEnabledCheck->setChecked(hasStage && m_document.pipeline.stages.at(m_activeStageIndex).enabled);
    }
    if (m_adjustmentSoloCheck) {
        QSignalBlocker blocker(m_adjustmentSoloCheck);
        m_adjustmentSoloCheck->setEnabled(hasStage);
        m_adjustmentSoloCheck->setChecked(hasStage && m_document.pipeline.stages.at(m_activeStageIndex).solo);
    }
    if (m_adjustmentBlendModeCombo) {
        QSignalBlocker blocker(m_adjustmentBlendModeCombo);
        m_adjustmentBlendModeCombo->setEnabled(hasStage);
        const QString blendMode = hasStage ? m_document.pipeline.stages.at(m_activeStageIndex).blendMode : QString("normal");
        const int index = m_adjustmentBlendModeCombo->findData(blendMode.isEmpty() ? QString("normal") : blendMode);
        m_adjustmentBlendModeCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (m_adjustmentOpacityControl) {
        QSignalBlocker blocker(m_adjustmentOpacityControl);
        m_adjustmentOpacityControl->setEnabled(hasStage);
        m_adjustmentOpacityControl->setValue(hasStage ? m_document.pipeline.stages.at(m_activeStageIndex).opacity : 1.0);
    }
    if (m_bypassAllButton) {
        QSignalBlocker blocker(m_bypassAllButton);
        m_bypassAllButton->setChecked(m_adjustmentsBypassed);
    }
}

void MainWindow::syncActiveStageParams()
{
    if (m_document.pipeline.stages.isEmpty()) {
        rebuildDefaultStages();
    }
    m_activeStageIndex = std::clamp(m_activeStageIndex, 0, std::max(0, static_cast<int>(m_document.pipeline.stages.size()) - 1));
    if (isParamGradeStageType(m_document.pipeline.stages.at(m_activeStageIndex).type)) {
        m_document.pipeline.stages[m_activeStageIndex].params = paramsToJson(m_document.pipeline.params);
    }
    syncNodeGraphFromPipelineStages();
}

void MainWindow::loadActiveStageParams(int index)
{
    if (m_document.pipeline.stages.isEmpty()) {
        rebuildDefaultStages();
    }
    m_activeStageIndex = std::clamp(index, 0, std::max(0, static_cast<int>(m_document.pipeline.stages.size()) - 1));

    const QString inputColorSpace = m_document.pipeline.params.inputColorSpace;
    const float importedStrength = m_document.pipeline.params.importedLutStrength;
    const QString activeImported = m_document.pipeline.params.activeImportedLutAssetId;
    if (isParamGradeStageType(m_document.pipeline.stages.at(m_activeStageIndex).type)) {
        ColorGradeParams params = paramsFromJson(m_document.pipeline.stages.at(m_activeStageIndex).params);
        params.inputColorSpace = inputColorSpace;
        params.importedLutStrength = importedStrength;
        params.activeImportedLutAssetId = activeImported;
        m_document.pipeline.params = params;
    } else {
        m_document.pipeline.params.inputColorSpace = inputColorSpace;
        m_document.pipeline.params.importedLutStrength = importedStrength;
        m_document.pipeline.params.activeImportedLutAssetId = activeImported;
    }
    refreshAdjustmentStackUi();
    refreshNodePropertyUi();
}

void MainWindow::addAdjustmentStage()
{
    syncActiveStageParams();
    ColorGradeParams params;
    params.inputColorSpace = m_document.pipeline.params.inputColorSpace;
    params.importedLutStrength = m_document.pipeline.params.importedLutStrength;
    params.activeImportedLutAssetId = m_document.pipeline.params.activeImportedLutAssetId;

    ColorGradeStage stage;
    stage.stageId = QString("adjustment-%1").arg(m_document.pipeline.stages.size());
    stage.label = QString("调整 %1").arg(m_document.pipeline.stages.size());
    stage.type = "globalColorGrade";
    stage.enabled = true;
    stage.opacity = 1.0f;
    stage.blendMode = "normal";
    stage.params = paramsToJson(params);
    m_document.pipeline.stages.append(stage);
    loadActiveStageParams(static_cast<int>(m_document.pipeline.stages.size()) - 1);
    if (m_document.nodeGraph.enabled) {
        rebuildNodeGraphFromPipeline(m_nodeGraphModeCombo ? m_nodeGraphModeCombo->currentData().toString() : QString("serial"));
    }
    refreshParameterWidgets();
    markDirty();
    schedulePreview();
}

void MainWindow::addNodeGraphStage(const QString& type, const QPointF& position, const QString& insertAfterNodeId)
{
    Q_UNUSED(insertAfterNodeId);
    syncActiveStageParams();
    if (!m_document.nodeGraph.enabled) {
        m_document.nodeGraph.enabled = true;
    }
    if (m_document.nodeGraph.nodes.isEmpty()) {
        rebuildNodeGraphFromPipeline("serial");
    }

    ColorGradeParams params;
    params.inputColorSpace = m_document.pipeline.params.inputColorSpace;
    params.importedLutStrength = m_document.pipeline.params.importedLutStrength;
    params.activeImportedLutAssetId = m_document.pipeline.params.activeImportedLutAssetId;

    const QString suffix = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    const QString stageId = QString("node-grade-%1").arg(suffix);
    const QString nodeType = type.isEmpty() ? QString("globalColorGrade") : type;
    const QString label = nodeStageLabelForType(nodeType);

    ColorGradeStage stage;
    stage.stageId = stageId;
    stage.label = label;
    stage.type = nodeType;
    stage.enabled = true;
    stage.opacity = 1.0f;
    stage.blendMode = "normal";
    stage.params = defaultNodeParamsForType(stage.type, params);
    m_document.pipeline.stages.append(stage);
    m_selectedNodeGraphNodeId.clear();

    NodeGraphNode node{stage.stageId, stage.type, stage.label, position, true, stage.params};
    m_document.nodeGraph.nodes.append(node);
    const bool hasOutput = std::any_of(m_document.nodeGraph.nodes.begin(), m_document.nodeGraph.nodes.end(), [](const NodeGraphNode& existing) {
        return existing.nodeId == "node-output";
    });
    if (!hasOutput) {
        m_document.nodeGraph.outputNodeId = "node-output";
        m_document.nodeGraph.nodes.append(NodeGraphNode{"node-output", "output", "输出", position + QPointF(220, 0), true, QJsonObject{}});
    }

    loadActiveStageParams(static_cast<int>(m_document.pipeline.stages.size()) - 1);
    refreshParameterWidgets();
    refreshNodeGraphUi();
    markDirty();
    schedulePreview();
}

void MainWindow::cloneNodeGraphStage(const NodeGraphNode& sourceNode, const QPointF& position)
{
    if (sourceNode.type == "input" || sourceNode.type == "output") {
        return;
    }
    syncActiveStageParams();
    if (!m_document.nodeGraph.enabled) {
        m_document.nodeGraph.enabled = true;
    }
    if (m_document.nodeGraph.nodes.isEmpty()) {
        rebuildNodeGraphFromPipeline("serial");
    }

    const QString suffix = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    ColorGradeStage stage;
    stage.stageId = QString("node-grade-%1").arg(suffix);
    stage.label = sourceNode.label.trimmed().isEmpty() ? nodeStageLabelForType(sourceNode.type) : sourceNode.label.trimmed() + " 副本";
    stage.type = sourceNode.type.isEmpty() ? QString("globalColorGrade") : sourceNode.type;
    stage.enabled = true;
    stage.opacity = 1.0f;
    stage.blendMode = "normal";
    stage.params = sourceNode.params;
    m_document.pipeline.stages.append(stage);

    m_selectedNodeGraphNodeId.clear();
    m_document.nodeGraph.nodes.append(NodeGraphNode{stage.stageId, stage.type, stage.label, position, true, stage.params});
    loadActiveStageParams(static_cast<int>(m_document.pipeline.stages.size()) - 1);
    refreshParameterWidgets();
    refreshNodeGraphUi();
    markDirty();
    schedulePreview();
}

void MainWindow::removeActiveAdjustmentStage()
{
    if (m_document.pipeline.stages.size() <= 1) {
        return;
    }
    m_document.pipeline.stages.removeAt(m_activeStageIndex);
    m_activeStageIndex = std::clamp(m_activeStageIndex, 0, static_cast<int>(m_document.pipeline.stages.size()) - 1);
    loadActiveStageParams(m_activeStageIndex);
    if (m_document.nodeGraph.enabled) {
        rebuildNodeGraphFromPipeline(m_nodeGraphModeCombo ? m_nodeGraphModeCombo->currentData().toString() : QString("serial"));
    }
    refreshParameterWidgets();
    markDirty();
    schedulePreview();
}

void MainWindow::duplicateActiveAdjustmentStage()
{
    if (m_activeStageIndex < 0 || m_activeStageIndex >= m_document.pipeline.stages.size()) {
        return;
    }
    syncActiveStageParams();
    ColorGradeStage copy = m_document.pipeline.stages.at(m_activeStageIndex);
    copy.stageId = QString("adjustment-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    copy.label = copy.label.trimmed().isEmpty()
        ? QString("调整副本")
        : copy.label.trimmed() + " 副本";
    copy.solo = false;
    if (!copy.maskRef.id.isEmpty()) {
        const QString oldMaskId = copy.maskRef.id;
        const QString newMaskId = QString("%1-%2").arg(kLocalMaskId, copy.stageId);
        const auto maskIt = std::find_if(m_document.masks.begin(), m_document.masks.end(), [&](const MaskAsset& mask) {
            return mask.maskId == oldMaskId;
        });
        if (maskIt != m_document.masks.end()) {
            MaskAsset maskCopy = *maskIt;
            maskCopy.maskId = newMaskId;
            m_document.masks.append(maskCopy);
            copy.maskRef.id = newMaskId;
        }
    }
    const int insertIndex = m_activeStageIndex + 1;
    m_document.pipeline.stages.insert(insertIndex, copy);
    loadActiveStageParams(insertIndex);
    if (m_document.nodeGraph.enabled) {
        rebuildNodeGraphFromPipeline(m_nodeGraphModeCombo ? m_nodeGraphModeCombo->currentData().toString() : QString("serial"));
    }
    refreshParameterWidgets();
    markDirty();
    schedulePreview();
}

void MainWindow::moveActiveAdjustmentStage(int direction)
{
    if (m_activeStageIndex < 0 || m_activeStageIndex >= m_document.pipeline.stages.size()) {
        return;
    }
    const int next = std::clamp(m_activeStageIndex + direction, 0, static_cast<int>(m_document.pipeline.stages.size()) - 1);
    if (next == m_activeStageIndex) {
        return;
    }
    syncActiveStageParams();
    m_document.pipeline.stages.move(m_activeStageIndex, next);
    loadActiveStageParams(next);
    if (m_document.nodeGraph.enabled) {
        rebuildNodeGraphFromPipeline(m_nodeGraphModeCombo ? m_nodeGraphModeCombo->currentData().toString() : QString("serial"));
    }
    refreshParameterWidgets();
    markDirty();
    schedulePreview();
}

void MainWindow::clearSoloExcept(int index)
{
    for (int i = 0; i < m_document.pipeline.stages.size(); ++i) {
        if (i != index) {
            m_document.pipeline.stages[i].solo = false;
        }
    }
}

QString MainWindow::localMaskIdForStage(int index) const
{
    if (index < 0 || index >= m_document.pipeline.stages.size()) {
        return QString(kLocalMaskId);
    }
    const QString stageId = m_document.pipeline.stages.at(index).stageId.isEmpty()
        ? QString("stage-%1").arg(index)
        : m_document.pipeline.stages.at(index).stageId;
    return QString("%1-%2").arg(kLocalMaskId, stageId);
}

void MainWindow::ensureLocalMaskAsset()
{
    if (m_document.pipeline.stages.isEmpty()) {
        rebuildDefaultStages();
    }
    m_activeStageIndex = std::clamp(m_activeStageIndex, 0, std::max(0, static_cast<int>(m_document.pipeline.stages.size()) - 1));

    ColorGradeStage& stage = m_document.pipeline.stages[m_activeStageIndex];
    const QString localMaskId = localMaskIdForStage(m_activeStageIndex);
    if (stage.maskRef.id.isEmpty()) {
        stage.maskRef.id = localMaskId;
        stage.maskRef.opacity = 1.0f;
    }

    auto it = std::find_if(m_document.masks.begin(), m_document.masks.end(), [&](const MaskAsset& mask) {
        return mask.maskId == stage.maskRef.id || mask.maskId == localMaskId;
    });
    if (it == m_document.masks.end()) {
        MaskAsset mask;
        mask.maskId = localMaskId;
        mask.kind = MaskKind::Full;
        mask.enabled = false;
        mask.opacity = 1.0f;
        mask.params = QJsonObject{
            {"centerX", 0.5},
            {"centerY", 0.5},
            {"radius", 0.45},
            {"angle", 0.0},
            {"position", 0.5},
            {"hue", 0.0},
            {"saturation", 0.65},
            {"luma", 0.5},
            {"tolerance", 0.18},
            {"softness", 0.18},
            {"brushRadius", 0.04},
            {"strokes", QJsonArray{}},
        };
        m_document.masks.append(mask);
    } else if (it->maskId != localMaskId && it->maskId == stage.maskRef.id) {
        it->maskId = localMaskId;
    }
    stage.maskRef.id = localMaskId;
}

MaskAsset* MainWindow::localMaskForActiveStage()
{
    ensureLocalMaskAsset();
    const QString id = localMaskIdForStage(m_activeStageIndex);
    auto it = std::find_if(m_document.masks.begin(), m_document.masks.end(), [&](const MaskAsset& mask) {
        return mask.maskId == id;
    });
    return it == m_document.masks.end() ? nullptr : &(*it);
}

const MaskAsset* MainWindow::localMaskForActiveStage() const
{
    if (m_activeStageIndex < 0 || m_activeStageIndex >= m_document.pipeline.stages.size()) {
        return nullptr;
    }
    const QString id = localMaskIdForStage(m_activeStageIndex);
    auto it = std::find_if(m_document.masks.begin(), m_document.masks.end(), [&](const MaskAsset& mask) {
        return mask.maskId == id || mask.maskId == m_document.pipeline.stages.at(m_activeStageIndex).maskRef.id;
    });
    return it == m_document.masks.end() ? nullptr : &(*it);
}

MaskReference MainWindow::activeMaskReference() const
{
    if (m_activeStageIndex >= 0 && m_activeStageIndex < m_document.pipeline.stages.size()) {
        const ColorGradeStage& stage = m_document.pipeline.stages.at(m_activeStageIndex);
        const MaskAsset* mask = localMaskForActiveStage();
        if (mask && mask->enabled && mask->kind != MaskKind::Full) {
            return stage.maskRef;
        }
    }
    return MaskReference();
}

void MainWindow::setLocalMaskKind(MaskKind kind)
{
    ensureLocalMaskAsset();
    if (MaskAsset* mask = localMaskForActiveStage()) {
        mask->kind = kind;
        mask->enabled = kind != MaskKind::Full;
    }
    if (m_activeStageIndex >= 0 && m_activeStageIndex < m_document.pipeline.stages.size()) {
        m_document.pipeline.stages[m_activeStageIndex].maskRef.id = localMaskIdForStage(m_activeStageIndex);
        m_document.pipeline.stages[m_activeStageIndex].maskRef.opacity = 1.0f;
    }
    refreshMaskOverlay();
}

void MainWindow::setLocalMaskParam(const QString& key, double value)
{
    ensureLocalMaskAsset();
    if (MaskAsset* mask = localMaskForActiveStage()) {
        mask->params[key] = value;
    }
}

void MainWindow::refreshLocalMaskUi()
{
    stripLocalMasksFromDocument();
    return;
    if (!m_maskTypeCombo) {
        return;
    }
    ensureLocalMaskAsset();
    const MaskAsset* maskPtr = localMaskForActiveStage();
    if (!maskPtr) {
        return;
    }
    const MaskAsset& mask = *maskPtr;
    const MaskKind kind = mask.enabled ? mask.kind : MaskKind::Full;
    {
        QSignalBlocker blocker(m_maskTypeCombo);
        const int index = m_maskTypeCombo->findData(static_cast<int>(kind));
        m_maskTypeCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (m_maskInvertCheck) {
        QSignalBlocker blocker(m_maskInvertCheck);
        m_maskInvertCheck->setChecked(mask.inverted);
        m_maskInvertCheck->setEnabled(kind != MaskKind::Full);
    }
    if (m_maskOverlayCheck) {
        QSignalBlocker blocker(m_maskOverlayCheck);
        m_maskOverlayCheck->setChecked(m_maskOverlayEnabled);
        m_maskOverlayCheck->setEnabled(kind != MaskKind::Full);
    }
    if (m_maskViewCheck) {
        QSignalBlocker blocker(m_maskViewCheck);
        m_maskViewCheck->setChecked(m_maskViewEnabled);
        m_maskViewCheck->setEnabled(kind != MaskKind::Full);
    }
    if (m_maskEditButton) {
        QSignalBlocker blocker(m_maskEditButton);
        m_maskEditButton->setEnabled(kind == MaskKind::Gradient || kind == MaskKind::Radial || kind == MaskKind::Paint);
        m_maskEditButton->setChecked(m_maskEditMode && m_maskEditButton->isEnabled());
        if (!m_maskEditButton->isEnabled()) {
            m_maskEditMode = false;
            if (m_resultView) {
                m_resultView->setMaskEditMode(false);
            }
        }
    }
    if (m_maskPickColorButton) {
        QSignalBlocker blocker(m_maskPickColorButton);
        m_maskPickColorButton->setText("添加采样");
        m_maskPickColorButton->setEnabled(kind == MaskKind::ColorRange);
        m_maskPickColorButton->setChecked(m_maskColorPickMode && !m_maskSubtractSample);
    }
    if (m_maskSubtractColorButton) {
        QSignalBlocker blocker(m_maskSubtractColorButton);
        m_maskSubtractColorButton->setText(kind == MaskKind::Paint ? "擦除绘制" : "减去");
        m_maskSubtractColorButton->setEnabled(kind == MaskKind::ColorRange || kind == MaskKind::Paint);
        m_maskSubtractColorButton->setChecked(kind == MaskKind::Paint ? m_maskSubtractSample : (m_maskColorPickMode && m_maskSubtractSample));
    }
    if (m_maskClearSamplesButton) {
        if (kind == MaskKind::Paint) {
            m_maskClearSamplesButton->setText("清除绘制笔画");
            m_maskClearSamplesButton->setEnabled(mask.params.value("strokes").toArray().size() > 0);
        } else if (kind == MaskKind::ExternalImage) {
            m_maskClearSamplesButton->setText("清除遮罩图片");
            m_maskClearSamplesButton->setEnabled(!mask.params.value("imagePath").toString().isEmpty());
        } else {
            m_maskClearSamplesButton->setText("清除限定器采样");
            m_maskClearSamplesButton->setEnabled(kind == MaskKind::ColorRange && mask.params.value("samples").toArray().size() > 0);
        }
    }
    if (m_maskUndoStrokeButton) {
        m_maskUndoStrokeButton->setVisible(kind == MaskKind::Paint);
        m_maskUndoStrokeButton->setEnabled(kind == MaskKind::Paint && mask.params.value("strokes").toArray().size() > 0);
    }
    if (m_maskLoadImageButton) {
        m_maskLoadImageButton->setEnabled(kind == MaskKind::ExternalImage);
        const QString imagePath = mask.params.value("imagePath").toString();
        const QFileInfo imageInfo(imagePath);
        const bool missing = kind == MaskKind::ExternalImage && !imagePath.isEmpty() && !imageInfo.exists();
        m_maskLoadImageButton->setText(imagePath.isEmpty()
            ? "载入遮罩图片"
            : missing ? "重新链接缺失图片" : QString("图片：%1").arg(imageInfo.fileName()));
        if (kind == MaskKind::ExternalImage && !imagePath.isEmpty() && !imageInfo.exists()) {
            setStatus("遮罩图片缺失：" + imagePath);
        }
    }

    auto configure = [](ParameterControl* control,
                        const QString& label,
                        double minimum,
                        double maximum,
                        double step,
                        int decimals,
                        double fallback,
                        const QJsonObject& params,
                        const QString& key,
                        bool enabled) {
        if (!control) {
            return;
        }
        QSignalBlocker blocker(control);
        control->setEnabled(enabled);
        control->setLabelText(label);
        control->setRange(minimum, maximum, step, decimals, fallback);
        control->setValue(params.value(key).toDouble(fallback));
    };

    configure(m_maskOpacityControl, "不透明度", 0.0, 1.0, 0.01, 2, 1.0, QJsonObject{{"opacity", mask.opacity}}, "opacity", kind != MaskKind::Full);
    if (kind == MaskKind::Gradient) {
        configure(m_maskParamAControl, "角度", -180.0, 180.0, 1.0, 0, 0.0, mask.params, "angle", true);
        configure(m_maskParamBControl, "位置", 0.0, 1.0, 0.01, 2, 0.5, mask.params, "position", true);
        configure(m_maskParamCControl, "羽化", 0.001, 1.0, 0.01, 2, 0.18, mask.params, "softness", true);
        configure(m_maskParamDControl, "未使用", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamEControl, "未使用", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
    } else if (kind == MaskKind::Radial) {
        configure(m_maskParamAControl, "中心 X", 0.0, 1.0, 0.01, 2, 0.5, mask.params, "centerX", true);
        configure(m_maskParamBControl, "中心 Y", 0.0, 1.0, 0.01, 2, 0.5, mask.params, "centerY", true);
        configure(m_maskParamCControl, "半径", 0.001, 1.5, 0.01, 2, 0.45, mask.params, "radius", true);
        configure(m_maskParamDControl, "羽化", 0.001, 1.5, 0.01, 2, 0.18, mask.params, "softness", true);
        configure(m_maskParamEControl, "未使用", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
    } else if (kind == MaskKind::ColorRange) {
        configure(m_maskParamAControl, "色相中心", 0.0, 1.0, 0.001, 3, 0.0, mask.params, "hue", true);
        configure(m_maskParamBControl, "饱和中心", 0.0, 1.0, 0.001, 3, 0.65, mask.params, "saturation", true);
        configure(m_maskParamCControl, "亮度中心", 0.0, 1.0, 0.001, 3, 0.5, mask.params, "luma", true);
        configure(m_maskParamDControl, "范围", 0.001, 1.0, 0.01, 2, 0.18, mask.params, "tolerance", true);
        configure(m_maskParamEControl, "羽化", 0.001, 1.0, 0.01, 2, 0.12, mask.params, "softness", true);
    } else if (kind == MaskKind::Paint) {
        configure(m_maskParamAControl, "画笔大小", 0.001, 0.5, 0.005, 3, 0.04, mask.params, "brushRadius", true);
        configure(m_maskParamBControl, "羽化", 0.001, 0.5, 0.005, 3, 0.04, mask.params, "softness", true);
        configure(m_maskParamCControl, "未使用", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamDControl, "未使用", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamEControl, "未使用", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
    } else if (kind == MaskKind::ExternalImage) {
        configure(m_maskParamAControl, "图片亮度", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamBControl, "未使用", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamCControl, "未使用", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamDControl, "未使用", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamEControl, "未使用", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
    } else {
        configure(m_maskParamAControl, "A", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamBControl, "B", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamCControl, "C", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamDControl, "D", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
        configure(m_maskParamEControl, "E", 0.0, 1.0, 0.01, 2, 0.0, mask.params, "", false);
    }
    refreshMaskOverlay();
}

void MainWindow::refreshMaskOverlay()
{
    stripLocalMasksFromDocument();
    return;
    if (!m_resultView) {
        return;
    }
    const MaskAsset* mask = localMaskForActiveStage();
    const bool hasVisibleMask = !m_sourceImage.isNull() && mask && mask->enabled && mask->kind != MaskKind::Full;
    if (!hasVisibleMask) {
        m_resultView->setMaskOverlay(QImage());
        m_resultView->setMaskGuide(MaskKind::Full, QJsonObject(), false);
        if (m_maskViewEnabled) {
            updatePreviewDisplay();
        }
        return;
    }

    const QSize overlaySize = !m_previewImage.isNull() ? m_previewImage.size() : m_sourceImage.size();
    if (overlaySize.isEmpty()) {
        m_resultView->setMaskOverlay(QImage());
        return;
    }

    QJsonObject maskKeyObj;
    maskKeyObj["id"] = mask->maskId;
    maskKeyObj["kind"] = static_cast<int>(mask->kind);
    maskKeyObj["enabled"] = mask->enabled;
    maskKeyObj["inverted"] = mask->inverted;
    maskKeyObj["opacity"] = mask->opacity;
    maskKeyObj["params"] = mask->params;
    maskKeyObj["sourceSize"] = QJsonArray{m_sourceImage.width(), m_sourceImage.height()};
    const QString maskKey = QString::fromUtf8(QJsonDocument(maskKeyObj).toJson(QJsonDocument::Compact));
    if (m_cachedMaskPreview.isNull() || m_cachedMaskPreviewSize != overlaySize || m_cachedMaskPreviewKey != maskKey) {
        m_cachedMaskPreview = makeMaskPreviewImage(overlaySize);
        m_cachedMaskPreviewSize = overlaySize;
        m_cachedMaskPreviewKey = maskKey;
    }
    const QImage maskPreview = m_cachedMaskPreview;
    m_resultView->setMaskGuide(mask->kind, mask->params, m_maskEditMode || m_maskOverlayEnabled || m_maskViewEnabled);
    if (m_maskViewEnabled) {
        m_resultView->setImage(maskPreview);
        m_resultView->setMaskOverlay(QImage());
        m_resultView->setOverlayInfo(QString("遮罩视图 | %1").arg(maskKindLabel(mask->kind)));
        return;
    }

    updatePreviewDisplay();
    if (!m_maskOverlayEnabled) {
        m_resultView->setMaskOverlay(QImage());
        return;
    }

    QImage overlay(overlaySize, QImage::Format_ARGB32_Premultiplied);
    overlay.fill(Qt::transparent);
    for (int y = 0; y < overlay.height(); ++y) {
        const QRgb* maskLine = reinterpret_cast<const QRgb*>(maskPreview.constScanLine(y));
        QRgb* line = reinterpret_cast<QRgb*>(overlay.scanLine(y));
        for (int x = 0; x < overlay.width(); ++x) {
            const float value = qRed(maskLine[x]) / 255.0f;
            const int alpha = std::clamp(static_cast<int>(value * 155.0f), 0, 155);
            if (alpha > 0) {
                line[x] = qRgba(74, 190, 255, alpha);
            }
        }
    }
    m_resultView->setMaskOverlay(overlay);
}

void MainWindow::setMaskColorPickMode(bool enabled)
{
    setMaskColorPickMode(enabled, false);
}

void MainWindow::setMaskColorPickMode(bool enabled, bool subtractSample)
{
    bool finalEnabled = enabled;
    if (finalEnabled && m_sourceImage.isNull()) {
        finalEnabled = false;
        setStatus("请先载入输入图像，再采样颜色范围");
    }
    if (finalEnabled && m_warperImagePickMode) {
        setWarperImagePickMode(false);
    }
    if (finalEnabled) {
        setLocalMaskKind(MaskKind::ColorRange);
        refreshLocalMaskUi();
    }

    m_maskColorPickMode = finalEnabled;
    m_maskSubtractSample = finalEnabled && subtractSample;
    if (m_sourceView) {
        m_sourceView->setPickMode(finalEnabled,
                                  finalEnabled
                                      ? (m_maskSubtractSample ? "点击输入图像以从颜色范围中减去" : "点击输入图像以添加颜色范围采样")
                                      : QString());
    }
    if (m_maskPickColorButton) {
        QSignalBlocker blocker(m_maskPickColorButton);
        m_maskPickColorButton->setChecked(finalEnabled && !m_maskSubtractSample);
    }
    if (m_maskSubtractColorButton) {
        QSignalBlocker blocker(m_maskSubtractColorButton);
        m_maskSubtractColorButton->setChecked(finalEnabled && m_maskSubtractSample);
    }
}

void MainWindow::applyMaskColorSample(const QColor& color)
{
    setLocalMaskKind(MaskKind::ColorRange);
    ensureLocalMaskAsset();

    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
    color.getHsvF(&hue, &saturation, &value);
    if (hue < 0.0f) {
        hue = 0.0f;
    }
    const double luma = std::clamp(color.redF() * 0.2126 + color.greenF() * 0.7152 + color.blueF() * 0.0722, 0.0, 1.0);
    if (MaskAsset* mask = localMaskForActiveStage()) {
        mask->params["hue"] = std::clamp(static_cast<double>(hue), 0.0, 1.0);
        mask->params["saturation"] = std::clamp(static_cast<double>(saturation), 0.0, 1.0);
        mask->params["luma"] = luma;
        QJsonArray samples = mask->params.value("samples").toArray();
        samples.append(QJsonObject{
            {"hue", std::clamp(static_cast<double>(hue), 0.0, 1.0)},
            {"saturation", std::clamp(static_cast<double>(saturation), 0.0, 1.0)},
            {"luma", luma},
            {"subtract", m_maskSubtractSample},
        });
        mask->params["samples"] = samples;
        if (!mask->params.contains("tolerance")) {
            mask->params["tolerance"] = 0.18;
        }
        if (!mask->params.contains("softness")) {
            mask->params["softness"] = 0.12;
        }
    }
    refreshLocalMaskUi();
    markDirty();
    schedulePreview();
}

void MainWindow::resetActiveMask()
{
    ensureLocalMaskAsset();
    MaskAsset* mask = localMaskForActiveStage();
    if (!mask) {
        return;
    }
    const MaskKind kind = mask->kind;
    mask->inverted = false;
    mask->opacity = 1.0f;
    mask->params = QJsonObject{
        {"centerX", 0.5},
        {"centerY", 0.5},
        {"radius", 0.45},
        {"angle", 0.0},
        {"position", 0.5},
        {"hue", 0.0},
        {"saturation", 0.65},
        {"luma", 0.5},
        {"tolerance", 0.18},
        {"softness", 0.18},
        {"brushRadius", 0.04},
        {"strokes", QJsonArray{}},
    };
    mask->kind = kind;
    mask->enabled = kind != MaskKind::Full;
    refreshLocalMaskUi();
    refreshMaskOverlay();
}

QImage MainWindow::makeMaskPreviewImage(const QSize& size) const
{
    const MaskAsset* mask = localMaskForActiveStage();
    if (m_sourceImage.isNull() || !mask || !mask->enabled || mask->kind == MaskKind::Full || size.isEmpty()) {
        return QImage();
    }

    const QImage src = m_sourceImage.convertToFormat(QImage::Format_RGBA8888);
    QImage out(size, QImage::Format_RGBA8888);
    QVector<MaskAsset> masks;
    masks.append(*mask);
    MaskReference ref;
    ref.id = mask->maskId;
    ref.opacity = 1.0f;

    const int outWidth = std::max(1, out.width() - 1);
    const int outHeight = std::max(1, out.height() - 1);
    for (int y = 0; y < out.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            const QPointF uv(x / static_cast<double>(outWidth), y / static_cast<double>(outHeight));
            const int sx = std::clamp(static_cast<int>(uv.x() * (src.width() - 1)), 0, src.width() - 1);
            const int sy = std::clamp(static_cast<int>(uv.y() * (src.height() - 1)), 0, src.height() - 1);
            const QRgb p = src.pixel(sx, sy);
            const QVector3D color(qRed(p) / 255.0f, qGreen(p) / 255.0f, qBlue(p) / 255.0f);
            const int value = std::clamp(static_cast<int>(MaskEvaluator::evaluate(masks, ref, uv, color) * 255.0f + 0.5f), 0, 255);
            line[x] = qRgba(value, value, value, 255);
        }
    }
    return out;
}

void MainWindow::applyMaskDrag(const QPointF& startUv, const QPointF& endUv)
{
    ensureLocalMaskAsset();
    MaskAsset* mask = localMaskForActiveStage();
    if (!mask) {
        return;
    }
    if (mask->kind == MaskKind::Full || !mask->enabled) {
        setLocalMaskKind(MaskKind::Radial);
        mask = localMaskForActiveStage();
        if (!mask) {
            return;
        }
    }

    const double dx = endUv.x() - startUv.x();
    const double dy = endUv.y() - startUv.y();
    const double length = std::clamp(std::hypot(dx, dy), 0.001, 1.5);
    if (mask->kind == MaskKind::Gradient) {
        const double angle = qRadiansToDegrees(std::atan2(dy, dx));
        const QPointF center((startUv.x() + endUv.x()) * 0.5, (startUv.y() + endUv.y()) * 0.5);
        const QPointF axis(std::cos(qDegreesToRadians(angle)), std::sin(qDegreesToRadians(angle)));
        const double position = std::clamp((center.x() - 0.5) * axis.x() + (center.y() - 0.5) * axis.y() + 0.5, 0.0, 1.0);
        mask->params["angle"] = angle;
        mask->params["position"] = position;
        mask->params["softness"] = std::clamp(length * 0.5, 0.01, 1.0);
    } else if (mask->kind == MaskKind::Radial) {
        mask->params["centerX"] = std::clamp(startUv.x(), 0.0, 1.0);
        mask->params["centerY"] = std::clamp(startUv.y(), 0.0, 1.0);
        mask->params["radius"] = std::clamp(length, 0.01, 1.5);
    } else if (mask->kind == MaskKind::Paint) {
        QJsonArray strokes = mask->params.value("strokes").toArray();
        strokes.append(QJsonObject{
            {"x1", std::clamp(startUv.x(), 0.0, 1.0)},
            {"y1", std::clamp(startUv.y(), 0.0, 1.0)},
            {"x2", std::clamp(endUv.x(), 0.0, 1.0)},
            {"y2", std::clamp(endUv.y(), 0.0, 1.0)},
            {"radius", std::clamp(mask->params.value("brushRadius").toDouble(0.04), 0.001, 0.5)},
            {"softness", std::clamp(mask->params.value("softness").toDouble(0.04), 0.001, 0.5)},
            {"strength", 1.0},
            {"subtract", m_maskSubtractSample},
        });
        mask->params["strokes"] = strokes;
    }
    refreshLocalMaskUi();
    markDirty();
    schedulePreview();
}

void MainWindow::configureHueVsCurve(CurveWidget* widget,
                                     const QVector<QPointF>& curve,
                                     const QVector<CurveHandleMode>& handles,
                                     const QColor& color)
{
    if (!widget) {
        return;
    }
    QSignalBlocker blocker(widget);
    widget->setChannelColor(color);
    widget->setNeutralLineY(0.5);
    widget->setCurve(curve);
    widget->setHandleModes(handles);
}

void MainWindow::resetHueVsCurve(int index)
{
    const QVector<QPointF> neutral = {QPointF(0.0, 0.5), QPointF(1.0, 0.5)};
    const QVector<CurveHandleMode> handles = {CurveHandleMode::Vector, CurveHandleMode::Vector};
    if (index == 0) {
        m_document.pipeline.params.hueVsHueCurve = neutral;
        m_document.pipeline.params.hueVsHueCurveHandles = handles;
        configureHueVsCurve(m_hueVsCurveWidgets[0], neutral, handles, QColor(236, 157, 73));
    } else if (index == 1) {
        m_document.pipeline.params.hueVsSaturationCurve = neutral;
        m_document.pipeline.params.hueVsSaturationCurveHandles = handles;
        configureHueVsCurve(m_hueVsCurveWidgets[1], neutral, handles, QColor(88, 204, 147));
    } else if (index == 2) {
        m_document.pipeline.params.hueVsLuminanceCurve = neutral;
        m_document.pipeline.params.hueVsLuminanceCurveHandles = handles;
        configureHueVsCurve(m_hueVsCurveWidgets[2], neutral, handles, QColor(108, 168, 255));
    }
}

void MainWindow::refreshParameterWidgets()
{
    const QColor curveColor = m_currentCurveChannel == 1 ? QColor(244, 89, 89)
        : m_currentCurveChannel == 2 ? QColor(112, 211, 118)
        : m_currentCurveChannel == 3 ? QColor(92, 166, 255)
                                     : QColor(225, 225, 225);

    auto setControl = [](ParameterControl* control, float value) {
        if (!control) {
            return;
        }
        QSignalBlocker blocker(control);
        control->setValue(value);
    };

    if (m_inputColorSpaceCombo) {
        QSignalBlocker blocker(m_inputColorSpaceCombo);
        const QStringList keys = inputColorSpaceKeys();
        int index = keys.indexOf(m_document.pipeline.params.inputColorSpace.trimmed().toLower());
        if (index < 0) {
            index = 0;
            m_document.pipeline.params.inputColorSpace = "srgb";
        }
        m_inputColorSpaceCombo->setCurrentIndex(index);
    }
    refreshAdjustmentStackUi();
    refreshLocalMaskUi();

    setControl(m_lutStrengthControl, m_document.pipeline.params.importedLutStrength);
    if (m_lutStrengthControl) {
        m_lutStrengthControl->setEnabled(m_importedLut.isValid());
    }
    if (m_clearImportedLutButton) {
        m_clearImportedLutButton->setEnabled(m_importedLut.isValid()
            || !m_document.pipeline.params.activeImportedLutAssetId.isEmpty()
            || !m_document.importedLuts.isEmpty());
    }
    setControl(m_exposureControl, m_document.pipeline.params.exposure);
    setControl(m_contrastControl, m_document.pipeline.params.contrast);
    setControl(m_saturationControl, m_document.pipeline.params.saturation);
    setControl(m_vibranceControl, m_document.pipeline.params.vibrance);
    setControl(m_temperatureControl, m_document.pipeline.params.temperature);
    setControl(m_tintControl, m_document.pipeline.params.tint);
    setControl(m_highlightsControl, m_document.pipeline.params.highlights);
    setControl(m_shadowsControl, m_document.pipeline.params.shadows);
    setControl(m_whitesControl, m_document.pipeline.params.whites);
    setControl(m_blacksControl, m_document.pipeline.params.blacks);
    setControl(m_textureControl, m_document.pipeline.params.texture);
    setControl(m_clarityControl, m_document.pipeline.params.clarity);
    setControl(m_dehazeControl, m_document.pipeline.params.dehaze);
    setControl(m_sharpenControl, m_document.pipeline.params.sharpen);
    setControl(m_logShadowControl, m_document.pipeline.params.logShadow);
    setControl(m_logDarkControl, m_document.pipeline.params.logDark);
    setControl(m_logLightControl, m_document.pipeline.params.logLight);
    setControl(m_logHighlightControl, m_document.pipeline.params.logHighlight);
    setControl(m_hdrShadowControl, m_document.pipeline.params.hdrShadow);
    setControl(m_hdrDarkControl, m_document.pipeline.params.hdrDark);
    setControl(m_hdrLightControl, m_document.pipeline.params.hdrLight);
    setControl(m_hdrHighlightControl, m_document.pipeline.params.hdrHighlight);
    setControl(m_printerRedControl, m_document.pipeline.params.printerRed);
    setControl(m_printerGreenControl, m_document.pipeline.params.printerGreen);
    setControl(m_printerBlueControl, m_document.pipeline.params.printerBlue);
    setControl(m_skinHueControl, m_document.pipeline.params.skinToneHue);
    setControl(m_skinSaturationControl, m_document.pipeline.params.skinToneSaturation);
    setControl(m_skinLuminanceControl, m_document.pipeline.params.skinToneLuminance);
    setControl(m_calibrationRedHueControl, m_document.pipeline.params.calibrationRedHue);
    setControl(m_calibrationRedSaturationControl, m_document.pipeline.params.calibrationRedSaturation);
    setControl(m_calibrationGreenHueControl, m_document.pipeline.params.calibrationGreenHue);
    setControl(m_calibrationGreenSaturationControl, m_document.pipeline.params.calibrationGreenSaturation);
    setControl(m_calibrationBlueHueControl, m_document.pipeline.params.calibrationBlueHue);
    setControl(m_calibrationBlueSaturationControl, m_document.pipeline.params.calibrationBlueSaturation);

    const QVector3D wheelVectors[] = {
        m_document.pipeline.params.lift,
        m_document.pipeline.params.gamma,
        m_document.pipeline.params.gain,
        m_document.pipeline.params.offset,
    };
    setControl(m_wheelControls[0], wheelVectors[0].x());
    setControl(m_wheelControls[1], wheelVectors[0].y());
    setControl(m_wheelControls[2], wheelVectors[0].z());
    setControl(m_wheelControls[3], wheelVectors[1].x());
    setControl(m_wheelControls[4], wheelVectors[1].y());
    setControl(m_wheelControls[5], wheelVectors[1].z());
    setControl(m_wheelControls[6], wheelVectors[2].x());
    setControl(m_wheelControls[7], wheelVectors[2].y());
    setControl(m_wheelControls[8], wheelVectors[2].z());
    setControl(m_wheelControls[9], wheelVectors[3].x());
    setControl(m_wheelControls[10], wheelVectors[3].y());
    setControl(m_wheelControls[11], wheelVectors[3].z());

    if (m_curveWidget) {
        QSignalBlocker blocker(m_curveWidget);
        m_curveWidget->setCurve(curveForChannel(m_currentCurveChannel));
        m_curveWidget->setHandleModes(curveHandlesForChannel(m_currentCurveChannel));
        m_curveWidget->setChannelColor(curveColor);
    }
    refreshCurveHandleButtons();
    refreshCurvePointTable();
    configureHueVsCurve(m_hueVsCurveWidgets[0],
                        m_document.pipeline.params.hueVsHueCurve,
                        m_document.pipeline.params.hueVsHueCurveHandles,
                        QColor(236, 157, 73));
    configureHueVsCurve(m_hueVsCurveWidgets[1],
                        m_document.pipeline.params.hueVsSaturationCurve,
                        m_document.pipeline.params.hueVsSaturationCurveHandles,
                        QColor(88, 204, 147));
    configureHueVsCurve(m_hueVsCurveWidgets[2],
                        m_document.pipeline.params.hueVsLuminanceCurve,
                        m_document.pipeline.params.hueVsLuminanceCurveHandles,
                        QColor(108, 168, 255));
    if (m_colorWarper) {
        {
            QSignalBlocker blocker(m_colorWarper);
            m_colorWarper->setWarpPoints(m_document.pipeline.params.colorWarpSources,
                                         m_document.pipeline.params.colorWarpPoints,
                                         m_document.pipeline.params.colorWarpPinned);
        }
        m_document.pipeline.params.colorWarpSources = m_colorWarper->sources();
        m_document.pipeline.params.colorWarpPoints = m_colorWarper->points();
        m_document.pipeline.params.colorWarpPinned = m_colorWarper->pinned();
        m_warperSelectedIndex = m_colorWarper->selectedPoint();
    }
    m_warperSelectedSources = m_document.pipeline.params.colorWarpSources;
    m_warperSelectedTargets = m_document.pipeline.params.colorWarpPoints;
    m_warperSelectedPinned = m_document.pipeline.params.colorWarpPinned;
    refreshWarperSelectionUi();
    refreshNodeGraphUi();

    for (int i = 0; i < 8; ++i) {
        setControl(m_hslControls[static_cast<size_t>(i)],
                   m_document.pipeline.params.hslHue[static_cast<size_t>(i)]);
        setControl(m_hslControls[static_cast<size_t>(8 + i)],
                   m_document.pipeline.params.hslSaturation[static_cast<size_t>(i)]);
        setControl(m_hslControls[static_cast<size_t>(16 + i)],
                   m_document.pipeline.params.hslLuminance[static_cast<size_t>(i)]);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!maybeSaveChanges()) {
        event->ignore();
        return;
    }

    autosaveNow();
    QSettings settings("IkClutStudio", "IkClutStudio");
    settings.setValue("lastProjectPath", m_document.projectPath);
    settings.setValue("recentProjects", m_recentProjects);
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    applyResponsiveLayout();
}

void MainWindow::applyResponsiveLayout()
{
    if (!m_tabs || !m_mainSplitter) {
        return;
    }

    const int windowWidth = width();
    const bool compact = windowWidth < 1320;
    m_tabs->setMinimumWidth(compact ? 320 : 380);

    const QList<int> sizes = m_mainSplitter->sizes();
    if (sizes.size() != 2) {
        return;
    }

    const int preferredPanelWidth = compact ? 320 : 400;
    const int currentPanelWidth = sizes[1];
    if (currentPanelWidth < m_tabs->minimumWidth()) {
        m_mainSplitter->setSizes({qMax(560, windowWidth - preferredPanelWidth), preferredPanelWidth});
    }
}

MainWindow::HistoryState MainWindow::captureHistoryState() const
{
    HistoryState state;
    state.document = m_document;
    state.importedLut = m_importedLut;
    state.activeStageIndex = m_activeStageIndex;
    state.adjustmentsBypassed = m_adjustmentsBypassed;
    state.maskOverlayEnabled = m_maskOverlayEnabled;
    state.maskViewEnabled = m_maskViewEnabled;
    return state;
}

void MainWindow::restoreHistoryState(const HistoryState& state)
{
    m_restoringHistory = true;
    m_document = state.document;
    m_importedLut = state.importedLut;
    m_activeStageIndex = state.activeStageIndex;
    m_adjustmentsBypassed = state.adjustmentsBypassed;
    m_maskOverlayEnabled = state.maskOverlayEnabled;
    m_maskViewEnabled = state.maskViewEnabled;
    if (m_document.pipeline.stages.isEmpty()) {
        rebuildDefaultStages();
    }
    loadActiveStageParams(m_activeStageIndex);
    syncTransformSource();
    refreshParameterWidgets();
    refreshMaskOverlay();
    schedulePreview();
    setDirty(true);
    m_restoringHistory = false;
}

void MainWindow::noteUndoCheckpoint()
{
    if (m_restoringHistory) {
        return;
    }
    if (!m_hasUndoBaseline) {
        m_lastUndoState = captureHistoryState();
        m_hasUndoBaseline = true;
    }
    m_undoCheckpointPending = true;
    m_undoCoalesceTimer.start();
}

void MainWindow::finalizeUndoCheckpoint()
{
    if (m_restoringHistory || !m_hasUndoBaseline || !m_undoCheckpointPending) {
        return;
    }
    m_undoStack.append(m_lastUndoState);
    while (m_undoStack.size() > 80) {
        m_undoStack.removeFirst();
    }
    m_redoStack.clear();
    m_lastUndoState = captureHistoryState();
    m_undoCheckpointPending = false;
    refreshUndoRedoActions();
}

void MainWindow::resetUndoHistory()
{
    m_undoCoalesceTimer.stop();
    m_undoStack.clear();
    m_redoStack.clear();
    m_lastUndoState = captureHistoryState();
    m_hasUndoBaseline = true;
    m_undoCheckpointPending = false;
    refreshUndoRedoActions();
}

void MainWindow::undo()
{
    finalizeUndoCheckpoint();
    if (m_undoStack.isEmpty()) {
        return;
    }
    const HistoryState previous = m_undoStack.takeLast();
    m_redoStack.append(captureHistoryState());
    restoreHistoryState(previous);
    m_lastUndoState = captureHistoryState();
    m_undoCheckpointPending = false;
    refreshUndoRedoActions();
}

void MainWindow::redo()
{
    if (m_redoStack.isEmpty()) {
        return;
    }
    const HistoryState next = m_redoStack.takeLast();
    m_undoStack.append(captureHistoryState());
    restoreHistoryState(next);
    m_lastUndoState = captureHistoryState();
    m_undoCheckpointPending = false;
    refreshUndoRedoActions();
}

void MainWindow::refreshUndoRedoActions()
{
    if (m_undoAction) {
        m_undoAction->setEnabled(!m_undoStack.isEmpty());
    }
    if (m_redoAction) {
        m_redoAction->setEnabled(!m_redoStack.isEmpty());
    }
}

void MainWindow::markDirty()
{
    noteUndoCheckpoint();
    setDirty(true);
}

void MainWindow::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        return;
    }
    m_dirty = dirty;
    updateWindowTitle();
}

bool MainWindow::maybeSaveChanges()
{
    if (!m_dirty) {
        return true;
    }

    const QMessageBox::StandardButton result = QMessageBox::question(
        this,
        tr("Unsaved Changes"),
        tr("Save changes to the current project?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (result == QMessageBox::Cancel) {
        return false;
    }
    if (result == QMessageBox::Discard) {
        return true;
    }

    if (m_document.projectPath.isEmpty()) {
        saveProject();
    } else {
        saveProjectPath(m_document.projectPath);
    }
    return !m_dirty;
}

void MainWindow::updateWindowTitle()
{
    const QString name = m_document.projectPath.isEmpty()
        ? tr("Untitled")
        : QFileInfo(m_document.projectPath).completeBaseName();
    setWindowTitle(QString("%1%2 - IKCLUT Studio").arg(m_dirty ? "*" : "", name));
}

void MainWindow::addRecentProject(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }
    m_recentProjects.removeAll(path);
    m_recentProjects.prepend(path);
    while (m_recentProjects.size() > 8) {
        m_recentProjects.removeLast();
    }
    QSettings settings("IkClutStudio", "IkClutStudio");
    settings.setValue("recentProjects", m_recentProjects);
    settings.setValue("lastProjectPath", path);
    rebuildRecentProjectsMenu();
}

void MainWindow::rebuildRecentProjectsMenu()
{
    if (!m_recentProjectsMenu) {
        return;
    }
    m_recentProjectsMenu->clear();
    for (const QString& path : m_recentProjects) {
        QAction* action = m_recentProjectsMenu->addAction(QFileInfo(path).fileName());
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            if (maybeSaveChanges()) {
                openProjectPath(path);
            }
        });
    }
    if (m_recentProjects.isEmpty()) {
        QAction* empty = m_recentProjectsMenu->addAction(tr("No recent projects"));
        empty->setEnabled(false);
    }
}

void MainWindow::openProjectPath(const QString& path)
{
    ImageDocument document;
    QString error;
    if (!ProjectSerializer::loadProject(path, &document, &error)) {
        QMessageBox::warning(this, tr("Open Project"), error);
        return;
    }

    m_document = document;
    m_document.projectPath = path;
    m_activeStageIndex = 0;
    loadActiveStageParams(0);
    restoreImportedLutFromDocument();
    refreshParameterWidgets();
    if (!m_document.imagePath.isEmpty()) {
        m_sourceImage = QImage(m_document.imagePath);
        m_cachedBeforeScaled = QImage();
        m_cachedMaskPreview = QImage();
        m_sourceView->setImage(m_sourceImage);
        if (m_warperImagePickMode) {
            m_sourceView->setPickMode(true, tr("Click the Input image to create a Warper point"));
        }
    } else {
        m_sourceImage = QImage();
        m_previewImage = QImage();
        m_cachedBeforeScaled = QImage();
        m_cachedMaskPreview = QImage();
        m_sourceView->setImage(QImage());
        m_resultView->setImage(QImage());
        setWarperImagePickMode(false);
    }
    addRecentProject(path);
    setDirty(false);
    resetUndoHistory();
    schedulePreview();
    setStatus(tr("Project opened"));
}

void MainWindow::restoreImportedLutFromDocument()
{
    m_importedLut = Lut3D();
    if (m_document.importedLuts.isEmpty()) {
        m_document.pipeline.params.activeImportedLutAssetId.clear();
        syncTransformSource();
        return;
    }

    const QString activeId = m_document.pipeline.params.activeImportedLutAssetId;
    const ImportedLutAsset* selectedAsset = nullptr;
    for (const ImportedLutAsset& asset : m_document.importedLuts) {
        if (!activeId.isEmpty() && asset.assetId == activeId) {
            selectedAsset = &asset;
            break;
        }
    }
    if (!selectedAsset) {
        selectedAsset = &m_document.importedLuts.first();
        m_document.pipeline.params.activeImportedLutAssetId = selectedAsset->assetId;
    }

    const LutImportResult result = LutImportService::importFile(selectedAsset->sourcePath);
    if (result.ok) {
        m_importedLut = result.lut;
    } else {
        m_document.pipeline.params.importedLutStrength = 0.0f;
    }
    syncTransformSource();
}

void MainWindow::saveProjectPath(const QString& path)
{
    syncStageParams();
    ImageDocument toSave = m_document;
    toSave.projectPath = path;
    QString error;
    if (!ProjectSerializer::saveProject(toSave, path, &error)) {
        QMessageBox::warning(this, tr("Save Project"), error);
        return;
    }
    m_document.projectPath = path;
    addRecentProject(path);
    setDirty(false);
    resetUndoHistory();
    setStatus(tr("Project saved"));
}

QString MainWindow::autosavePath() const
{
    return QDir::current().filePath("IkClutStudio.autosave.ikclutproj");
}

void MainWindow::autosaveNow()
{
    if (!m_dirty) {
        return;
    }
    syncStageParams();
    QString error;
    ImageDocument toSave = m_document;
    toSave.projectPath = autosavePath();
    if (ProjectSerializer::saveProject(toSave, autosavePath(), &error)) {
        setStatus(tr("Autosaved"));
    }
}

void MainWindow::restoreLastSession()
{
    QSettings settings("IkClutStudio", "IkClutStudio");
    const QString lastProject = settings.value("lastProjectPath").toString();
    if (!lastProject.isEmpty() && QFileInfo::exists(lastProject)) {
        openProjectPath(lastProject);
    }
}

void MainWindow::scheduleScopes(const QImage& image)
{
    const quint64 generation = ++m_scopeGeneration;
    auto* watcher = new QFutureWatcher<ScopeData>(this);
    connect(watcher, &QFutureWatcher<ScopeData>::finished, this, [this, watcher, generation]() {
        watcher->deleteLater();
        if (generation != m_scopeGeneration) {
            return;
        }
        const ScopeData data = watcher->result();
        m_scopeWidget->setScopeData(data);
        if (m_resultView && data.sampledPixels > 0) {
            const double shadow = 100.0 * data.shadowClipPixels / data.sampledPixels;
            const double highlight = 100.0 * data.highlightClipPixels / data.sampledPixels;
            const double gamut = 100.0 * data.gamutWarningPixels / data.sampledPixels;
            m_resultView->setOverlayInfo(QString("Clip S %1% H %2% | Gamut %3%")
                                             .arg(shadow, 0, 'f', 2)
                                             .arg(highlight, 0, 'f', 2)
                                             .arg(gamut, 0, 'f', 2));
        }
        if (m_curveWidget) {
            QVector<float> histogram;
            histogram.reserve(data.luminanceHistogram.size());
            for (int value : data.luminanceHistogram) {
                histogram.append(static_cast<float>(value));
            }
            m_curveWidget->setHistogram(histogram);
        }
    });
    watcher->setFuture(QtConcurrent::run([image]() {
        return ScopeRenderer::compute(image);
    }));
}

void MainWindow::refreshLutViewerAsync()
{
    syncTransformSource();
    const quint64 generation = m_lutGeneration;
    const ColorTransformSource source = m_transformSource;
    const ColorGradeParams params = m_document.pipeline.params;
    const Lut3D imported = m_importedLut;
    auto* watcher = new QFutureWatcher<Lut3D>(this);
    connect(watcher, &QFutureWatcher<Lut3D>::finished, this, [this, watcher, generation]() {
        watcher->deleteLater();
        if (generation != m_lutGeneration) {
            return;
        }
        m_currentPreviewLut = watcher->result();
        if (m_currentPreviewLut.isValid()) {
            m_lutViewer->setLut(m_currentPreviewLut);
            updateIkClutStrip(m_currentPreviewLut);
        }
    });
    watcher->setFuture(QtConcurrent::run([source, params, imported]() {
        if (source.usePipeline) {
            if (source.kind == ColorTransformKind::ImportedLut && imported.isValid()) {
                return ColorPipeline::composePipelineLut(source.pipeline, source.masks, imported, source.importedLutStrength, 17);
            }
            return ColorPipeline::bakePipelineLut(source.pipeline, source.masks, 17);
        }
        if (source.kind == ColorTransformKind::ImportedLut && imported.isValid()) {
            return ColorPipeline::composeLut(params, imported, source.importedLutStrength, 17);
        }
        return ColorPipeline::bakeLut(params, 17);
    }));
}

void MainWindow::updateIkClutStrip(const Lut3D& lut)
{
    if (!m_ikClutStripView) {
        return;
    }
    const QImage strip = IkClutExporter::makeImage(lut);
    m_ikClutStripView->setImage(strip);
    m_ikClutStripView->setOverlayInfo(strip.isNull()
        ? tr("No export LUT")
        : QString("%1 x %2").arg(strip.width()).arg(strip.height()));
}

void MainWindow::updateBusy(bool busy, const QString& text)
{
    m_progress->setVisible(busy);
    setStatus(text);
}

void MainWindow::setStatus(const QString& text)
{
    m_statusLabel->setText(text);
}

void MainWindow::rebuildDefaultStages()
{
    m_document.pipeline.stages.clear();
    ColorGradeStage stage;
    stage.stageId = "global-grade";
    stage.label = tr("Base Grade");
    stage.type = "globalColorGrade";
    stage.enabled = true;
    stage.params = paramsToJson(m_document.pipeline.params);
    stage.blendMode = "normal";
    stage.maskRef.id = "";
    stage.maskRef.inverted = false;
    stage.maskRef.opacity = 1.0f;
    stage.inputTransformRef = "";
    m_document.pipeline.stages.append(stage);
    m_transformSource.params = m_document.pipeline.params;
}

void MainWindow::rebuildNodeGraphFromPipeline(const QString& mode)
{
    syncStageParams();
    m_selectedNodeGraphNodeId.clear();
    NodeGraph graph;
    graph.enabled = true;
    graph.outputNodeId = "node-output";
    graph.nodes.append(NodeGraphNode{"node-input", "input", tr("Input"), QPointF(0, 90), true, QJsonObject{{"graphMode", mode}}});

    QStringList previousOutputs;
    previousOutputs.append("node-input");
    for (int i = 0; i < m_document.pipeline.stages.size(); ++i) {
        const ColorGradeStage& stage = m_document.pipeline.stages.at(i);
        const bool parallel = mode == "parallel" || mode == "layerMixer";
        const QPointF position(parallel ? 220.0 : 220.0 + i * 180.0,
                               parallel ? 30.0 + i * 96.0 : 90.0);
        graph.nodes.append(NodeGraphNode{stage.stageId, stage.type.isEmpty() ? QString("globalColorGrade") : stage.type, stage.label, position, stage.enabled, stage.params});
        if (parallel) {
            graph.edges.append(NodeGraphEdge{QString("edge-input-%1").arg(i), NodeGraphPortRef{"node-input", "out"}, NodeGraphPortRef{stage.stageId, "in"}});
            previousOutputs.append(stage.stageId);
        } else {
            graph.edges.append(NodeGraphEdge{QString("edge-%1").arg(i), NodeGraphPortRef{previousOutputs.last(), "out"}, NodeGraphPortRef{stage.stageId, "in"}});
            previousOutputs = {stage.stageId};
        }
    }

    if (mode == "layerMixer" || mode == "parallel") {
        QJsonObject mixerParams = defaultNodeParamsForType(mode == "layerMixer" ? "layerMixer" : "parallelMixer", m_document.pipeline.params);
        mixerParams["graphMode"] = mode;
        graph.nodes.append(NodeGraphNode{"node-mixer", mode == "layerMixer" ? "layerMixer" : "parallelMixer", mode == "layerMixer" ? tr("Layer Mixer") : tr("Parallel Mixer"), QPointF(470, 90), true, mixerParams});
        for (int i = 1; i < previousOutputs.size(); ++i) {
            graph.edges.append(NodeGraphEdge{QString("edge-mix-%1").arg(i), NodeGraphPortRef{previousOutputs.at(i), "out"}, NodeGraphPortRef{"node-mixer", QString("in%1").arg(i)}});
        }
        previousOutputs = {"node-mixer"};
    }

    graph.nodes.append(NodeGraphNode{"node-output", "output", tr("Output"), QPointF(mode == "serial" ? 420.0 + m_document.pipeline.stages.size() * 180.0 : 700.0, 90), true, QJsonObject{}});
    graph.edges.append(NodeGraphEdge{"edge-output", NodeGraphPortRef{previousOutputs.last(), "out"}, NodeGraphPortRef{"node-output", "in"}});
    m_document.nodeGraph = graph;
    refreshNodeGraphUi();
}

void MainWindow::syncNodeGraphFromPipelineStages()
{
    if (!m_document.nodeGraph.enabled || m_document.nodeGraph.nodes.isEmpty()) {
        return;
    }
    for (NodeGraphNode& node : m_document.nodeGraph.nodes) {
        const auto it = std::find_if(m_document.pipeline.stages.begin(), m_document.pipeline.stages.end(), [&](const ColorGradeStage& stage) {
            return stage.stageId == node.nodeId;
        });
        if (it == m_document.pipeline.stages.end()) {
            continue;
        }
        node.type = it->type.isEmpty() ? QString("globalColorGrade") : it->type;
        node.label = it->label;
        node.enabled = it->enabled;
        node.params = it->params;
    }
}

ColorGradePipeline MainWindow::pipelineFromNodeGraph() const
{
    ColorGradePipeline pipeline;
    pipeline.params = m_document.pipeline.params;
    if (!m_document.nodeGraph.enabled || m_document.nodeGraph.nodes.isEmpty()) {
        pipeline.stages = m_document.pipeline.stages;
        return pipeline;
    }

    QHash<QString, NodeGraphNode> nodesById;
    for (const NodeGraphNode& node : m_document.nodeGraph.nodes) {
        nodesById.insert(node.nodeId, node);
    }
    QHash<QString, QString> incomingByNodePort;
    for (const NodeGraphEdge& edge : m_document.nodeGraph.edges) {
        incomingByNodePort.insert(edge.to.nodeId + "\n" + edge.to.portId, edge.from.nodeId);
        if (edge.to.portId == "in") {
            incomingByNodePort.insert(edge.to.nodeId + "\nin0", edge.from.nodeId);
        }
    }

    const auto incoming = [&](const QString& nodeId, const QString& port) {
        const QString exact = incomingByNodePort.value(nodeId + "\n" + port);
        if (!exact.isEmpty()) {
            return exact;
        }
        return port == "in0" ? incomingByNodePort.value(nodeId + "\nin") : QString();
    };
    const auto incomingBranches = [&](const QString& nodeId) {
        QVector<QPair<int, QString>> branches;
        for (const NodeGraphEdge& edge : m_document.nodeGraph.edges) {
            if (edge.to.nodeId != nodeId || edge.from.nodeId.isEmpty()) {
                continue;
            }
            int index = 0;
            if (edge.to.portId.startsWith("in")) {
                bool ok = false;
                const int parsed = edge.to.portId.mid(2).toInt(&ok);
                index = ok ? parsed : 0;
            }
            branches.append(qMakePair(index, edge.from.nodeId));
        }
        std::sort(branches.begin(), branches.end(), [](const auto& a, const auto& b) {
            return a.first == b.first ? a.second < b.second : a.first < b.first;
        });
        return branches;
    };
    const auto embeddedStageJson = [&](const NodeGraphNode& node) {
        QJsonObject obj;
        const auto stageIt = std::find_if(m_document.pipeline.stages.begin(), m_document.pipeline.stages.end(), [&](const ColorGradeStage& stage) {
            return stage.stageId == node.nodeId;
        });
        obj["stageId"] = node.nodeId;
        obj["type"] = node.type.isEmpty() ? QString("globalColorGrade") : node.type;
        obj["label"] = node.label;
        obj["enabled"] = node.enabled;
        obj["opacity"] = stageIt == m_document.pipeline.stages.end() ? 1.0 : stageIt->opacity;
        obj["blendMode"] = stageIt == m_document.pipeline.stages.end() ? QString("normal") : stageIt->blendMode;
        obj["params"] = node.params;
        return obj;
    };

    QSet<QString> visitedMain;
    std::function<void(const QString&)> appendPath;
    std::function<QJsonArray(const QString&)> embeddedPath;
    std::function<QJsonArray(const QString&, QSet<QString>)> embeddedPathGuarded;

    appendPath = [&](const QString& nodeId) {
        if (nodeId.isEmpty() || visitedMain.contains(nodeId)) {
            return;
        }
        visitedMain.insert(nodeId);
        const NodeGraphNode node = nodesById.value(nodeId);
        if (node.type == "output") {
            appendPath(incoming(nodeId, "in0"));
            return;
        }
        if (node.type == "input") {
            return;
        }
        if (node.type == "parallelMixer" || node.type == "layerMixer") {
            ColorGradeStage stage;
            stage.stageId = node.nodeId;
            stage.type = node.type;
            stage.label = node.label;
            stage.enabled = node.enabled;
            stage.params = node.params;
            QJsonArray branches;
            for (const auto& branch : incomingBranches(nodeId)) {
                const QJsonArray stages = embeddedPath(branch.second);
                if (!stages.isEmpty()) {
                    branches.append(stages);
                }
            }
            stage.params["branches"] = branches;
            pipeline.stages.append(stage);
            return;
        }
        appendPath(incoming(nodeId, "in0"));
        if (node.type != "parallelMixer" && node.type != "layerMixer") {
            const auto stageIt = std::find_if(m_document.pipeline.stages.begin(), m_document.pipeline.stages.end(), [&](const ColorGradeStage& stage) {
                return stage.stageId == node.nodeId;
            });
            ColorGradeStage stage = stageIt == m_document.pipeline.stages.end() ? ColorGradeStage() : *stageIt;
            stage.stageId = node.nodeId;
            stage.type = node.type.isEmpty() ? QString("globalColorGrade") : node.type;
            stage.label = node.label;
            stage.enabled = node.enabled;
            stage.params = node.params;
            if (stage.type == "mixColor") {
                const QJsonArray branchB = embeddedPath(incoming(nodeId, "in1"));
                if (!branchB.isEmpty()) {
                    stage.params["branchBStages"] = branchB;
                }
            }
            pipeline.stages.append(stage);
        }
    };

    embeddedPathGuarded = [&](const QString& nodeId, QSet<QString> activePath) {
        QJsonArray stages;
        if (nodeId.isEmpty() || activePath.contains(nodeId)) {
            return stages;
        }
        activePath.insert(nodeId);
        const NodeGraphNode node = nodesById.value(nodeId);
        if (node.type == "input" || node.type == "output") {
            return stages;
        }
        if (node.type == "parallelMixer" || node.type == "layerMixer") {
            QJsonObject obj = embeddedStageJson(node);
            QJsonObject params = obj.value("params").toObject();
            QJsonArray branches;
            for (const auto& branch : incomingBranches(nodeId)) {
                const QJsonArray branchStages = embeddedPathGuarded(branch.second, activePath);
                if (!branchStages.isEmpty()) {
                    branches.append(branchStages);
                }
            }
            params["branches"] = branches;
            obj["params"] = params;
            stages.append(obj);
            return stages;
        }
        const QJsonArray previous = embeddedPathGuarded(incoming(nodeId, "in0"), activePath);
        for (const QJsonValue& value : previous) {
            stages.append(value);
        }
        QJsonObject obj = embeddedStageJson(node);
        if (node.type == "mixColor") {
            const QJsonArray branchB = embeddedPathGuarded(incoming(nodeId, "in1"), activePath);
            if (!branchB.isEmpty()) {
                QJsonObject p = obj.value("params").toObject();
                p["branchBStages"] = branchB;
                obj["params"] = p;
            }
        }
        stages.append(obj);
        return stages;
    };
    embeddedPath = [&](const QString& nodeId) {
        return embeddedPathGuarded(nodeId, {});
    };

    appendPath(m_document.nodeGraph.outputNodeId.isEmpty() ? QString("node-output") : m_document.nodeGraph.outputNodeId);
    return pipeline;
}

void MainWindow::selectStageById(const QString& stageId)
{
    const auto it = std::find_if(m_document.pipeline.stages.begin(), m_document.pipeline.stages.end(), [&](const ColorGradeStage& stage) {
        return stage.stageId == stageId;
    });
    if (it == m_document.pipeline.stages.end()) {
        return;
    }
    const int index = static_cast<int>(std::distance(m_document.pipeline.stages.begin(), it));
    if (index == m_activeStageIndex) {
        return;
    }
    syncActiveStageParams();
    loadActiveStageParams(index);
    refreshParameterWidgets();
    schedulePreview();
}

void MainWindow::refreshNodeGraphUi()
{
    if (m_nodeGraphEnabledCheck) {
        QSignalBlocker blocker(m_nodeGraphEnabledCheck);
        m_nodeGraphEnabledCheck->setChecked(m_document.nodeGraph.enabled);
    }
    if (m_nodeGraphModeCombo) {
        QSignalBlocker blocker(m_nodeGraphModeCombo);
        const QString mode = m_document.nodeGraph.nodes.isEmpty()
            ? QString("serial")
            : m_document.nodeGraph.nodes.first().params.value("graphMode").toString("serial");
        const int index = m_nodeGraphModeCombo->findData(mode);
        m_nodeGraphModeCombo->setCurrentIndex(index < 0 ? 0 : index);
    }
    if (m_nodeGraphWidget) {
        QSignalBlocker blocker(m_nodeGraphWidget);
        m_nodeGraphWidget->setGraph(m_document.nodeGraph);
    }
    if (m_nodeGraphDiagnosticLabel) {
        const ExecutionPlan plan = ExecutionPlanCompiler::compileDocument(m_document);
        const bool hasWarning = plan.diagnostic.contains("Cycle detected", Qt::CaseInsensitive)
            || plan.diagnostic.contains("not connected", Qt::CaseInsensitive);
        const int executableSteps = std::count_if(plan.steps.begin(), plan.steps.end(), [](const ExecutionStep& step) {
            return step.kind == ExecutionStepKind::Stage;
        });
        QString diagnostic = plan.diagnostic;
        diagnostic.replace("Compiled node graph.", "节点图已编译。");
        diagnostic.replace("Compiled linear pipeline.", "线性流程已编译。");
        diagnostic.replace("Cycle detected; cyclic branches were skipped.", "检测到循环；已跳过循环分支。");
        diagnostic.replace("Output is not connected to the input flow.", "输出未连接到输入流程。");
        m_nodeGraphDiagnosticLabel->setText(QString("%1 可执行阶段：%2").arg(diagnostic).arg(executableSteps));
        m_nodeGraphDiagnosticLabel->setStyleSheet(hasWarning
            ? "color: #ffd39a; background: #21170f; border: 1px solid #805b2d; padding: 6px;"
            : "color: #9fa7b3; background: #171a20; border: 1px solid #2d3440; padding: 6px;");
    }
    refreshNodePropertyUi();
}

void MainWindow::refreshNodePropertyUi()
{
    auto setVisible = [](QWidget* widget, bool visible) {
        if (widget) {
            widget->setVisible(visible);
        }
    };

    const auto selectedStageIt = std::find_if(m_document.pipeline.stages.begin(), m_document.pipeline.stages.end(), [&](const ColorGradeStage& stage) {
        return stage.stageId == m_selectedNodeGraphNodeId;
    });
    const auto selectedNodeIt = std::find_if(m_document.nodeGraph.nodes.begin(), m_document.nodeGraph.nodes.end(), [&](const NodeGraphNode& node) {
        return node.nodeId == m_selectedNodeGraphNodeId;
    });
    const bool hasStage = selectedStageIt != m_document.pipeline.stages.end();
    const bool hasNode = selectedNodeIt != m_document.nodeGraph.nodes.end();
    const QString type = hasStage ? selectedStageIt->type : (hasNode ? selectedNodeIt->type : QString());
    ColorGradeStage* stage = hasStage ? &(*selectedStageIt) : nullptr;
    NodeGraphNode* graphNode = hasNode ? &(*selectedNodeIt) : nullptr;
    const QJsonObject params = stage ? stage->params : (graphNode ? graphNode->params : QJsonObject());
    const bool editableNode = !m_selectedNodeGraphNodeId.isEmpty() && (hasStage || hasNode) && type != "input" && type != "output";
    setVisible(m_nodePropertyPanel, editableNode);
    if (!editableNode) {
        m_nodePropertyActiveStageId.clear();
        m_nodePropertyActiveType.clear();
        return;
    }

    if (m_nodePropertyTitle) {
        m_nodePropertyTitle->setText(nodeStageLabelForType(type));
    }

    if (!m_nodePropertyContentLayout) {
        return;
    }
    if (m_nodePropertyActiveStageId == m_selectedNodeGraphNodeId && m_nodePropertyActiveType == type) {
        return;
    }

    for (QWidget* widget : m_nodePropertyDynamicWidgets) {
        m_nodePropertyContentLayout->removeWidget(widget);
        widget->deleteLater();
    }
    m_nodePropertyDynamicWidgets.clear();
    m_nodePropertyActiveStageId = m_selectedNodeGraphNodeId;
    m_nodePropertyActiveType = type;

    auto touchNode = [this]() {
        syncNodeGraphFromPipelineStages();
        if (m_nodeGraphWidget) {
            QSignalBlocker blocker(m_nodeGraphWidget);
            m_nodeGraphWidget->setGraph(m_document.nodeGraph);
        }
        markDirty();
        schedulePreview();
    };
    auto writeParam = [this, touchNode](const QString& key, const QJsonValue& value) {
        auto stageIt = std::find_if(m_document.pipeline.stages.begin(), m_document.pipeline.stages.end(), [&](const ColorGradeStage& item) {
            return item.stageId == m_selectedNodeGraphNodeId;
        });
        if (stageIt != m_document.pipeline.stages.end()) {
            stageIt->params[key] = value;
        }
        auto nodeIt = std::find_if(m_document.nodeGraph.nodes.begin(), m_document.nodeGraph.nodes.end(), [&](const NodeGraphNode& item) {
            return item.nodeId == m_selectedNodeGraphNodeId;
        });
        if (nodeIt != m_document.nodeGraph.nodes.end()) {
            nodeIt->params[key] = value;
        }
        touchNode();
    };
    auto readSelectedParams = [this]() {
        const auto stageIt = std::find_if(m_document.pipeline.stages.begin(), m_document.pipeline.stages.end(), [&](const ColorGradeStage& item) {
            return item.stageId == m_selectedNodeGraphNodeId;
        });
        if (stageIt != m_document.pipeline.stages.end()) {
            return stageIt->params;
        }
        const auto nodeIt = std::find_if(m_document.nodeGraph.nodes.begin(), m_document.nodeGraph.nodes.end(), [&](const NodeGraphNode& item) {
            return item.nodeId == m_selectedNodeGraphNodeId;
        });
        return nodeIt == m_document.nodeGraph.nodes.end() ? QJsonObject() : nodeIt->params;
    };
    auto addWidget = [&](QWidget* widget) {
        m_nodePropertyDynamicWidgets.append(widget);
        m_nodePropertyContentLayout->addWidget(widget);
    };
    auto addSlider = [&](const QString& label,
                         const QString& key,
                         double min,
                         double max,
                         double fallback,
                         double step,
                         int decimals) {
        auto readValue = [&]() {
            if (key.contains('.')) {
                const QStringList parts = key.split('.');
                if (parts.size() == 2) {
                    const QJsonArray arr = params.value(parts.at(0)).toArray();
                    const int index = parts.at(1).toInt();
                    if (index >= 0 && index < arr.size()) {
                        return arr.at(index).toDouble(fallback);
                    }
                }
                return fallback;
            }
            return params.value(key).toDouble(fallback);
        };
        auto* control = new ParameterControl(label, min, max, readValue(), step, decimals, m_nodePropertyPanel);
        connect(control, &ParameterControl::valueChanged, this, [key, fallback, writeParam, readSelectedParams](double v) {
            if (key.contains('.')) {
                const QStringList parts = key.split('.');
                if (parts.size() == 2) {
                    QJsonArray arr = readSelectedParams().value(parts.at(0)).toArray();
                    const int index = parts.at(1).toInt();
                    while (arr.size() <= index) {
                        arr.append(fallback);
                    }
                    arr[index] = v;
                    writeParam(parts.at(0), arr);
                }
            } else {
                writeParam(key, v);
            }
        });
        addWidget(control);
    };
    auto addVectorSlider = [&](const QString& label,
                               const QString& key,
                               int channel,
                               double min,
                               double max,
                               double fallback,
                               double step,
                               int decimals) {
        const QVector3D vector = vectorFromJson(params.value(key), QVector3D(fallback, fallback, fallback));
        const double value = channel == 0 ? vector.x() : channel == 1 ? vector.y() : vector.z();
        auto* control = new ParameterControl(label, min, max, value, step, decimals, m_nodePropertyPanel);
        connect(control, &ParameterControl::valueChanged, this, [key, channel, fallback, writeParam, readSelectedParams](double v) {
            QJsonArray arr = readSelectedParams().value(key).toArray();
            while (arr.size() < 3) {
                arr.append(fallback);
            }
            arr[channel] = v;
            writeParam(key, arr);
        });
        addWidget(control);
    };
    auto addSection = [&](const QString& text) {
        auto* label = new QLabel(text, m_nodePropertyPanel);
        label->setStyleSheet("color: #aeb5c0; font-weight: 600; margin-top: 6px;");
        addWidget(label);
    };

    if (type == "globalColorGrade") {
        addSection("基础");
        addSlider("曝光", "exposure", -5.0, 5.0, 0.0, 0.01, 2);
        addSlider("对比度", "contrast", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("饱和度", "saturation", 0.0, 2.0, 1.0, 0.01, 2);
        addSlider("自然饱和度", "vibrance", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("色温", "temperature", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("色调", "tint", -1.0, 1.0, 0.0, 0.01, 2);
        addSection("明暗");
        addSlider("高光", "highlights", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("阴影", "shadows", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("白场", "whites", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("黑场", "blacks", -1.0, 1.0, 0.0, 0.01, 2);
    } else if (type == "curveGrade") {
        addSection("主曲线");
        auto* label = new QLabel("请在“曲线”页编辑控制点。此节点面板提供柔和裁切参数，便于快速调整节点图。", m_nodePropertyPanel);
        label->setWordWrap(true);
        label->setStyleSheet("color: #9fa7b3;");
        addWidget(label);
        addSlider("低端", "softClipLow", 0.0, 0.45, 0.0, 0.01, 2);
        addSlider("高端", "softClipHigh", 0.55, 1.0, 1.0, 0.01, 2);
        addSlider("低端柔和", "softClipLowSoftness", 0.0, 1.0, 0.0, 0.01, 2);
        addSlider("高端柔和", "softClipHighSoftness", 0.0, 1.0, 0.0, 0.01, 2);
    } else if (type == "hslGrade") {
        addSection("色相分区");
        for (int i = 0; i < 8; ++i) {
            const QString band = QString::fromUtf8(kHslBands[i]);
            addSlider(band + " 色相", QString("hslHue.%1").arg(i), -180.0, 180.0, 0.0, 1.0, 0);
            addSlider(band + " 饱和", QString("hslSaturation.%1").arg(i), -1.0, 1.0, 0.0, 0.01, 2);
        }
    } else if (type == "proGrade") {
        addSection("Log 色轮");
        addSlider("阴影", "logShadow", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("暗部", "logDark", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("亮部", "logLight", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("高光", "logHighlight", -1.0, 1.0, 0.0, 0.01, 2);
        addSection("印片灯");
        addSlider("红", "printerRed", -12.0, 12.0, 0.0, 1.0, 0);
        addSlider("绿", "printerGreen", -12.0, 12.0, 0.0, 1.0, 0);
        addSlider("蓝", "printerBlue", -12.0, 12.0, 0.0, 1.0, 0);
    } else if (type == "mixColor") {
        auto* combo = new NoWheelComboBox(m_nodePropertyPanel);
        const QVector<QPair<QString, QString>> mixModes = {
            {"混合", "mix"}, {"变暗", "darken"}, {"正片叠底", "multiply"}, {"颜色加深", "color-burn"},
            {"变亮", "lighten"}, {"滤色", "screen"}, {"颜色减淡", "color-dodge"}, {"添加", "add"},
            {"叠加", "overlay"}, {"柔光", "soft-light"}, {"线性光", "linear-light"},
            {"差值", "difference"}, {"排除", "exclusion"}, {"减去", "subtract"},
            {"相除", "divide"}, {"色相", "hue"}, {"饱和度", "saturation"}, {"颜色", "color"}, {"明度", "value"},
        };
        for (const auto& mode : mixModes) {
            combo->addItem(mode.first, mode.second);
        }
        const int index = combo->findData(params.value("blendType").toString("mix"));
        combo->setCurrentIndex(index < 0 ? 0 : index);
        connect(combo, &QComboBox::currentIndexChanged, this, [combo, writeParam]() {
            writeParam("blendType", combo->currentData().toString());
        });
        addWidget(combo);
        auto* clamp = new QCheckBox("限制结果范围", m_nodePropertyPanel);
        clamp->setChecked(params.value("clampResult").toBool(true));
        connect(clamp, &QCheckBox::toggled, this, [writeParam](bool checked) {
            writeParam("clampResult", checked);
        });
        addWidget(clamp);
        addSlider("强度", "factor", 0.0, 1.0, 1.0, 0.01, 2);
        addSection("颜色 B");
        addVectorSlider("R", "colorB", 0, 0.0, 1.0, 1.0, 0.01, 2);
        addVectorSlider("G", "colorB", 1, 0.0, 1.0, 1.0, 0.01, 2);
        addVectorSlider("B", "colorB", 2, 0.0, 1.0, 1.0, 0.01, 2);
    } else if (type == "parallelMixer" || type == "layerMixer") {
        addSection(type == "parallelMixer" ? "并行混合器" : "图层混合器");
        addSlider("混合强度", "mix", 0.0, 1.0, 1.0, 0.01, 2);
        addSection("分支权重");
        addSlider("输入 1", "weights.0", 0.0, 4.0, 1.0, 0.01, 2);
        addSlider("输入 2", "weights.1", 0.0, 4.0, 1.0, 0.01, 2);
        addSlider("输入 3", "weights.2", 0.0, 4.0, 1.0, 0.01, 2);
    } else if (type == "brightnessContrast") {
        addSlider("亮度", "brightness", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("对比度", "contrast", -1.0, 1.0, 0.0, 0.01, 2);
    } else if (type == "hueSaturationValue") {
        addSlider("色相", "hue", 0.0, 1.0, 0.5, 0.01, 2);
        addSlider("饱和度", "saturation", 0.0, 2.0, 1.0, 0.01, 2);
        addSlider("明度", "value", 0.0, 2.0, 1.0, 0.01, 2);
    } else if (type == "gammaNode") {
        addSlider("伽马", "gamma", 0.05, 4.0, 1.0, 0.01, 2);
    } else if (type == "colorBalance") {
        addSection("提升");
        addVectorSlider("R", "lift", 0, -1.0, 1.0, 0.0, 0.01, 2);
        addVectorSlider("G", "lift", 1, -1.0, 1.0, 0.0, 0.01, 2);
        addVectorSlider("B", "lift", 2, -1.0, 1.0, 0.0, 0.01, 2);
        addSection("伽马");
        addVectorSlider("R", "gamma", 0, 0.05, 4.0, 1.0, 0.01, 2);
        addVectorSlider("G", "gamma", 1, 0.05, 4.0, 1.0, 0.01, 2);
        addVectorSlider("B", "gamma", 2, 0.05, 4.0, 1.0, 0.01, 2);
        addSection("增益");
        addVectorSlider("R", "gain", 0, 0.0, 4.0, 1.0, 0.01, 2);
        addVectorSlider("G", "gain", 1, 0.0, 4.0, 1.0, 0.01, 2);
        addVectorSlider("B", "gain", 2, 0.0, 4.0, 1.0, 0.01, 2);
    } else if (type == "rgbCurves") {
        addSection("RGB 曲线");
        addSlider("主中点", "masterMid", 0.0, 1.0, 0.5, 0.01, 2);
        addSlider("红中点", "redMid", 0.0, 1.0, 0.5, 0.01, 2);
        addSlider("绿中点", "greenMid", 0.0, 1.0, 0.5, 0.01, 2);
        addSlider("蓝中点", "blueMid", 0.0, 1.0, 0.5, 0.01, 2);
    } else if (type == "colorCorrection") {
        auto addCorrectionGroup = [&](const QString& title, const QString& prefix) {
            addSection(title);
            addSlider("色相", prefix + "Hue", -180.0, 180.0, prefix == "master" ? 0.0 : 0.0, 1.0, 0);
            addSlider("饱和度", prefix + "Saturation", prefix == "master" ? 0.0 : -1.0, prefix == "master" ? 2.0 : 1.0, prefix == "master" ? 1.0 : 0.0, 0.01, 2);
            addSlider("明度", prefix + "Value", prefix == "master" ? 0.0 : -1.0, prefix == "master" ? 2.0 : 1.0, prefix == "master" ? 1.0 : 0.0, 0.01, 2);
            addSlider("对比度", prefix + "Contrast", -1.0, 1.0, 0.0, 0.01, 2);
            addSlider("伽马", prefix + "Gamma", prefix == "master" ? 0.05 : -2.0, prefix == "master" ? 4.0 : 2.0, prefix == "master" ? 1.0 : 0.0, 0.01, 2);
        };
        addCorrectionGroup("主控", "master");
        addCorrectionGroup("阴影", "shadows");
        addCorrectionGroup("中间调", "midtones");
        addCorrectionGroup("高光", "highlights");
    } else if (type == "invertColor") {
        addSlider("强度", "factor", 0.0, 1.0, 1.0, 0.01, 2);
    } else if (type == "posterizeColor") {
        addSlider("级数", "steps", 2.0, 64.0, 8.0, 1.0, 0);
    } else if (type == "clampColor") {
        addSlider("最小值", "minimum", 0.0, 1.0, 0.0, 0.01, 2);
        addSlider("最大值", "maximum", 0.0, 1.0, 1.0, 0.01, 2);
    } else if (type == "levelsColor") {
        addSlider("黑点", "blackPoint", 0.0, 1.0, 0.0, 0.01, 2);
        addSlider("白点", "whitePoint", 0.0, 1.0, 1.0, 0.01, 2);
        addSlider("伽马", "gamma", 0.05, 4.0, 1.0, 0.01, 2);
        addSlider("输出黑场", "outputBlack", 0.0, 1.0, 0.0, 0.01, 2);
        addSlider("输出白场", "outputWhite", 0.0, 1.0, 1.0, 0.01, 2);
    } else if (type == "exposureColor") {
        addSlider("曝光", "exposure", -8.0, 8.0, 0.0, 0.01, 2);
        addSlider("偏移", "offset", -1.0, 1.0, 0.0, 0.01, 2);
    } else if (type == "thresholdColor") {
        addSlider("阈值", "threshold", 0.0, 1.0, 0.5, 0.01, 2);
        addSlider("柔和度", "softness", 0.0, 0.5, 0.0, 0.01, 2);
    } else if (type == "sepiaColor") {
        addSlider("强度", "factor", 0.0, 1.0, 1.0, 0.01, 2);
    } else if (type == "channelMixer") {
        addSection("红色输出");
        addVectorSlider("R", "red", 0, -2.0, 2.0, 1.0, 0.01, 2);
        addVectorSlider("G", "red", 1, -2.0, 2.0, 0.0, 0.01, 2);
        addVectorSlider("B", "red", 2, -2.0, 2.0, 0.0, 0.01, 2);
        addSection("绿色输出");
        addVectorSlider("R", "green", 0, -2.0, 2.0, 0.0, 0.01, 2);
        addVectorSlider("G", "green", 1, -2.0, 2.0, 1.0, 0.01, 2);
        addVectorSlider("B", "green", 2, -2.0, 2.0, 0.0, 0.01, 2);
        addSection("蓝色输出");
        addVectorSlider("R", "blue", 0, -2.0, 2.0, 0.0, 0.01, 2);
        addVectorSlider("G", "blue", 1, -2.0, 2.0, 0.0, 0.01, 2);
        addVectorSlider("B", "blue", 2, -2.0, 2.0, 1.0, 0.01, 2);
    } else if (type == "temperatureTint") {
        addSlider("色温", "temperature", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("色调", "tint", -1.0, 1.0, 0.0, 0.01, 2);
    } else if (type == "grayscaleColor") {
        addSlider("强度", "factor", 0.0, 1.0, 1.0, 0.01, 2);
    } else if (type == "vibranceColor") {
        addSlider("饱和度", "saturation", 0.0, 2.0, 1.0, 0.01, 2);
        addSlider("自然饱和度", "vibrance", -1.0, 1.0, 0.0, 0.01, 2);
    } else if (type == "softClipColor") {
        addSlider("低端", "low", 0.0, 0.45, 0.0, 0.01, 2);
        addSlider("高端", "high", 0.55, 1.0, 1.0, 0.01, 2);
        addSlider("低端柔和", "lowSoftness", 0.0, 1.0, 0.0, 0.01, 2);
        addSlider("高端柔和", "highSoftness", 0.0, 1.0, 0.0, 0.01, 2);
    } else if (type == "duotoneColor") {
        addSlider("强度", "factor", 0.0, 1.0, 1.0, 0.01, 2);
        addSection("阴影颜色");
        addVectorSlider("R", "shadowColor", 0, 0.0, 1.0, 0.05, 0.01, 2);
        addVectorSlider("G", "shadowColor", 1, 0.0, 1.0, 0.05, 0.01, 2);
        addVectorSlider("B", "shadowColor", 2, 0.0, 1.0, 0.08, 0.01, 2);
        addSection("高光颜色");
        addVectorSlider("R", "highlightColor", 0, 0.0, 1.0, 1.0, 0.01, 2);
        addVectorSlider("G", "highlightColor", 1, 0.0, 1.0, 0.92, 0.01, 2);
        addVectorSlider("B", "highlightColor", 2, 0.0, 1.0, 0.78, 0.01, 2);
    } else if (type == "ascCdl") {
        addSection("斜率");
        addVectorSlider("R", "slope", 0, 0.0, 4.0, 1.0, 0.01, 2);
        addVectorSlider("G", "slope", 1, 0.0, 4.0, 1.0, 0.01, 2);
        addVectorSlider("B", "slope", 2, 0.0, 4.0, 1.0, 0.01, 2);
        addSection("偏移");
        addVectorSlider("R", "offset", 0, -1.0, 1.0, 0.0, 0.01, 2);
        addVectorSlider("G", "offset", 1, -1.0, 1.0, 0.0, 0.01, 2);
        addVectorSlider("B", "offset", 2, -1.0, 1.0, 0.0, 0.01, 2);
        addSection("幂次");
        addVectorSlider("R", "power", 0, 0.05, 4.0, 1.0, 0.01, 2);
        addVectorSlider("G", "power", 1, 0.05, 4.0, 1.0, 0.01, 2);
        addVectorSlider("B", "power", 2, 0.05, 4.0, 1.0, 0.01, 2);
        addSlider("饱和度", "saturation", 0.0, 2.0, 1.0, 0.01, 2);
    } else if (type == "colorRamp") {
        addSlider("黑点", "blackPoint", 0.0, 1.0, 0.0, 0.01, 2);
        addSlider("白点", "whitePoint", 0.0, 1.0, 1.0, 0.01, 2);
        addSection("阴影颜色");
        addVectorSlider("R", "shadowColor", 0, 0.0, 1.0, 0.0, 0.01, 2);
        addVectorSlider("G", "shadowColor", 1, 0.0, 1.0, 0.0, 0.01, 2);
        addVectorSlider("B", "shadowColor", 2, 0.0, 1.0, 0.0, 0.01, 2);
        addSection("高光颜色");
        addVectorSlider("R", "highlightColor", 0, 0.0, 1.0, 1.0, 0.01, 2);
        addVectorSlider("G", "highlightColor", 1, 0.0, 1.0, 1.0, 0.01, 2);
        addVectorSlider("B", "highlightColor", 2, 0.0, 1.0, 1.0, 0.01, 2);
    } else if (type == "selectiveColor") {
        addSlider("目标色相", "targetHue", 0.0, 1.0, 0.0, 0.01, 2);
        addSlider("范围", "range", 0.02, 0.5, 0.12, 0.01, 2);
        addSlider("色相偏移", "hue", -180.0, 180.0, 0.0, 1.0, 0);
        addSlider("饱和度", "saturation", -1.0, 1.0, 0.0, 0.01, 2);
        addSlider("亮度", "luminance", -1.0, 1.0, 0.0, 0.01, 2);
    } else if (type == "lutMix") {
        addSlider("强度", "strength", 0.0, 1.0, 1.0, 0.01, 2);
    }
}

void MainWindow::syncStageParams()
{
    if (m_document.pipeline.stages.isEmpty()) {
        rebuildDefaultStages();
    }
    syncActiveStageParams();
}

Lut3D MainWindow::currentLutForExport() const
{
    ColorTransformSource source = m_transformSource;
    source.params = m_document.pipeline.params;
    source.pipeline = pipelineFromNodeGraph();
    source.masks = m_document.masks;
    source.usePipeline = true;
    source.importedLutStrength = std::clamp(m_document.pipeline.params.importedLutStrength, 0.0f, 1.0f);
    if (m_adjustmentsBypassed) {
        ColorGradeParams baseParams;
        baseParams.inputColorSpace = m_document.pipeline.params.inputColorSpace;
        source.params = baseParams;
        source.pipeline = ColorGradePipeline();
        source.pipeline.params = baseParams;
        source.masks.clear();
        source.importedLutStrength = 0.0f;
    }
    source.kind = m_importedLut.isValid() && source.importedLutStrength > 0.0f
        ? ColorTransformKind::ImportedLut
        : ColorTransformKind::ParamPipeline;
    return IkClutExporter::resolveExportLut(source, m_document.pipeline.params, m_importedLut);
}

} // namespace ikclut
