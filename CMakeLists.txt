cmake_minimum_required(VERSION 3.25)

project(IkClutStudio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent LinguistTools)

option(IKCLUT_ENABLE_QT_DEPLOY "Run windeployqt after building the app" OFF)

add_library(ikclut_core STATIC
    src/core/ColorPipeline.cpp
    src/core/ExecutionPlan.cpp
    src/core/IkClutExporter.cpp
    src/core/Lut3D.cpp
    src/core/LutImportService.cpp
    src/core/LutQualityAnalyzer.cpp
    src/core/MaskEvaluator.cpp
    src/core/ProjectSerializer.cpp
    src/render/DirectX11Backend.cpp
    src/render/DirectX11PreviewRenderer.cpp
    src/render/PreviewRenderer.cpp
    src/render/ScopeRenderer.cpp
)

target_include_directories(ikclut_core PUBLIC src)
target_link_libraries(ikclut_core PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent)

foreach(target ikclut_core)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
endforeach()

if(WIN32)
    target_compile_definitions(ikclut_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(ikclut_core PUBLIC d3d11 dxgi d3dcompiler)
endif()

qt_add_executable(IkClutStudio WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/ui/ColorWheelControl.cpp
    src/ui/ColorWarperWidget.cpp
    src/ui/CollapsiblePanel.cpp
    src/ui/CurveWidget.cpp
    src/ui/ImageView.cpp
    src/ui/Lut3DViewer.cpp
    src/ui/NodeGraphWidget.cpp
    src/ui/ParameterControl.cpp
    src/ui/ResponsiveTabPanel.cpp
    src/ui/ScopeWidget.cpp
)

target_link_libraries(IkClutStudio PRIVATE ikclut_core Qt6::Widgets)

qt_add_translations(IkClutStudio
    TS_FILES
        translations/IkClutStudio_zh_CN.ts
    RESOURCE_PREFIX
        "/i18n"
)

if(MSVC)
    target_compile_options(IkClutStudio PRIVATE /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(IkClutStudio PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

if(WIN32 AND IKCLUT_ENABLE_QT_DEPLOY)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET IkClutStudio POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-system-d3d-compiler --dir "$<TARGET_FILE_DIR:IkClutStudio>" "$<TARGET_FILE:IkClutStudio>"
            COMMENT "Deploying Qt runtime with windeployqt")
    endif()
endif()

enable_testing()

add_executable(IkClutStudioTests
    tests/core_tests.cpp
)

target_link_libraries(IkClutStudioTests PRIVATE ikclut_core)

if(MSVC)
    target_compile_options(IkClutStudioTests PRIVATE /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(IkClutStudioTests PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

add_test(NAME IkClutStudioCore COMMAND IkClutStudioTests)
