#include "D3D11Presenter.h"
#include <QTransform>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>

using Microsoft::WRL::ComPtr;

static QString dxError(HRESULT hr) {
    return QString("HRESULT 0x%1").arg(quint32(hr),8,16,QChar('0'));
}

static constexpr char kShader[] = R"HLSL(
cbuffer Calibration : register(b0) {
    float4 warpX;
    float4 warpY;
    float4 misc;
    float4 rgb;
    float4 crop;
};
Texture2D imageTex : register(t0);
SamplerState imageSampler : register(s0);

struct VSOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint vertexId : SV_VertexID) {
    float2 position = vertexId == 0 ? float2(-1.0,-1.0) :
                      vertexId == 1 ? float2(-1.0, 3.0) : float2(3.0,-1.0);
    VSOutput result;
    result.position = float4(position,0.0,1.0);
    result.uv=float2((position.x+1.0)*0.5,(1.0-position.y)*0.5);
    return result;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    float2 xy=input.uv;
    float denominator=warpX.w*xy.x + warpY.w*xy.y + misc.x;
    if(abs(denominator)<1.0e-6) return float4(0,0,0,1);
    float2 local=float2(dot(warpX.xyz,float3(xy,1.0)),
                        dot(warpY.xyz,float3(xy,1.0))) / denominator;
    if(local.x<0.0 || local.x>1.0 || local.y<0.0 || local.y>1.0)
        return float4(0,0,0,1);

    float sourceX=lerp(crop.x,crop.y,local.x);
    float3 color=imageTex.Sample(imageSampler,float2(sourceX,local.y)).rgb;
    const float feather=misc.y;
    float weight=1.0;
    if(feather>0.00001) {
        if(crop.z<0.5 && local.x>1.0-feather)
            weight=saturate((1.0-local.x)/feather);
        if(crop.z>=0.5 && local.x<feather)
            weight=saturate(local.x/feather);
        weight=pow(weight,1.0/max(misc.w,0.1));
    }
    color *= rgb.xyz * misc.z * weight;
    return float4(saturate(color),1.0);
}
)HLSL";

bool D3D11Presenter::initialize(QString& error) {
    if(ready_) return true;
    if(!engine_.isReady() && !engine_.initialize(error)) return false;
    ID3D11Device* device=engine_.device();
    ComPtr<ID3DBlob> vsBlob, psBlob, errors;
    HRESULT hr=D3DCompile(kShader,std::strlen(kShader),"ProjectionShader",nullptr,nullptr,
                          "VSMain","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,
                          vsBlob.GetAddressOf(),errors.GetAddressOf());
    if(FAILED(hr)) {
        error=errors ? QString::fromUtf8(static_cast<const char*>(errors->GetBufferPointer()),
                                         int(errors->GetBufferSize())) : dxError(hr);
        return false;
    }
    errors.Reset();
    hr=D3DCompile(kShader,std::strlen(kShader),"ProjectionShader",nullptr,nullptr,
                  "PSMain","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,
                  psBlob.GetAddressOf(),errors.GetAddressOf());
    if(FAILED(hr)) {
        error=errors ? QString::fromUtf8(static_cast<const char*>(errors->GetBufferPointer()),
                                         int(errors->GetBufferSize())) : dxError(hr);
        return false;
    }
    hr=device->CreateVertexShader(vsBlob->GetBufferPointer(),vsBlob->GetBufferSize(),
                                  nullptr,vertexShader_.GetAddressOf());
    if(FAILED(hr)) {error="CreateVertexShader: "+dxError(hr);return false;}
    hr=device->CreatePixelShader(psBlob->GetBufferPointer(),psBlob->GetBufferSize(),
                                 nullptr,pixelShader_.GetAddressOf());
    if(FAILED(hr)) {error="CreatePixelShader: "+dxError(hr);return false;}

    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth=sizeof(Constants);
    cb.Usage=D3D11_USAGE_DEFAULT;
    cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    hr=device->CreateBuffer(&cb,nullptr,constants_.GetAddressOf());
    if(FAILED(hr)) {error="CreateBuffer: "+dxError(hr);return false;}
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD=D3D11_FLOAT32_MAX;
    hr=device->CreateSamplerState(&sampler,sampler_.GetAddressOf());
    if(FAILED(hr)) {error="CreateSamplerState: "+dxError(hr);return false;}
    ready_=true;
    return true;
}

