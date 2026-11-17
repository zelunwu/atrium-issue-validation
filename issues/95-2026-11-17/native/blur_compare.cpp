#include "blur_compare.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "glass_vs.h"
#include "blur_baseline_ps.h"
#include "blur_downsample_ps.h"
#include "blur_kawase_ps.h"
#include "blur_upsample_ps.h"

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
void Png(const fs::path&, const std::vector<uint8_t>&, UINT, UINT);

namespace {
void CheckBlur(HRESULT hr) {
    if (FAILED(hr)) throw std::runtime_error("Blur comparison HRESULT " + std::to_string(static_cast<unsigned>(hr)));
}
template<class T> T** Out(ComPtr<T>& p) { return p.ReleaseAndGetAddressOf(); }

struct Target {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11RenderTargetView> rtv;
    UINT width = 0, height = 0;
};

Target MakeTarget(ID3D11Device* dev, UINT width, UINT height, bool mips) {
    Target t{};t.width=width;t.height=height;
    D3D11_TEXTURE2D_DESC d{};
    d.Width=width;d.Height=height;d.MipLevels=mips?0:1;d.ArraySize=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;
    d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    d.MiscFlags=mips?D3D11_RESOURCE_MISC_GENERATE_MIPS:0;
    CheckBlur(dev->CreateTexture2D(&d,nullptr,Out(t.texture)));
    CheckBlur(dev->CreateShaderResourceView(t.texture.Get(),nullptr,Out(t.srv)));
    if(!mips) CheckBlur(dev->CreateRenderTargetView(t.texture.Get(),nullptr,Out(t.rtv)));
    return t;
}

std::vector<uint8_t> Input(UINT width, UINT height) {
    std::vector<uint8_t> p(size_t(width)*height*4);
    for(UINT y=0;y<height;++y) for(UINT x=0;x<width;++x) {
        const UINT i=(y*width+x)*4;
        const bool square=x>width/3 && x<width*2/3 && y>height/3 && y<height*2/3;
        const int stripe=(x/13)%2?220:30;
        p[i]=uint8_t(square?245:std::min(255,stripe+int((x/29+y/29)%2)*20));
        p[i+1]=uint8_t(square?245:(y*5+(x/17)*17)%256);
        p[i+2]=uint8_t(square?245:(x*3+y*2+stripe/2)%256);
        p[i+3]=255;
    }
    return p;
}

void Save(ID3D11Device* dev, ID3D11DeviceContext* ctx, const Target& t, const fs::path& path) {
    D3D11_TEXTURE2D_DESC d{};t.texture->GetDesc(&d);
    d.MipLevels=1;d.BindFlags=0;d.MiscFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> stage;CheckBlur(dev->CreateTexture2D(&d,nullptr,Out(stage)));
    ctx->CopyResource(stage.Get(),t.texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};CheckBlur(ctx->Map(stage.Get(),0,D3D11_MAP_READ,0,&mapped));
    std::vector<uint8_t> pixels(size_t(t.width)*t.height*4);
    for(UINT y=0;y<t.height;++y)
        std::memcpy(pixels.data()+size_t(y)*t.width*4,static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch,t.width*4);
    ctx->Unmap(stage.Get(),0);
    Png(path,pixels,t.width,t.height);
}

struct Params {float invX,invY,offset,lod;};
struct Pipeline {
    ID3D11DeviceContext* ctx;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> baseline,downsample,kawase,upsample;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11Buffer> constants;

    explicit Pipeline(ID3D11Device* dev, ID3D11DeviceContext* context):ctx(context) {
        CheckBlur(dev->CreateVertexShader(g_glass_vs,sizeof(g_glass_vs),nullptr,Out(vs)));
        CheckBlur(dev->CreatePixelShader(g_blur_baseline_ps,sizeof(g_blur_baseline_ps),nullptr,Out(baseline)));
        CheckBlur(dev->CreatePixelShader(g_blur_downsample_ps,sizeof(g_blur_downsample_ps),nullptr,Out(downsample)));
        CheckBlur(dev->CreatePixelShader(g_blur_kawase_ps,sizeof(g_blur_kawase_ps),nullptr,Out(kawase)));
        CheckBlur(dev->CreatePixelShader(g_blur_upsample_ps,sizeof(g_blur_upsample_ps),nullptr,Out(upsample)));
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;s.MaxLOD=D3D11_FLOAT32_MAX;
        CheckBlur(dev->CreateSamplerState(&s,Out(sampler)));
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Params);cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        CheckBlur(dev->CreateBuffer(&cb,nullptr,Out(constants)));
    }

    void Draw(ID3D11PixelShader* ps, const Target& source, const Target& dest, float offset, float lod) {
        Params p{1.f/source.width,1.f/source.height,offset,lod};
        ctx->UpdateSubresource(constants.Get(),0,nullptr,&p,0,0);
        ID3D11RenderTargetView* rt=dest.rtv.Get();ctx->OMSetRenderTargets(1,&rt,nullptr);
        D3D11_VIEWPORT vp{0,0,float(dest.width),float(dest.height),0,1};ctx->RSSetViewports(1,&vp);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->IASetInputLayout(nullptr);ctx->VSSetShader(vs.Get(),nullptr,0);ctx->PSSetShader(ps,nullptr,0);
        ID3D11ShaderResourceView* srv=source.srv.Get();ctx->PSSetShaderResources(0,1,&srv);
        ID3D11SamplerState* s=sampler.Get();ctx->PSSetSamplers(0,1,&s);
        ID3D11Buffer* cb=constants.Get();ctx->PSSetConstantBuffers(0,1,&cb);
        ctx->Draw(3,0);
        srv=nullptr;rt=nullptr;ctx->PSSetShaderResources(0,1,&srv);ctx->OMSetRenderTargets(1,&rt,nullptr);
    }
};

