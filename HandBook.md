# IKCLUT Studio HandBook

Author: 克里斯提亚娜

License: Apache-2.0

## 1. 软件定位

IKCLUT Studio 是一个 Windows 桌面调色与 LUT 导出工具。它的目标不是做完整的视频剪辑软件，而是提供一个轻量的图像调色工作台：载入参考图像，调整色彩，预览效果，再导出给 MMD/MME、图像处理或其他支持 LUT 的流程使用。

当前版本定位为开源 beta。核心调色、LUT 导入导出、项目保存、预览、示波器、3D LUT 查看和节点式调色流程已经可用；局部遮罩编辑因为交互不够可靠，已从当前 beta 移除。

## 2. 主要功能

- 图像载入与前后对比预览。
- 基础调色：曝光、对比、饱和度、自然饱和度、色温、色调、高光、阴影、白场、黑场等。
- 曲线调色：Master/R/G/B 曲线，以及 Hue-vs-Hue、Hue-vs-Saturation、Hue-vs-Luminance 曲线。
- HSL 八色段调整。
- 色彩变形工具，通过源点和目标点映射改变局部色相、饱和度和亮度关系。
- 专业调色工具：Lift/Gamma/Gain/Offset、Log/HDR 分区、Printer Lights、肤色工具、校准、Soft Clip。
- LUT 导入：支持 3D CUBE、纯 1D CUBE 烘焙为 3D LUT、Hald PNG。
- LUT 导出：支持 ikClut PNG、CUBE、Hald PNG。
- 批量图像导出。
- 项目和预设保存。
- 节点图：支持串行节点、Mix Color、Parallel Mixer、Layer Mixer 等实用流程。
- 质量检查：LUT 检查、示波器、假色/裁切/色域提示、3D LUT 查看。

## 3. 使用流程

1. 打开 `IkClutStudio.exe`。
2. 通过菜单或工具栏打开图像。
3. 在右侧面板调整基础、曲线、HSL、色轮、节点或专业参数。
4. 可选：导入外部 `.cube` 或 Hald PNG LUT，并设置混合强度。
5. 在预览区查看结果，也可以切换前后对比、快照 A/B、示波器和 3D LUT。
6. 导出为 ikClut PNG、CUBE、Hald PNG，或批量套用到图像。
7. 如需继续编辑，保存项目或预设。

## 4. 工程结构

项目使用 C++20、Qt 6 和 CMake 构建。

```text
src/
  main.cpp
  MainWindow.cpp / MainWindow.h
  core/
    ColorPipeline.*
    Lut3D.*
    LutImportService.*
    IkClutExporter.*
    ProjectSerializer.*
    ExecutionPlan.*
    LutQualityAnalyzer.*
  render/
    PreviewRenderer.*
    DirectX11PreviewRenderer.*
    ScopeRenderer.*
  ui/
    ImageView.*
    CurveWidget.*
    ColorWheelControl.*
    ColorWarperWidget.*
    NodeGraphWidget.*
    ScopeWidget.*
    Lut3DViewer.*
  model/
    ColorTypes.h
tests/
  core_tests.cpp
translations/
  IkClutStudio_zh_CN.ts
```

`MainWindow` 负责把 UI、项目状态、预览调度和导出流程串起来。`core` 目录负责调色数据结构、颜色计算、LUT 导入导出和项目序列化。`render` 目录负责预览渲染、GPU 后端和示波器数据。`ui` 目录是自定义 Qt 控件。

## 5. 核心数据模型

主要数据结构定义在 `src/model/ColorTypes.h`。

- `ColorGradeParams`：全局调色参数，包括曝光、曲线、HSL、色轮、HDR/Log、肤色、Soft Clip、色彩变形等。
- `ColorGradeStage`：调整堆栈中的一个图层/阶段，包含类型、启用状态、不透明度、混合模式和参数 JSON。
- `ColorGradePipeline`：由多个 `ColorGradeStage` 和一份全局 `ColorGradeParams` 组成。
- `ImportedLutAsset`：外部 LUT 资源记录。
- `NodeGraphNode`、`NodeGraphEdge`、`NodeGraph`：节点图的节点、连接线和整体图结构。
- `ImageDocument`：一个工程文件的完整状态，包括图像路径、调色管线、导入 LUT、节点图等。
- `ColorTransformSource`：预览和导出时使用的统一变换描述。

项目文件和预设本质上是这些结构的 JSON 表示。

## 6. 调色原理

软件的核心思想是：把用户的调色参数转换成颜色变换函数，再把这个函数应用到图像或烘焙成 3D LUT。

核心入口在 `ColorPipeline`：

- `applyParams(color, params)`：对单个 RGB 颜色应用一组调色参数。
- `applyPipeline(color, pipeline)`：按阶段应用一条调色管线。
- `bakeLut(params, size)`：把参数烘焙成指定大小的 3D LUT。
- `composeLut(params, importedLut, strength, size)`：把参数调色和导入 LUT 混合后烘焙。
- `applyToImage(input, source, importedLut)`：把变换应用到整张图像。

3D LUT 可以理解为一个三维颜色查找表。输入颜色 `(r, g, b)` 被看作一个三维坐标，在 LUT 的颜色立方体中采样，得到输出颜色。这样复杂的调色运算可以提前烘焙为固定表格，运行时只需要采样，速度更快，也更容易被其他软件使用。

## 7. LUT 格式

### ikClut PNG

ikClut 使用固定尺寸：

- LUT 尺寸：`32 x 32 x 32`
- PNG 宽度：`1024`
- PNG 高度：`32`

