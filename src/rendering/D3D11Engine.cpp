#include "D3D11Engine.h"
#include <array>

static QString hresultMessage(HRESULT hr) {
    return QString("HRESULT 0x%1").arg(quint32(hr),8,16,QChar('0'));
}

bool D3D11Engine::initialize(QString& error) {
    if(device_) return true;
    const std::array<D3D_FEATURE_LEVEL,4> levels{
        D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0
    };
    auto create=[this](const D3D_FEATURE_LEVEL* requested,UINT count) {
        return D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,requested,count,D3D11_SDK_VERSION,
            device_.GetAddressOf(),&featureLevel_,context_.GetAddressOf());
    };
    HRESULT hr=create(levels.data(),UINT(levels.size()));
    if(hr==E_INVALIDARG) hr=create(levels.data()+1,UINT(levels.size()-1));
    if(FAILED(hr)) {
        error="D3D11 hardware device creation failed: "+hresultMessage(hr);
        device_.Reset();context_.Reset();
        return false;
    }
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC desc{};
    if(SUCCEEDED(device_.As(&dxgiDevice)) &&
       SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf())) &&
       SUCCEEDED(adapter->GetDesc(&desc))) {
        adapterName_=QString::fromWCharArray(desc.Description);
    } else adapterName_="Unknown GPU";
    return true;
}

bool D3D11Engine::uploadFrame(const QImage& frame,QString& error) {
    if(!device_) {error="GPU is not initialized.";return false;}
    if(frame.isNull()) {error="Empty video frame.";return false;}
    // QImage ARGB32 stores BGRA byte order on little-endian Windows.
    QImage image=frame.convertToFormat(QImage::Format_ARGB32);
    if(image.width()<=0||image.height()<=0) return false;
    if(width_!=UINT(image.width()) || height_!=UINT(image.height())) {
        videoView_.Reset(); videoTexture_.Reset();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=UINT(image.width());
        desc.Height=UINT(image.height());
        desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count=1;
        desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        const HRESULT hr=device_->CreateTexture2D(&desc,nullptr,videoTexture_.GetAddressOf());
        if(FAILED(hr)) {
            error="CreateTexture2D failed: "+hresultMessage(hr);
            return false;
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format=desc.Format;
        view.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels=1;
        const HRESULT srv=device_->CreateShaderResourceView(videoTexture_.Get(),&view,videoView_.GetAddressOf());
        if(FAILED(srv)) {
            error="CreateShaderResourceView failed: "+hresultMessage(srv);
            videoTexture_.Reset();
            return false;
        }
        width_=desc.Width;height_=desc.Height;
    }
    context_->UpdateSubresource(videoTexture_.Get(),0,nullptr,image.constBits(),UINT(image.bytesPerLine()),0);
    ++uploadedFrames_;
    uploadedBytes_+=std::uint64_t(image.bytesPerLine())*std::uint64_t(image.height());
    return true;
}

QString D3D11Engine::description() const {
    if(!device_) return "D3D11 not initialized";
    const int major=(int(featureLevel_)>>12)&15;
    const int minor=(int(featureLevel_)>>8)&15;
    return QString("%1 | D3D feature level %2.%3 | uploaded %4 frames (%5 MiB)")
        .arg(adapterName_).arg(major).arg(minor).arg(uploadedFrames_)
        .arg(uploadedBytes_/1048576);
}