struct Timer {
    ID3D11DeviceContext* ctx;
    ComPtr<ID3D11Query> disjoint,start,end;
    explicit Timer(ID3D11Device* dev, ID3D11DeviceContext* context):ctx(context) {
        D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP_DISJOINT,0};CheckBlur(dev->CreateQuery(&q,Out(disjoint)));
        q.Query=D3D11_QUERY_TIMESTAMP;CheckBlur(dev->CreateQuery(&q,Out(start)));
        CheckBlur(dev->CreateQuery(&q,Out(end)));
    }
    void Begin() {ctx->Begin(disjoint.Get());ctx->End(start.Get());}
    double Finish() {
        ctx->End(end.Get());ctx->End(disjoint.Get());ctx->Flush();
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        auto read=[&](ID3D11Query* q,void* data,UINT bytes) {
            while(ctx->GetData(q,data,bytes,0)==S_FALSE) {
                if(std::chrono::steady_clock::now()>deadline) throw std::runtime_error("Blur GPU query timeout");
                Sleep(1);
            }
        };
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT info{};UINT64 a=0,b=0;
        read(disjoint.Get(),&info,sizeof(info));read(start.Get(),&a,sizeof(a));read(end.Get(),&b,sizeof(b));
        if(info.Disjoint||!info.Frequency||b<a) throw std::runtime_error("Blur GPU query disjoint");
        return double(b-a)*1000.0/double(info.Frequency);
    }
};
}

void RunBlurCompare(const fs::path& output) {
    CheckBlur(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
    fs::create_directories(output);
    ComPtr<ID3D11Device> dev;ComPtr<ID3D11DeviceContext> ctx;D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                 levels,2,D3D11_SDK_VERSION,Out(dev),&level,Out(ctx));
    if(FAILED(hr)) CheckBlur(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                             levels,2,D3D11_SDK_VERSION,Out(dev),&level,Out(ctx)));
    Pipeline pipe(dev.Get(),ctx.Get());Timer timer(dev.Get(),ctx.Get());
    std::ofstream csv(output/L"timings.csv");
    if(!csv) throw std::runtime_error("Cannot open blur comparison CSV");
    csv<<"width,height,algorithm,downsample,passes,offset_scale,sample,gpu_ms\n";
    constexpr float lod=3.25f; // Atrium's default blur=0.5
    const std::array<std::pair<UINT,UINT>,3> sizes{{{640,320},{1920,1080},{2560,1440}}};
    for(auto [width,height]:sizes) {
        auto source=MakeTarget(dev.Get(),width,height,true);
        auto pixels=Input(width,height);
        ctx->UpdateSubresource(source.texture.Get(),0,nullptr,pixels.data(),width*4,0);
        auto baseline=MakeTarget(dev.Get(),width,height,false);
        auto result=MakeTarget(dev.Get(),width,height,false);
        for(int sample=-3;sample<20;++sample) {
            timer.Begin();
            ctx->GenerateMips(source.srv.Get());
            pipe.Draw(pipe.baseline.Get(),source,baseline,0,lod);
            double ms=timer.Finish();
            if(sample>=0) csv<<width<<','<<height<<",atrium,1,0,0,"<<sample<<','<<ms<<'\n';
        }
        if(width==640) Save(dev.Get(),ctx.Get(),baseline,output/L"atrium_blur.png");
        for(int factor:{2,4}) {
          auto lowA=MakeTarget(dev.Get(),width/factor,height/factor,false);
          auto lowB=MakeTarget(dev.Get(),width/factor,height/factor,false);
          for(int passes=1;passes<=4;++passes) for(float scale:{0.5f,1.f,1.5f}) {
            for(int sample=-3;sample<20;++sample) {
                timer.Begin();
                pipe.Draw(pipe.downsample.Get(),source,lowA,0,0);
                Target* read=&lowA;Target* write=&lowB;
                for(int pass=0;pass<passes;++pass) {
                    pipe.Draw(pipe.kawase.Get(),*read,*write,(pass+0.5f)*scale,0);
                    std::swap(read,write);
                }
                pipe.Draw(pipe.upsample.Get(),*read,result,0,0);
                double ms=timer.Finish();
                if(sample>=0) csv<<width<<','<<height<<",kawase,"<<factor<<','<<passes<<','<<scale<<','<<sample<<','<<ms<<'\n';
            }
            if(width==640) {
                wchar_t file[80]{};swprintf_s(file,L"kawase_%dx_%dpass_%.1foffset.png",factor,passes,scale);
                Save(dev.Get(),ctx.Get(),result,output/file);
            }
          }
        }
        std::cout<<width<<'x'<<height<<" blur comparison complete\n";
    }
    CoUninitialize();
}
