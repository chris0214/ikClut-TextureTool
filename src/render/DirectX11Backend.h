#pragma once

#include <QString>

namespace ikclut {

class DirectX11Backend {
public:
    static bool probe(QString* details);
};

} // namespace ikclut