bool D3D11Presenter::attach(int index,HWND hwnd,QSize size,QString& error) {
    if(index<0||index>1||!hwnd||size.width()<1||size.height()<1) {
        error="Invalid output window or size.";return false;
    }
    if(!initialize(error)) return false;
    auto& out=outputs_[index];
    out.rtv.Reset();out.swapChain.Reset();
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory> factory;
    HRESULT hr=engine_.device()->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()));
    if(SUCCEEDED(hr)) hr=dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if(SUCCEEDED(hr)) hr=adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));
    if(FAILED(hr)) {error="DXGI factory: "+dxError(hr);return false;}

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=UINT(size.width());
    desc.BufferDesc.Height=UINT(size.height());
    desc.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=2;
    desc.OutputWindow=hwnd;
    desc.Windowed=TRUE;
    desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    hr=factory->CreateSwapChain(engine_.device(),&desc,out.swapChain.GetAddressOf());
    if(FAILED(hr)) {error="CreateSwapChain: "+dxError(hr);return false;}
    factory->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);
    ComPtr<ID3D11Texture2D> backbuffer;
    hr=out.swapChain->GetBuffer(0,IID_PPV_ARGS(backbuffer.GetAddressOf()));
    if(SUCCEEDED(hr)) hr=engine_.device()->CreateRenderTargetView(
        backbuffer.Get(),nullptr,out.rtv.GetAddressOf());
    if(FAILED(hr)) {error="CreateRenderTargetView: "+dxError(hr);out.swapChain.Reset();return false;}
    out.width=UINT(size.width());out.height=UINT(size.height());
    return true;
}

void D3D11Presenter::detach() {
    if(engine_.context()) {
        engine_.context()->OMSetRenderTargets(0,nullptr,nullptr);
        engine_.context()->Flush();
    }
    for(auto& out:outputs_) {
        out.rtv.Reset();
        out.swapChain.Reset();
        out.width=out.height=0;
    }
}

bool D3D11Presenter::render(int index,const GpuCalibration& settings,QString& error) {
    if(index<0||index>1||!ready_ || !engine_.videoView()) return false;
    auto& out=outputs_[index];
    if(!out.rtv || !out.swapChain) return false;
    if(settings.quad.size()!=4) {error="Four warp points required.";return false;}
    QPolygonF unit;
    unit << QPointF(0,0) << QPointF(1,0) << QPointF(1,1) << QPointF(0,1);
    QTransform inverse;
    if(!QTransform::quadToQuad(settings.quad,unit,inverse)) {
        error="Warp quadrilateral is degenerate.";return false;
    }
    Constants c{};
    c.warpX[0]=float(inverse.m11());
    c.warpX[1]=float(inverse.m21());
    c.warpX[2]=float(inverse.dx());
    c.warpX[3]=float(inverse.m13());
    c.warpY[0]=float(inverse.m12());
    c.warpY[1]=float(inverse.m22());
    c.warpY[2]=float(inverse.dy());
    c.warpY[3]=float(inverse.m23());
    const float overlap=float(std::clamp(settings.overlap,0,40))/100.0f;
    c.misc[0]=float(inverse.m33());
    c.misc[1]=overlap<=0 ? 0.0f : overlap/(.5f+overlap*.5f);
    c.misc[2]=float(std::clamp(settings.brightness,0,100))/100.0f;
    c.misc[3]=float(std::clamp(settings.blendGamma,1.0,3.5));
    for(int j=0;j<3;++j) c.rgb[j]=float(std::clamp(settings.gains[j],0,100))/100.0f;
    c.crop[0]=index==0 ? 0.0f : .5f-overlap*.5f;
    c.crop[1]=index==0 ? .5f+overlap*.5f : 1.0f;
    c.crop[2]=float(index);

    ID3D11DeviceContext* ctx=engine_.context();
    ctx->UpdateSubresource(constants_.Get(),0,nullptr,&c,0,0);
    ID3D11RenderTargetView* rtv=out.rtv.Get();
    ctx->OMSetRenderTargets(1,&rtv,nullptr);
    const FLOAT black[4]={0,0,0,1};
    ctx->ClearRenderTargetView(rtv,black);
    D3D11_VIEWPORT viewport{0,0,float(out.width),float(out.height),0,1};
    ctx->RSSetViewports(1,&viewport);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vertexShader_.Get(),nullptr,0);
    ctx->PSSetShader(pixelShader_.Get(),nullptr,0);
    ID3D11Buffer* buffer=constants_.Get();
    ctx->PSSetConstantBuffers(0,1,&buffer);
    ID3D11ShaderResourceView* srv=engine_.videoView();
    ctx->PSSetShaderResources(0,1,&srv);
    ID3D11SamplerState* sampler=sampler_.Get();
    ctx->PSSetSamplers(0,1,&sampler);
    ctx->Draw(3,0);
    // Release the SRV bind before any subsequent texture updates.
    ID3D11ShaderResourceView* nullSrv=nullptr;
    ctx->PSSetShaderResources(0,1,&nullSrv);
    HRESULT hr=out.swapChain->Present(1,0);
    if(FAILED(hr)) {error="Present: "+dxError(hr);return false;}
    return true;
}
