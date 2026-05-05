#include "render/DirectX11Backend.h"

#ifdef _WIN32
#include <d3d11.h>
#endif

namespace ikclut {

bool DirectX11Backend::probe(QString* details)
{
#ifdef _WIN32
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL selectedLevel = D3D_FEATURE_LEVEL_10_0;
    const HRESULT hr = D3D11CreateDevice(nullptr,
                                         D3D_DRIVER_TYPE_HARDWARE,
                                         nullptr,
                                         0,
                                         featureLevels,
                                         ARRAYSIZE(featureLevels),
                                         D3D11_SDK_VERSION,
                                         &device,
                                         &selectedLevel,
                                         &context);
    if (context) {
        context->Release();
    }
    if (device) {
        device->Release();
    }
    if (SUCCEEDED(hr)) {
        if (details) {
            *details = QString("DirectX 11 device available, feature level 0x%1.").arg(static_cast<unsigned int>(selectedLevel), 0, 16);
        }
        return true;
    }
    if (details) {
        *details = QString("DirectX 11 probe failed with HRESULT 0x%1.").arg(static_cast<unsigned int>(hr), 0, 16);
    }
    return false;
#else
    if (details) {
        *details = "DirectX 11 is only available on Windows.";
    }
    return false;
#endif
}

} // namespace ikclut

