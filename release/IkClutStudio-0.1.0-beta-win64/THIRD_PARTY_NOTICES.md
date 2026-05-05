# Third-Party Notices

This project is released under Apache-2.0 by 克里斯提亚娜. The binary package also includes third-party runtime components needed to run the application on Windows.

## Qt

The application uses Qt 6 dynamically linked runtime libraries and plugins.

Qt is available under commercial licenses and open-source licenses, including LGPLv3. See:

- https://www.qt.io/licensing/
- https://doc.qt.io/qt-6/lgpl.html
- https://doc.qt.io/qt-6/windows-deployment.html

The binary package keeps Qt as separate dynamic libraries and plugins so users can inspect or replace those runtime files with compatible versions.

## MinGW Runtime

The Windows binary package may include MinGW runtime DLLs such as `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, and `libwinpthread-1.dll`, as provided by the compiler toolchain used to build this release.

## Windows / DirectX

The application uses Windows and DirectX system APIs when available. Those platform components are provided by Windows and are not bundled as part of this project.
