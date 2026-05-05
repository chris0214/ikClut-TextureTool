#pragma once

#include "core/Lut3D.h"

#include <QJsonObject>
#include <QString>

namespace ikclut {

struct LutImportResult {
    bool ok = false;
    QString error;
    QString format;
    QJsonObject metadata;
    Lut3D lut;
};

class LutImportService {
public:
    static LutImportResult importFile(const QString& path);
    static LutImportResult importCube(const QString& path);
    static LutImportResult importHaldPng(const QString& path);
};

} // namespace ikclut

