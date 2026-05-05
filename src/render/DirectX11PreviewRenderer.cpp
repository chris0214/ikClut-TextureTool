#include "render/DirectX11PreviewRenderer.h"

#ifdef _WIN32
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#endif

#include <cstring>
#include <mutex>
#include <vector>
#include <QElapsedTimer>

namespace ikclut {

#ifdef _WIN32
namespace {

template <typename T>
void release(T*& ptr)
{
    if (ptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

class DxPreviewBackend {
public:
    ~DxPreviewBackend()
    {
        release(m_stagingTexture);
        release(m_outputRtv);
        release(m_outputTexture);
        release(m_lutSrv);
        release(m_lutTexture);
        release(m_inputSrv);
        release(m_inputTexture);
        release(m_lutSampler);
        release(m_inputSampler);
        release(m_pixelShader);
        release(m_vertexShader);
        release(m_context);
        release(m_device);
    }

    bool render(const QImage& source, const Lut3D& lut, QImage* output, QString* error, QString* diagnostic)
    {
        QElapsedTimer timer;
        timer.start();
        std::lock_guard<std::mutex> guard(m_mutex);
        if (!ensure(error)) {
            return false;
        }
        if (source.isNull() || !lut.isValid()) {
            setError(error, "DirectX preview received invalid input.");
            return false;
        }

        QImage src = source.convertToFormat(QImage::Format_RGBA8888);
        const qint64 convertMs = timer.elapsed();

        if (!ensureFrameResources(src.size(), error) || !ensureLutResources(lut.size(), error)) {
            return false;
        }
        const qint64 ensureMs = timer.elapsed() - convertMs;

        m_context->UpdateSubresource(m_inputTexture, 0, nullptr, src.constBits(), static_cast<UINT>(src.bytesPerLine()), 0);
        updateLutTexture(lut);
        const qint64 uploadMs = timer.elapsed() - convertMs - ensureMs;

        const float clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        m_context->ClearRenderTargetView(m_outputRtv, clear);
        m_context->OMSetRenderTargets(1, &m_outputRtv, nullptr);

        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(src.width());
        viewport.Height = static_cast<float>(src.height());
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &viewport);

        ID3D11ShaderResourceView* srvs[2] = {m_inputSrv, m_lutSrv};
        m_context->VSSetShader(m_vertexShader, nullptr, 0);
        m_context->PSSetShader(m_pixelShader, nullptr, 0);
        ID3D11SamplerState* samplers[2] = {m_inputSampler, m_lutSampler};
        m_context->PSSetShaderResources(0, 2, srvs);
        m_context->PSSetSamplers(0, 2, samplers);
        m_context->IASetInputLayout(nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->Draw(3, 0);

        ID3D11ShaderResourceView* nullSrvs[2] = {nullptr, nullptr};
        m_context->PSSetShaderResources(0, 2, nullSrvs);
        m_context->CopyResource(m_stagingTexture, m_outputTexture);
        const qint64 drawMs = timer.elapsed() - convertMs - ensureMs - uploadMs;

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        HRESULT hr = m_context->Map(m_stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) {
            setError(error, QString("Map staging texture failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }
        const qint64 mapMs = timer.elapsed() - convertMs - ensureMs - uploadMs - drawMs;

        QImage image(src.size(), QImage::Format_RGBA8888);
        for (int y = 0; y < image.height(); ++y) {
            std::memcpy(image.scanLine(y),
                        static_cast<const char*>(mapped.pData) + mapped.RowPitch * y,
                        static_cast<size_t>(image.width() * 4));
        }
        m_context->Unmap(m_stagingTexture, 0);
        const qint64 copyMs = timer.elapsed() - convertMs - ensureMs - uploadMs - drawMs - mapMs;

        *output = image;
        if (diagnostic) {
            *diagnostic = QString("DirectX 11 preview | convert %1 ms, ensure %2 ms, upload %3 ms, draw/copyres %4 ms, map %5 ms, cpu-copy %6 ms")
                              .arg(convertMs)
                              .arg(ensureMs)
                              .arg(uploadMs)
                              .arg(drawMs)
                              .arg(mapMs)
                              .arg(copyMs);
        }
        return true;
    }

private:
    bool ensure(QString* error)
    {
        if (m_device && m_context && m_vertexShader && m_pixelShader && m_inputSampler && m_lutSampler) {
            return true;
        }

        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };
        D3D_FEATURE_LEVEL selectedLevel = D3D_FEATURE_LEVEL_10_0;
        HRESULT hr = D3D11CreateDevice(nullptr,
                                       D3D_DRIVER_TYPE_HARDWARE,
                                       nullptr,
                                       0,
                                       featureLevels,
                                       ARRAYSIZE(featureLevels),
                                       D3D11_SDK_VERSION,
                                       &m_device,
                                       &selectedLevel,
                                       &m_context);
        if (FAILED(hr)) {
            setError(error, QString("D3D11CreateDevice failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }

        if (!compileShaders(error)) {
            return false;
        }

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = m_device->CreateSamplerState(&samplerDesc, &m_inputSampler);
        if (FAILED(hr)) {
            setError(error, QString("Create input sampler failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        hr = m_device->CreateSamplerState(&samplerDesc, &m_lutSampler);
        if (FAILED(hr)) {
            setError(error, QString("Create LUT sampler failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }

        return true;
    }

    bool ensureFrameResources(const QSize& size, QString* error)
    {
        if (m_inputTexture && m_outputTexture && m_outputRtv && m_stagingTexture && m_inputSrv && m_frameSize == size) {
            return true;
        }

        release(m_stagingTexture);
        release(m_outputRtv);
        release(m_outputTexture);
        release(m_inputSrv);
        release(m_inputTexture);
        m_frameSize = QSize();

        D3D11_TEXTURE2D_DESC inputDesc = {};
        inputDesc.Width = static_cast<UINT>(size.width());
        inputDesc.Height = static_cast<UINT>(size.height());
        inputDesc.MipLevels = 1;
        inputDesc.ArraySize = 1;
        inputDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        inputDesc.SampleDesc.Count = 1;
        inputDesc.Usage = D3D11_USAGE_DEFAULT;
        inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = m_device->CreateTexture2D(&inputDesc, nullptr, &m_inputTexture);
        if (FAILED(hr)) {
            setError(error, QString("Create cached input texture failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }
        hr = m_device->CreateShaderResourceView(m_inputTexture, nullptr, &m_inputSrv);
        if (FAILED(hr)) {
            setError(error, QString("Create cached input SRV failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }

        D3D11_TEXTURE2D_DESC outputDesc = inputDesc;
        outputDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
        hr = m_device->CreateTexture2D(&outputDesc, nullptr, &m_outputTexture);
        if (FAILED(hr)) {
            setError(error, QString("Create cached output texture failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }
        hr = m_device->CreateRenderTargetView(m_outputTexture, nullptr, &m_outputRtv);
        if (FAILED(hr)) {
            setError(error, QString("Create cached output RTV failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }

        D3D11_TEXTURE2D_DESC stagingDesc = outputDesc;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        hr = m_device->CreateTexture2D(&stagingDesc, nullptr, &m_stagingTexture);
        if (FAILED(hr)) {
            setError(error, QString("Create cached staging texture failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }

        m_frameSize = size;
        return true;
    }

    bool ensureLutResources(int size, QString* error)
    {
        if (m_lutTexture && m_lutSrv && m_lutSize == size) {
            return true;
        }

        release(m_lutSrv);
        release(m_lutTexture);
        m_lutSize = 0;

        D3D11_TEXTURE3D_DESC desc = {};
        desc.Width = static_cast<UINT>(size);
        desc.Height = static_cast<UINT>(size);
        desc.Depth = static_cast<UINT>(size);
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = m_device->CreateTexture3D(&desc, nullptr, &m_lutTexture);
        if (FAILED(hr)) {
            setError(error, QString("Create cached LUT texture failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }
        hr = m_device->CreateShaderResourceView(m_lutTexture, nullptr, &m_lutSrv);
        if (FAILED(hr)) {
            setError(error, QString("Create cached LUT SRV failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }

        m_lutSize = size;
        return true;
    }

    void updateLutTexture(const Lut3D& lut)
    {
        const int size = lut.size();
        std::vector<float> data(static_cast<size_t>(size * size * size * 4));
        size_t cursor = 0;
        for (int b = 0; b < size; ++b) {
            for (int g = 0; g < size; ++g) {
                for (int r = 0; r < size; ++r) {
                    const QVector3D value = lut.value(r, g, b);
                    data[cursor++] = value.x();
                    data[cursor++] = value.y();
                    data[cursor++] = value.z();
                    data[cursor++] = 1.0f;
                }
            }
        }

        m_context->UpdateSubresource(m_lutTexture,
                                     0,
                                     nullptr,
                                     data.data(),
                                     static_cast<UINT>(sizeof(float) * 4 * size),
                                     static_cast<UINT>(sizeof(float) * 4 * size * size));
    }

    bool compileShaders(QString* error)
    {
        static const char* vertexShaderSource = R"(
            struct VSOut {
                float4 pos : SV_POSITION;
                float2 uv : TEXCOORD0;
            };

            VSOut main(uint id : SV_VertexID) {
                float2 positions[3] = {
                    float2(-1.0, -1.0),
                    float2(-1.0,  3.0),
                    float2( 3.0, -1.0)
                };
                float2 uvs[3] = {
                    float2(0.0, 1.0),
                    float2(0.0, -1.0),
                    float2(2.0, 1.0)
                };
                VSOut output;
                output.pos = float4(positions[id], 0.0, 1.0);
                output.uv = uvs[id];
                return output;
            }
        )";

        static const char* pixelShaderSource = R"(
            Texture2D InputTex : register(t0);
            Texture3D LutTex : register(t1);
            SamplerState InputSamp : register(s0);
            SamplerState LutSamp : register(s1);

            struct VSOut {
                float4 pos : SV_POSITION;
                float2 uv : TEXCOORD0;
            };

            float4 main(VSOut input) : SV_TARGET {
                float4 color = InputTex.Sample(InputSamp, input.uv);
                float3 graded = LutTex.Sample(LutSamp, saturate(color.rgb)).rgb;
                return float4(saturate(graded), color.a);
            }
        )";

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;
        HRESULT hr = D3DCompile(vertexShaderSource,
                                std::strlen(vertexShaderSource),
                                nullptr,
                                nullptr,
                                nullptr,
                                "main",
                                "vs_4_0",
                                0,
                                0,
                                &vsBlob,
                                &errorBlob);
        if (FAILED(hr)) {
            setCompileError(error, "Vertex shader compilation failed", hr, errorBlob);
            release(errorBlob);
            return false;
        }
        release(errorBlob);

        hr = D3DCompile(pixelShaderSource,
                        std::strlen(pixelShaderSource),
                        nullptr,
                        nullptr,
                        nullptr,
                        "main",
                        "ps_4_0",
                        0,
                        0,
                        &psBlob,
                        &errorBlob);
        if (FAILED(hr)) {
            setCompileError(error, "Pixel shader compilation failed", hr, errorBlob);
            release(errorBlob);
            release(vsBlob);
            return false;
        }
        release(errorBlob);

        hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
        if (FAILED(hr)) {
            setError(error, QString("CreateVertexShader failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            release(vsBlob);
            release(psBlob);
            return false;
        }
        hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
        if (FAILED(hr)) {
            setError(error, QString("CreatePixelShader failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            release(vsBlob);
            release(psBlob);
            return false;
        }

        release(vsBlob);
        release(psBlob);
        return true;
    }

    bool createLutTexture(const Lut3D& lut, ID3D11Texture3D** texture, ID3D11ShaderResourceView** srv, QString* error)
    {
        const int size = lut.size();
        std::vector<float> data(static_cast<size_t>(size * size * size * 4));
        size_t cursor = 0;
        for (int b = 0; b < size; ++b) {
            for (int g = 0; g < size; ++g) {
                for (int r = 0; r < size; ++r) {
                    const QVector3D value = lut.value(r, g, b);
                    data[cursor++] = value.x();
                    data[cursor++] = value.y();
                    data[cursor++] = value.z();
                    data[cursor++] = 1.0f;
                }
            }
        }

        D3D11_TEXTURE3D_DESC desc = {};
        desc.Width = static_cast<UINT>(size);
        desc.Height = static_cast<UINT>(size);
        desc.Depth = static_cast<UINT>(size);
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initial = {};
        initial.pSysMem = data.data();
        initial.SysMemPitch = static_cast<UINT>(sizeof(float) * 4 * size);
        initial.SysMemSlicePitch = static_cast<UINT>(sizeof(float) * 4 * size * size);

        HRESULT hr = m_device->CreateTexture3D(&desc, &initial, texture);
        if (FAILED(hr)) {
            setError(error, QString("Create LUT texture failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }
        hr = m_device->CreateShaderResourceView(*texture, nullptr, srv);
        if (FAILED(hr)) {
            setError(error, QString("Create LUT SRV failed: 0x%1").arg(static_cast<unsigned int>(hr), 0, 16));
            return false;
        }
        return true;
    }

    void setError(QString* error, const QString& message)
    {
        if (error) {
            *error = message;
        }
    }

    void setCompileError(QString* error, const QString& prefix, HRESULT hr, ID3DBlob* errorBlob)
    {
        QString message = QString("%1: 0x%2").arg(prefix).arg(static_cast<unsigned int>(hr), 0, 16);
        if (errorBlob && errorBlob->GetBufferPointer()) {
            message += QString(" - %1").arg(QString::fromUtf8(static_cast<const char*>(errorBlob->GetBufferPointer()),
                                                              static_cast<int>(errorBlob->GetBufferSize())));
        }
        setError(error, message);
    }

    std::mutex m_mutex;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    ID3D11Texture2D* m_inputTexture = nullptr;
    ID3D11ShaderResourceView* m_inputSrv = nullptr;
    ID3D11Texture3D* m_lutTexture = nullptr;
    ID3D11ShaderResourceView* m_lutSrv = nullptr;
    ID3D11Texture2D* m_outputTexture = nullptr;
    ID3D11RenderTargetView* m_outputRtv = nullptr;
    ID3D11Texture2D* m_stagingTexture = nullptr;
    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11SamplerState* m_inputSampler = nullptr;
    ID3D11SamplerState* m_lutSampler = nullptr;
    QSize m_frameSize;
    int m_lutSize = 0;
};

} // namespace
#endif

bool DirectX11PreviewRenderer::render(const QImage& source, const Lut3D& lut, QImage* output, QString* error)
{
#ifdef _WIN32
    static DxPreviewBackend backend;
    return backend.render(source, lut, output, error, nullptr);
#else
    Q_UNUSED(source);
    Q_UNUSED(lut);
    Q_UNUSED(output);
    if (error) {
        *error = "DirectX preview is only available on Windows.";
    }
    return false;
#endif
}

bool DirectX11PreviewRenderer::render(const QImage& source, const Lut3D& lut, QImage* output, QString* error, QString* diagnostic)
{
#ifdef _WIN32
    static DxPreviewBackend backend;
    return backend.render(source, lut, output, error, diagnostic);
#else
    Q_UNUSED(source);
    Q_UNUSED(lut);
    Q_UNUSED(output);
    Q_UNUSED(diagnostic);
    if (error) {
        *error = "DirectX preview is only available on Windows.";
    }
    return false;
#endif
}

} // namespace ikclut
