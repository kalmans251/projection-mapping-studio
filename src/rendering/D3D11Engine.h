#pragma once
#include <QImage>
#include <QString>
#include <cstdint>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi.h>

class D3D11Engine {
public:
    bool initialize(QString& error);
    bool uploadFrame(const QImage& frame, QString& error);
    bool isReady() const { return device_.Get()!=nullptr; }
    QString description() const;
    std::uint64_t uploadedFrames() const { return uploadedFrames_; }
    std::uint64_t uploadedBytes() const { return uploadedBytes_; }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> videoTexture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> videoView_;
    D3D_FEATURE_LEVEL featureLevel_ = D3D_FEATURE_LEVEL_9_1;
    QString adapterName_;
    UINT width_=0, height_=0;
    std::uint64_t uploadedFrames_=0, uploadedBytes_=0;
};
