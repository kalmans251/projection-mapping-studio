#pragma once

#include "D3D11Engine.h"
#include <QPolygonF>
#include <QString>
#include <QSize>
#include <array>
#include <windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi.h>

struct GpuCalibration {
    QPolygonF quad;
    int overlap=15;
    int brightness=100;
    double blendGamma=2.2;
    std::array<int,3> gains{100,100,100};
};

class D3D11Presenter {
public:
    explicit D3D11Presenter(D3D11Engine& engine) : engine_(engine) {}
    bool initialize(QString& error);
    bool attach(int index, HWND hwnd, QSize size, QString& error);
    void detach();
    bool render(int index, const GpuCalibration& settings, QString& error);

private:
    struct Output {
        Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        UINT width=0;
        UINT height=0;
    };
    struct alignas(16) Constants {
        float warpX[4]{};
        float warpY[4]{};
        float misc[4]{};
        float rgb[4]{};
        float crop[4]{};
    };
    D3D11Engine& engine_;
    std::array<Output,2> outputs_{};
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    bool ready_=false;
};