它把 3D 颜色立方体展开成二维 PNG。`IkClutExporter::makeImage()` 会按固定通道布局写入像素，`validateImage()` 会检查尺寸和 identity corner。

### CUBE

CUBE 是常见 LUT 文本格式。当前支持：

- 标准 3D CUBE。
- 纯 1D CUBE，并将其烘焙为等效 3D LUT。

当前不支持混合 1D+3D CUBE。遇到这种文件会明确拒绝，而不是静默导入错误结果。

### Hald PNG

Hald PNG 是另一种把 3D LUT 展开到二维图像的方式。软件支持导入和导出 Hald PNG，并通过测试覆盖基本往返。

## 8. 预览渲染

预览渲染由 `PreviewRenderer` 处理。它接收 `PreviewRenderRequest`，输出 `PreviewRenderResult`。

预览分两种质量：

- `Interactive`：交互时使用，会按 `maxInteractiveSize` 缩小图像，提高响应速度。
- `Full`：完整质量预览，保持原图尺寸。

渲染流程大致是：

1. 读取当前 `ColorTransformSource`。
2. 解析是否需要参数管线、节点管线或导入 LUT。
3. 生成预览 LUT 和导出 LUT。
4. 优先尝试 DirectX 11 GPU 预览。
5. 如果 GPU 不可用或失败，回退到 CPU 渲染。
6. 返回渲染耗时、后端信息和诊断文本。

这种设计让 UI 能快速响应，同时保持导出结果和完整预览尽量一致。

## 9. 节点系统

节点系统由两部分组成：

- `NodeGraphWidget`：负责节点画布、端口、连线、拖拽、缩放和平移等 UI 交互。
- `pipelineFromNodeGraph()` 和 `ExecutionPlanCompiler`：负责把节点图转换成可执行调色管线和诊断信息。

当前节点图支持实用型流程，而不是完全通用的 Blender 级节点引擎。它支持：

- 从输出拖到输入，也支持从输入反向拖到输出。
- 多输入节点的独立端口连接。
- 串行调色节点。
- Mix Color 的 A/B 分支。
- Parallel Mixer / Layer Mixer 的多分支混合。
- 节点诊断：循环、输出未连接等情况会显示提示。

节点图最终仍会转换为 `ColorGradePipeline`。因此节点系统的核心作用是用可视化方式组织调色阶段，而不是执行任意类型的数据流。

## 10. 项目与预设

项目保存由 `ProjectSerializer` 实现。工程文件保存 `ImageDocument`：

- 图像路径。
- 调色参数。
- 调整阶段。
- 导入 LUT 资源路径。
- 节点图。
- 其他扩展字段。

预设保存的是 `ColorGradePipeline`，适合复用某一套调色参数，而不绑定具体图像。

需要注意：资源多以路径方式记录。如果外部图像或 LUT 文件移动，旧项目可能需要重新指定资源路径。

## 11. 示波器和分析工具

`ScopeRenderer` 会从图像中计算分析数据，例如：

- RGB/亮度分布。
- Vectorscope。
- 曝光区间统计。

`LutQualityAnalyzer` 用来检查 LUT 是否存在明显问题，例如严重裁切、异常范围、空间遮罩无法完整烘焙等。当前 beta 已移除局部遮罩 UI，但底层结构仍保留历史兼容字段。

## 12. 构建方式

依赖：

- Windows
- CMake 3.25 或更新
- C++20 编译器
- Qt 6：Core、Gui、Widgets、Concurrent、LinguistTools

常规构建：

```powershell
cmake -S . -B build
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

如果要生成可分发 Windows 运行目录，需要将 Qt 运行库和插件部署到 exe 同目录。可以使用 Qt 的 `windeployqt`，或者在 CMake 中启用 `IKCLUT_ENABLE_QT_DEPLOY`。

## 13. 发布包说明

当前 release 通常包含三类文件：

- `IkClutStudio-0.1.0-beta-source.zip`：干净源码包。
- `IkClutStudio-0.1.0-beta-win64-portable.zip`：便携版，解压后运行 `IkClutStudio.exe`。
- `IkClutStudio-0.1.0-beta-win64.exe`：单文件自解压启动包。

单文件 EXE 不是静态链接的纯单文件程序。它是一个自解压启动器，会把便携版展开到：

```text
%LOCALAPPDATA%\IkClutStudio\0.1.0-beta
```

然后启动真正的 Qt 程序。这样做可以保持 Qt 动态链接运行库的可替换性，也更适合 LGPL 运行库的分发方式。

## 14. 第三方组件

二进制包包含 Qt 6 动态库和插件，以及 MinGW 运行库 DLL。项目本体使用 Apache-2.0 发布，但第三方运行库遵循各自协议。详情见：

- `LICENSE`
- `THIRD_PARTY_NOTICES.md`

## 15. 已知限制

- 当前 beta 移除了局部遮罩编辑。
- 混合 1D+3D CUBE 文件不支持。
- 节点图不是完全通用的类型化 DAG 引擎。
- 主窗口代码仍较大，后续可以拆分为更小的控制器和面板模块。
- 当前没有 CI。
- 当前没有安装器。

## 16. 后续改进方向

- 继续改善节点画布，加入更完整的 Blender 式节点体验。
- 拆分 `MainWindow`，降低 UI 和业务逻辑耦合。
- 增加更多真实 CUBE/LUT 样本测试。
- 增加 UI 自动化或冒烟测试。
- 增加 GitHub Actions 或其他 CI。
- 增加更完整的用户教程和截图。
- 重新设计局部遮罩，使其达到可发布质量后再恢复。
