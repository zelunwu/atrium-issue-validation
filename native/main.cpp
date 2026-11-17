#include <windows.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "render/glass_renderer.h"
#include "render/corner_profile.h"
#include "render/callout_profile.h"
#include "render/continuous_corners.h"
#include "render/wifi_glyph.h"
#include "render/bar_painter.h"
#include "render/ui_font.h"
#include "ui/widget_faces.h"
#include "ui/glass_menu.h"
#include "ui/calendar_panel.h"
#include "ui/notifications.h"
#include "ui/control_center.h"
#include "ui/window_preview.h"
#include "ui/start_menu.h"
#include "ui/tray_panel.h"
#include "ui/stage_manager.h"
#include "ui/desktop_widgets.h"
#include "glass_vs.h"
#include "glass_ps.h"
#include "backdrop_edge_ps.h"
#include "compare_cs.h"
#include "perf.h"
#include "blur_compare.h"

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
using namespace atrium;
constexpr UINT W = 640, H = 320;

void Check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("HRESULT " + std::to_string(static_cast<unsigned>(hr))); }
template<class T> T** Out(ComPtr<T>& p) { return p.ReleaseAndGetAddressOf(); }
uint8_t Byte(float v) { return static_cast<uint8_t>(std::lround(std::clamp(v, 0.f, 1.f) * 255)); }

void Png(const fs::path& path, const std::vector<uint8_t>& pixels, UINT w, UINT h)
{
    ComPtr<IWICImagingFactory> factory;
    Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(Out(factory))));
    ComPtr<IWICStream> stream;
    Check(factory->CreateStream(Out(stream)));
    Check(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;
    Check(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, Out(encoder)));
    Check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    Check(encoder->CreateNewFrame(Out(frame), Out(props)));
    Check(frame->Initialize(props.Get()));
    Check(frame->SetSize(w, h));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    Check(frame->SetPixelFormat(&format));
    if (format != GUID_WICPixelFormat32bppBGRA) throw std::runtime_error("WIC BGRA unsupported");
    Check(frame->WritePixels(h, w * 4, static_cast<UINT>(pixels.size()), const_cast<BYTE*>(pixels.data())));
    Check(frame->Commit());
    Check(encoder->Commit());
}

std::vector<uint8_t> Background()
{
    std::vector<uint8_t> p(W * H * 4);
    for (UINT y = 0; y < H; ++y) for (UINT x = 0; x < W; ++x) {
        UINT i = (y * W + x) * 4;
        int stripe = (x / 13) % 2 ? 215 : 35;
        int checker = (x / 29 + y / 29) % 2 ? 0 : 35;
        p[i] = static_cast<uint8_t>(std::min(255, stripe + checker));
        p[i + 1] = static_cast<uint8_t>((y * 3 + checker) % 256);
        p[i + 2] = static_cast<uint8_t>((x + y + stripe / 2) % 256);
        p[i + 3] = 255;
    }
    return p;
}

ComPtr<ID3D11Texture2D> Texture(ID3D11Device* dev, UINT w, UINT h, UINT bind, UINT misc = 0, UINT levels = 1, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, UINT cpu = 0)
{
    D3D11_TEXTURE2D_DESC d{};
    d.Width=w; d.Height=h; d.MipLevels=levels; d.ArraySize=1; d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count=1; d.Usage=usage; d.BindFlags=bind; d.CPUAccessFlags=cpu; d.MiscFlags=misc;
    ComPtr<ID3D11Texture2D> result; Check(dev->CreateTexture2D(&d,nullptr,Out(result))); return result;
}

std::vector<uint8_t> Read(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* source)
{
    D3D11_TEXTURE2D_DESC d{}; source->GetDesc(&d);
    auto stage=Texture(dev,d.Width,d.Height,0,0,1,D3D11_USAGE_STAGING,D3D11_CPU_ACCESS_READ);
    ctx->CopyResource(stage.Get(),source);
    D3D11_MAPPED_SUBRESOURCE map{}; Check(ctx->Map(stage.Get(),0,D3D11_MAP_READ,0,&map));
    std::vector<uint8_t> pixels(d.Width*d.Height*4);
    for(UINT y=0;y<d.Height;++y) memcpy(pixels.data()+y*d.Width*4,static_cast<const uint8_t*>(map.pData)+y*map.RowPitch,d.Width*4);
    ctx->Unmap(stage.Get(),0); return pixels;
}

namespace atrium {
fs::path baselineCapturePath;
void BaselineCapture(ID3D11Texture2D* texture)
{
    if(baselineCapturePath.empty()) return;
    ComPtr<ID3D11Device> dev; texture->GetDevice(Out(dev));
    ComPtr<ID3D11DeviceContext> ctx; dev->GetImmediateContext(Out(ctx));
    D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
    auto pixels=Read(dev.Get(),ctx.Get(),texture);
    for(size_t i=0;i<pixels.size();i+=4) {
        const float a=pixels[i+3]/255.f;
        for(int k=0;k<3;++k) pixels[i+k]=uint8_t(std::clamp(int(pixels[i+k]+255*(1-a)+.5f),0,255));
        pixels[i+3]=255;
    }
    Png(baselineCapturePath,pixels,desc.Width,desc.Height);
}
}

GlassParams Base()
{
    GlassParams p{};
    p.sizeX=float(W); p.sizeY=float(H); p.roiSizeX=float(W); p.roiSizeY=float(H);
    p.radius=28; p.edge=24; p.refract=18; p.dispersion=.05f; p.blurLod=2.4f; p.blurMix=.56f;
    p.tintMix=.2f; p.tintR=p.tintG=p.tintB=p.tintA=1; p.highlight=.5f; p.shadow=.25f;
    p.lightX=-.6f; p.lightY=-.8f; p.inset=14;
    p.shapeL=84; p.shapeT=74; p.shapeR=556; p.shapeB=246;
    return p;
}

void SaveGlass(const fs::path& path, ID3D11Device* dev, ID3D11DeviceContext* ctx,
               ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11ShaderResourceView* bg,
               ID3D11SamplerState* sampler, ID3D11Buffer* constants, ID3D11Texture2D* target,
               ID3D11RenderTargetView* rt, const GlassParams& p, const std::vector<uint8_t>& backdrop)
{
    float clear[4]{}; ctx->ClearRenderTargetView(rt,clear);
    ctx->OMSetRenderTargets(1,&rt,nullptr);
    D3D11_VIEWPORT viewport{0,0,float(W),float(H),0,1}; ctx->RSSetViewports(1,&viewport);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs,nullptr,0); ctx->PSSetShader(ps,nullptr,0);
    ctx->PSSetShaderResources(0,1,&bg); ctx->PSSetSamplers(0,1,&sampler);
    ctx->UpdateSubresource(constants,0,nullptr,&p,0,0); ctx->PSSetConstantBuffers(0,1,&constants);
    ctx->Draw(3,0);
    ID3D11RenderTargetView* noRt=nullptr; ctx->OMSetRenderTargets(0,&noRt,nullptr);
    auto pixels=Read(dev,ctx,target);
    for(size_t i=0;i<pixels.size();i+=4) {
        float inv=1.f-pixels[i+3]/255.f;
        for(int c=0;c<3;++c) pixels[i+c]=static_cast<uint8_t>(std::clamp(int(pixels[i+c]+backdrop[i+c]*inv+.5f),0,255));
        pixels[i+3]=255;
    }
    Png(path,pixels,W,H);
}

void CpuShape(const fs::path& path, bool callout)
{
    std::vector<uint8_t> pixels(W*H*4,255);
    auto corner=CornerFor(472,172,28);
    for(UINT y=0;y<H;++y) for(UINT x=0;x<W;++x) {
        float localX=float(x)-84.5f, localY=float(y)-74.5f;
        float d=callout ? CalloutEdge(localX,localY,472,172,28,236,23,28,7)
                        : CornerEdge(std::abs(localX-236)-236+corner.reach,std::abs(localY-86)-86+corner.reach,corner);
        float a=std::clamp(.5f-d,0.f,1.f);
        size_t i=(y*W+x)*4;
        pixels[i]=Byte(1-a*.7f); pixels[i+1]=Byte(1-a*.4f); pixels[i+2]=Byte(1-a*.9f); pixels[i+3]=255;
    }
    Png(path,pixels,W,H);
}

void BackdropEdge(const fs::path& path, ID3D11Device* dev, ID3D11DeviceContext* ctx,
                  ID3D11VertexShader* vs, ID3D11ShaderResourceView* source,
                  ID3D11Texture2D* target, ID3D11RenderTargetView* rt)
{
    ComPtr<ID3D11PixelShader> ps;
    Check(dev->CreatePixelShader(g_backdrop_edge_ps, sizeof(g_backdrop_edge_ps), nullptr, Out(ps)));
    struct EdgeParams { int originX, originY, extentX, extentY; } params{-48, 34, int(W), int(H)};
    D3D11_BUFFER_DESC desc{}; desc.ByteWidth=sizeof(params); desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> cb; Check(dev->CreateBuffer(&desc,nullptr,Out(cb)));
    ctx->UpdateSubresource(cb.Get(),0,nullptr,&params,0,0);
    D3D11_VIEWPORT viewport{0,0,float(W),float(H),0,1};
    ctx->RSSetViewports(1,&viewport);
    ctx->OMSetRenderTargets(1,&rt,nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs,nullptr,0); ctx->PSSetShader(ps.Get(),nullptr,0);
    ctx->PSSetShaderResources(0,1,&source); ID3D11Buffer* buffer=cb.Get();
    ctx->PSSetConstantBuffers(0,1,&buffer); ctx->Draw(3,0);
    ID3D11RenderTargetView* empty=nullptr; ctx->OMSetRenderTargets(0,&empty,nullptr);
    Png(path,Read(dev,ctx,target),W,H);
}

void Direct2DScenes(const fs::path& out, ID3D11Device* dev, ID3D11DeviceContext* gpu)
{
    constexpr UINT UI_H=480;
    ComPtr<IDXGIDevice> dxgi; Check(dev->QueryInterface(IID_PPV_ARGS(Out(dxgi))));
    ComPtr<ID2D1Factory1> factory;
    Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),nullptr,reinterpret_cast<void**>(Out(factory))));
    ComPtr<ID2D1Device> device; Check(factory->CreateDevice(dxgi.Get(),Out(device)));
    ComPtr<ID2D1DeviceContext> d2d; Check(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,Out(d2d)));
    auto tex=Texture(dev,W,UI_H,D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE);
    ComPtr<IDXGISurface> surface; Check(tex.As(&surface));
    auto properties=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
    ComPtr<ID2D1Bitmap1> bitmap; Check(d2d->CreateBitmapFromDxgiSurface(surface.Get(),&properties,Out(bitmap)));
    d2d->SetTarget(bitmap.Get());
    ComPtr<ID2D1SolidColorBrush> brush;
    Check(d2d->CreateSolidColorBrush(D2D1::ColorF(0.2f,0.45f,0.9f),Out(brush)));
    auto save=[&](const wchar_t* name) {
        Check(d2d->EndDraw());
        Png(out/name,Read(dev,gpu,tex.Get()),W,UI_H);
    };
    const auto rect=D2D1::RectF(84,74,556,246);
    d2d->BeginDraw(); d2d->Clear(D2D1::ColorF(D2D1::ColorF::White));
    FillContinuousRect(d2d.Get(),rect,42,brush.Get());
    save(L"016_d2d_continuous_fill.png");
    d2d->BeginDraw(); d2d->Clear(D2D1::ColorF(D2D1::ColorF::White));
    StrokeContinuousRect(d2d.Get(),rect,42,brush.Get(),6);
    save(L"017_d2d_continuous_stroke.png");
    d2d->BeginDraw(); d2d->Clear(D2D1::ColorF(D2D1::ColorF::White));
    for(int step=1;step<=3;++step)
        DrawWifiFan(d2d.Get(),brush.Get(),150.f*step,148,50,step,D2D1::ColorF(0.1f,0.3f,0.8f),D2D1::ColorF(0.75f,0.78f,0.83f));
    save(L"018_d2d_wifi_glyph.png");
    ComPtr<IDWriteFactory> dw;
    Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(Out(dw))));
    ComPtr<IDWriteTextFormat> format;
    Check(UiTextFormat(dw.Get(),29,DWRITE_FONT_WEIGHT_MEDIUM,Out(format)));
    d2d->BeginDraw(); d2d->Clear(D2D1::ColorF(D2D1::ColorF::White));
    brush->SetColor(D2D1::ColorF(0.1f,0.18f,0.32f));
    DrawUiText(d2d.Get(),dw.Get(),L"Atrium 12:34 玻璃效果",format.Get(),D2D1::RectF(62,115,600,210),brush.Get());
    const float textWidth=MeasureUiText(dw.Get(),L"Atrium 12:34 玻璃效果",29,DWRITE_FONT_WEIGHT_MEDIUM);
    d2d->DrawLine(D2D1::Point2F(62,205),D2D1::Point2F(62+textWidth,205),brush.Get(),1);
    save(L"024_directwrite_ui_font.png");
    ComPtr<ID2D1DeviceContext2> d2d2; Check(d2d.As(&d2d2));
    ComPtr<IDWriteFactory3> dw3;
    Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(Out(dw3))));
    BarPainter bar(d2d2.Get(),dw3.Get());
    BarScene scene{};
    scene.bar=D2D1::RectF(68,104,572,216); scene.windowW=float(W); scene.windowH=float(H);
    scene.scale=1; scene.plateRadius=22; scene.separatorX=448;
    scene.clock=D2D1::RectF(466,124,558,197); scene.timePx=20; scene.datePx=13;
    scene.hoverKey=L"app-2"; scene.pointerX=264; scene.pointerY=158;
    scene.magnify=1.45f; scene.magnifyRange=115;
    auto addItem=[&](const std::wstring& key,float x,const std::wstring& glyph,bool extra) {
        SceneItem item{}; item.key=key; item.icon=D2D1::RectF(x,135,x+46,181);
        item.plate=D2D1::RectF(x-7,125,x+53,191); item.glyph=glyph;
        item.running=true; item.active=(key==L"app-2");
        if(extra) scene.extras.push_back(item); else scene.items.push_back(item);
    };
    addItem(L"#start",86,L"",true);
    addItem(L"app-1",164,L"\uE8A7",false);
    addItem(L"app-2",235,L"\uE8B7",false);
    addItem(L"app-3",306,L"\uE80F",false);
    addItem(L"#status",382,L"",true);
    scene.status.online=true; scene.status.connected=true; scene.status.wifi=true;
    scene.status.signal=70; scene.status.battery=true; scene.status.batteryPercent=80;
    scene.status.audio=true; scene.status.volume=50;
    for(int i=0;i<30;++i) bar.Animate(scene,1000+i*16.666);
    d2d->BeginDraw(); d2d->Clear(D2D1::ColorF(D2D1::ColorF::White));
    bar.Draw(d2d2.Get(),scene,Ink{.8f,.8f},L"12:34",L"11/17",L"benchmark",true);
    save(L"026_bar_painter_content.png");
    Surface host(GetModuleHandleW(nullptr),L"Atrium baseline widget host",false,0);
    host.SetGlassless(true);
    WidgetContent content{};
    content.text=L"Atrium visual baseline\nSecond line of notes";
    content.items={{L"Review glass edge",false},{L"Compare pixel output",true},{L"Test hover wave",false}};
    WidgetWorld world{};
    world.now.wYear=2026; world.now.wMonth=11; world.now.wDay=17;
    world.now.wDayOfWeek=2; world.now.wHour=12; world.now.wMinute=34;
    world.city=L"Shanghai"; world.cpu=.37f; world.memory=.64f; world.disk=.48f;
    world.memoryGb=10.2; world.memoryTotalGb=16; world.diskFreeGb=240;
    world.status=scene.status; world.status.batteryPercent=80;
    world.weather.ok=true; world.weather.city=L"Shanghai"; world.weather.temp=18;
    world.weather.hi=23; world.weather.lo=13; world.weather.code=2; world.weather.day=true;
    world.weather.hours={{L"Now",2,true,18,0},{L"14:00",0,true,21,0},{L"16:00",3,true,20,0},
                         {L"Sunset",0,true,0,2},{L"20:00",61,false,16,0},{L"22:00",45,false,14,0}};
    world.weather.days={{L"Today",2,13,23},{L"Wed",61,12,19},{L"Thu",3,10,18},
                        {L"Fri",0,11,21},{L"Sat",71,7,15}};
    const wchar_t* kinds[]{L"clock",L"calendar",L"battery",L"digital",L"system",L"notes",L"reminders",L"photos",L"weather",L"bluetooth",L"network"};
    const wchar_t* sizes[]{L"small",L"medium",L"large"};
    const wchar_t* materials[]{L"glass",L"dusk",L"color"};
    int widgetNumber=27;
    IconImage icon{}; icon.width=64; icon.height=64; icon.bgra.resize(64*64*4);
    for(int y=0;y<64;++y) for(int x=0;x<64;++x) {
        size_t at=(size_t(y)*64+x)*4;
        icon.bgra[at]=uint8_t(x*4); icon.bgra[at+1]=uint8_t(y*4);
        icon.bgra[at+2]=uint8_t(255-x*2); icon.bgra[at+3]=255;
    }
    host.paint=[&](Canvas&) {
        for(int ki=0;ki<11;++ki) for(int si=0;si<3;++si) for(int mi=0;mi<3;++mi) {
            auto kind=static_cast<WidgetKind>(ki);
            auto size=static_cast<WidgetSize>(si);
            if(!WidgetSupports(kind,size)) continue;
            d2d->BeginDraw(); d2d->Clear(D2D1::ColorF(D2D1::ColorF::White));
            d2d->SetTransform(D2D1::Matrix3x2F::Identity());
            Canvas c(host,d2d.Get(),1,D2D1::Matrix3x2F::Identity());
            c.panel=false; c.theme=Theme::For(false); c.now=100000;
            std::vector<WidgetHit> hits;
            PaintWidget(c,kind,size,static_cast<WidgetMaterial>(mi),content,world,hits);
            Check(d2d->EndDraw());
            wchar_t name[128]{};
            swprintf_s(name,L"%03d_widget_%s_%s_%s.png",widgetNumber++,kinds[ki],sizes[si],materials[mi]);
            Png(out/name,Read(dev,gpu,tex.Get()),W,UI_H);
        }
        const auto blue=D2D1::ColorF(0.12f,0.36f,0.85f);
        const auto orange=D2D1::ColorF(0.95f,0.38f,0.12f);
        const auto box=D2D1::RectF(115,110,425,310);
        using Draw=std::function<void(Canvas&)>;
        std::vector<std::pair<const wchar_t*,Draw>> primitives{
            {L"fill",[&](Canvas& c){c.Fill(box,36,blue);}},
            {L"stroke",[&](Canvas& c){c.Stroke(box,36,blue,8);}},
            {L"line",[&](Canvas& c){c.Line(115,280,425,130,blue,9);}},
            {L"capsule",[&](Canvas& c){c.Capsule(115,280,425,130,blue,15);}},
            {L"circle",[&](Canvas& c){c.Circle(270,210,95,blue);}},
            {L"gradient",[&](Canvas& c){c.Gradient(box,36,blue,orange);}},
            {L"crescent",[&](Canvas& c){c.Crescent(270,210,100,36,-25,87,blue);}},
            {L"arc",[&](Canvas& c){c.Arc(270,210,100,35,300,blue,12);}},
            {L"text",[&](Canvas& c){c.Text(L"Atrium 12:34 玻璃",box,35,blue);}},
            {L"glyph",[&](Canvas& c){c.Glyph(L"\uE80F",box,120,blue);}},
            {L"wifi",[&](Canvas& c){c.WifiFan(270,210,95,2,blue);}},
            {L"callout",[&](Canvas& c){c.Callout(D2D1::RectF(115,100,425,280),30,270,35,35,8,blue);}},
            {L"group",[&](Canvas& c){c.BeginGroup(.65f,.8f,270,210,25,0);c.Fill(box,36,blue);c.EndGroup();}},
            {L"image",[&](Canvas& c){c.Image(L"image",icon,box);if(!c.Cached(L"image")) throw std::runtime_error("Canvas image cache failed");}},
            {L"image_circle",[&](Canvas& c){c.ImageCircle(L"circle",icon,D2D1::RectF(170,110,370,310));}},
            {L"image_rounded",[&](Canvas& c){c.ImageRounded(L"rounded",icon,box,36);}},
            {L"image_perspective",[&](Canvas& c){D2D1_MATRIX_4X4_F m{};m._11=m._22=m._33=m._44=1;c.ImagePerspective(L"perspective",icon,box,m);}},
            {L"ripple",[&](Canvas& c){c.Fill(box,30,blue);c.Ripple(box,30,270,210,.45f,D2D1::ColorF(1,1,1,.8f));}},
            {L"clip",[&](Canvas& c){c.Clip(D2D1::RectF(180,150,360,270));c.Circle(270,210,120,blue);c.Unclip();}},
            {L"blur",[&](Canvas& c){c.BeginBlur();c.Circle(270,210,80,blue);c.EndBlur(18);}},
            {L"mask_rounded",[&](Canvas& c){c.Gradient(D2D1::RectF(0,0,640,480),0,blue,orange);c.MaskRounded(box,42);}},
            {L"caret_measure",[&](Canvas& c){auto r=D2D1::RectF(115,160,425,260);auto caret=c.Caret(L"Atrium test",r,32,DWRITE_FONT_WEIGHT_NORMAL,6);c.Text(L"Atrium test",r,32,blue);c.Fill(caret,0,orange);c.Line(115,280,115+c.TextWidth(L"Atrium test",32),280,blue,2);}}
        };
        int n=105;
        for(const auto& [label,draw]:primitives) {
            d2d->BeginDraw();d2d->Clear(D2D1::ColorF(D2D1::ColorF::White));
            d2d->SetTransform(D2D1::Matrix3x2F::Identity());
            Canvas c(host,d2d.Get(),1,D2D1::Matrix3x2F::Identity());
            c.theme=Theme::For(false);c.panel=false;c.now=100000;
            draw(c);Check(d2d->EndDraw());
            wchar_t name[128]{};swprintf_s(name,L"%03d_canvas_%s.png",n++,label);
            Png(out/name,Read(dev,gpu,tex.Get()),W,UI_H);
        }
    };
    host.Show(RECT{-10000,-10000,-9360,-9520},false,POINT{-10000,-10000},false);
    host.Hide(false);
    d2d->SetTarget(nullptr);
}

void RegionScene(const fs::path& path)
{
    HRGN region=ContinuousRegion(RECT{84,74,556,246},42);
    if(!region) throw std::runtime_error("ContinuousRegion failed");
    std::vector<uint8_t> pixels(W*H*4,255);
    for(UINT y=0;y<H;++y) for(UINT x=0;x<W;++x) if(PtInRegion(region,int(x),int(y))) {
        size_t i=(size_t(y)*W+x)*4;
        pixels[i]=192; pixels[i+1]=85; pixels[i+2]=49;
    }
    DeleteObject(region); Png(path,pixels,W,H);
}

void HoverWaveScene(const fs::path& path)
{
    BarScene scene{};
    scene.bar=D2D1::RectF(84,160,556,246);
    scene.magnify=1.7f; scene.magnifyRange=125; scene.pointerX=320; scene.pointerY=180;
    HoverWave wave;
    for(int i=0;i<30;++i) wave.Update(scene,16.666);
    std::vector<uint8_t> pixels(W*H*4,255);
    for(int x=106;x<=534;x+=32) {
        int cx=int(std::lround(wave.MapX(float(x))));
        int width=std::max(2,int(std::lround(10*wave.Scale(float(x),10))));
        int top=H/2-int(std::lround(26*wave.Scale(float(x),10)));
        int bottom=H/2+int(std::lround(26*wave.Scale(float(x),10)));
        for(int yy=top;yy<bottom;++yy) for(int xx=cx-width/2;xx<=cx+width/2;++xx)
            if(xx>=0&&xx<int(W)&&yy>=0&&yy<int(H)) {
                size_t i=(size_t(yy)*W+xx)*4;
                pixels[i]=55; pixels[i+1]=95; pixels[i+2]=220;
            }
    }
    Png(path,pixels,W,H);
}

void CompareHeatmap(const fs::path& path, ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* cur)
{
    constexpr UINT tile=16, tilesX=W/tile, tilesY=H/tile, count=2+tilesX*tilesY;
    auto old=Texture(dev,W,H,D3D11_BIND_SHADER_RESOURCE);
    auto baseline=Background(); ctx->UpdateSubresource(old.Get(),0,nullptr,baseline.data(),W*4,0);
    ComPtr<ID3D11ShaderResourceView> curView,oldView;
    Check(dev->CreateShaderResourceView(cur,nullptr,Out(curView)));
    Check(dev->CreateShaderResourceView(old.Get(),nullptr,Out(oldView)));
    D3D11_BUFFER_DESC desc{}; desc.ByteWidth=count*4; desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS; desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; desc.StructureByteStride=4;
    ComPtr<ID3D11Buffer> buffer; Check(dev->CreateBuffer(&desc,nullptr,Out(buffer)));
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud{}; ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements=count;
    ComPtr<ID3D11UnorderedAccessView> uav; Check(dev->CreateUnorderedAccessView(buffer.Get(),&ud,Out(uav)));
    ComPtr<ID3D11ComputeShader> shader; Check(dev->CreateComputeShader(g_compare_cs,sizeof(g_compare_cs),nullptr,Out(shader)));
    struct Params { UINT width,height,tilesX,tileSize; } cp{W,H,tilesX,tile};
    D3D11_BUFFER_DESC cbd{}; cbd.ByteWidth=16; cbd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> cb; Check(dev->CreateBuffer(&cbd,nullptr,Out(cb)));
    ctx->UpdateSubresource(cb.Get(),0,nullptr,&cp,0,0);
    UINT zero[4]{}; ctx->ClearUnorderedAccessViewUint(uav.Get(),zero);
    ID3D11ShaderResourceView* views[]{curView.Get(),oldView.Get()};
    ID3D11UnorderedAccessView* u=uav.Get(); ID3D11Buffer* c=cb.Get();
    ctx->CSSetShader(shader.Get(),nullptr,0); ctx->CSSetShaderResources(0,2,views);
    ctx->CSSetUnorderedAccessViews(0,1,&u,nullptr); ctx->CSSetConstantBuffers(0,1,&c);
    ctx->Dispatch(tilesX,tilesY,1);
    ID3D11ShaderResourceView* emptyViews[2]{}; ID3D11UnorderedAccessView* emptyU=nullptr;
    ctx->CSSetShaderResources(0,2,emptyViews); ctx->CSSetUnorderedAccessViews(0,1,&emptyU,nullptr);
    D3D11_BUFFER_DESC stagingDesc=desc; stagingDesc.Usage=D3D11_USAGE_STAGING; stagingDesc.BindFlags=0;
    stagingDesc.MiscFlags=0; stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging; Check(dev->CreateBuffer(&stagingDesc,nullptr,Out(staging)));
    ctx->CopyResource(staging.Get(),buffer.Get());
    D3D11_MAPPED_SUBRESOURCE map{}; Check(ctx->Map(staging.Get(),0,D3D11_MAP_READ,0,&map));
    auto values=static_cast<const UINT*>(map.pData);
    if(values[0]!=79900 || values[1]!=64) throw std::runtime_error("Compare shader count or maximum changed");
    for(UINT ty=0;ty<tilesY;++ty) for(UINT tx=0;tx<tilesX;++tx) {
        const bool intersects=tx*tile<555 && (tx+1)*tile>85 && ty*tile<245 && (ty+1)*tile>75;
        if(values[2+ty*tilesX+tx]!=(intersects?64u:0u))
            throw std::runtime_error("Compare shader tile maximum changed");
    }
    std::vector<uint8_t> pixels(W*H*4);
    for(UINT y=0;y<H;++y) for(UINT x=0;x<W;++x) {
        UINT v=values[2+(y/tile)*tilesX+x/tile]; size_t i=(y*W+x)*4;
        pixels[i]=0; pixels[i+1]=static_cast<uint8_t>(255-v); pixels[i+2]=static_cast<uint8_t>(v); pixels[i+3]=255;
    }
    std::cout << "compare CS changed pixels=" << values[0] << " max channel delta=" << values[1] << '\n';
    ctx->Unmap(staging.Get(),0); Png(path,pixels,W,H);
}

int wmain(int argc, wchar_t** argv)
{
    try {
        if(argc==3 && wcscmp(argv[1],L"--profile")==0) {
            RunGpuProfile(fs::absolute(argv[2]));
            return 0;
        }
        if(argc==3 && wcscmp(argv[1],L"--blur-compare")==0) {
            RunBlurCompare(fs::absolute(argv[2]));
            return 0;
        }
        if(argc!=2) { std::wcerr << L"Usage: atrium_graphics_baseline.exe OUTPUT_DIRECTORY\n"; return 2; }
        Check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
        fs::path out=fs::absolute(argv[1]); fs::create_directories(out);
        ComPtr<ID3D11Device> dev; ComPtr<ID3D11DeviceContext> ctx;
        D3D_FEATURE_LEVEL level{};
        const D3D_FEATURE_LEVEL requested[]{D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
        auto create=[&](D3D_DRIVER_TYPE type) { return D3D11CreateDevice(nullptr,type,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,requested,2,D3D11_SDK_VERSION,Out(dev),&level,Out(ctx)); };
        HRESULT hr=create(D3D_DRIVER_TYPE_HARDWARE);
        bool hardware=SUCCEEDED(hr);
        if(!hardware) Check(create(D3D_DRIVER_TYPE_WARP));
        ComPtr<IDXGIDevice> dxgi; Check(dev.As(&dxgi));
        ComPtr<IDXGIAdapter> adapter; Check(dxgi->GetAdapter(Out(adapter)));
        DXGI_ADAPTER_DESC ad{}; Check(adapter->GetDesc(&ad));
        std::wcout << L"Renderer: " << ad.Description << (hardware?L" [hardware]\n":L" [WARP]\n");
        auto background=Background(); Png(out/L"000_synthetic_backdrop.png",background,W,H);
        auto bg=Texture(dev.Get(),W,H,D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET,D3D11_RESOURCE_MISC_GENERATE_MIPS,0);
        ctx->UpdateSubresource(bg.Get(),0,nullptr,background.data(),W*4,0);
        ComPtr<ID3D11ShaderResourceView> srv; Check(dev->CreateShaderResourceView(bg.Get(),nullptr,Out(srv)));
        ctx->GenerateMips(srv.Get());
        auto target=Texture(dev.Get(),W,H,D3D11_BIND_RENDER_TARGET);
        ComPtr<ID3D11RenderTargetView> rt; Check(dev->CreateRenderTargetView(target.Get(),nullptr,Out(rt)));
        ComPtr<ID3D11VertexShader> vs; Check(dev->CreateVertexShader(g_glass_vs,sizeof(g_glass_vs),nullptr,Out(vs)));
        ComPtr<ID3D11PixelShader> ps; Check(dev->CreatePixelShader(g_glass_ps,sizeof(g_glass_ps),nullptr,Out(ps)));
        D3D11_SAMPLER_DESC sd{}; sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxLOD=D3D11_FLOAT32_MAX;
        ComPtr<ID3D11SamplerState> sampler; Check(dev->CreateSamplerState(&sd,Out(sampler)));
        D3D11_BUFFER_DESC cbd{}; cbd.ByteWidth=sizeof(GlassParams); cbd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        ComPtr<ID3D11Buffer> constants; Check(dev->CreateBuffer(&cbd,nullptr,Out(constants)));
        struct Scene { const wchar_t* file; GlassParams p; };
        std::vector<Scene> scenes;
        auto add=[&](const wchar_t* name,GlassParams p){scenes.push_back({name,p});};
        auto base=Base(); add(L"001_glass_liquid.png",base);
        auto p=base; p.blurLod=4.5f; p.blurMix=1; p.refract=5; add(L"002_glass_frosted.png",p);
        p=base; p.shapeT=112; p.shapeB=208; p.radius=48; add(L"003_glass_pill.png",p);
        p=base; p.dispersion=0; add(L"004_glass_no_dispersion.png",p);
        p=base; p.rippleX=320; p.rippleY=160; p.rippleR=70; p.rippleA=.8f; add(L"005_glass_ripple.png",p);
        p=base; p.waveX=320; p.waveWidth=160; p.waveHeight=30; add(L"006_glass_hover_wave.png",p);
        p=base; p.tailX=320; p.tailHalfWidth=24; p.tailHeight=28; p.tailInset=7; add(L"007_glass_callout_tail.png",p);
        p=base; p.partCount=2; p.melt=12; p.parts[0][0]=84; p.parts[0][1]=74; p.parts[0][2]=335; p.parts[0][3]=246;
        p.parts[1][0]=305; p.parts[1][1]=90; p.parts[1][2]=556; p.parts[1][3]=230;
        for(int i=0;i<2;++i) for(float& r:p.partRadii[i]) r=28;
        add(L"008_glass_smooth_union.png",p);
        p.melt=0; add(L"009_glass_hard_union.png",p);
        p.partMix=.5f; p.melt=12; add(L"010_glass_union_morph.png",p);
        p=base; p.fade=.5f; add(L"011_glass_fade.png",p);
        p=base; p.tintR=.2f; p.tintG=.6f; p.tintB=1; p.tintMix=.72f; add(L"020_glass_tint.png",p);
        p=base; p.partCount=1; p.parts[0][0]=84; p.parts[0][1]=74; p.parts[0][2]=556; p.parts[0][3]=246;
        p.partRadii[0][0]=8; p.partRadii[0][1]=58; p.partRadii[0][2]=20; p.partRadii[0][3]=45;
        add(L"021_glass_asymmetric_corners.png",p);
        p=base; p.partCount=6; p.melt=14;
        for(int i=0;i<6;++i) {
            p.parts[i][0]=74.f+i*82.f; p.parts[i][1]=76.f+(i%2)*34.f;
            p.parts[i][2]=p.parts[i][0]+118.f; p.parts[i][3]=240.f-(i%2)*34.f;
            for(float& r:p.partRadii[i]) r=24;
        }
        add(L"022_glass_six_part_union.png",p);
        p=base; p.refract=0; p.dispersion=0; p.blurMix=0; p.highlight=0; p.shadow=0;
        add(L"023_glass_flat_control.png",p);
        for(const auto& scene:scenes) {
            SaveGlass(out/scene.file,dev.Get(),ctx.Get(),vs.Get(),ps.Get(),srv.Get(),sampler.Get(),constants.Get(),target.Get(),rt.Get(),scene.p,background);
            std::wcout << scene.file << '\n';
        }
        CpuShape(out/L"012_cpu_g3_corner.png",false);
        CpuShape(out/L"013_cpu_callout_profile.png",true);
        auto changed=Texture(dev.Get(),W,H,D3D11_BIND_SHADER_RESOURCE);
        auto shifted=background; for(UINT y=75;y<245;++y) for(UINT x=85;x<555;++x) shifted[(y*W+x)*4+2]^=0x40;
        ctx->UpdateSubresource(changed.Get(),0,nullptr,shifted.data(),W*4,0);
        CompareHeatmap(out/L"014_compare_cs_heatmap.png",dev.Get(),ctx.Get(),changed.Get());
        BackdropEdge(out/L"015_backdrop_edge_clamp.png",dev.Get(),ctx.Get(),vs.Get(),srv.Get(),target.Get(),rt.Get());
        {
            Gpu rendererGpu{};
            Check(CreateDXGIFactory1(IID_PPV_ARGS(rendererGpu.factory.put())));
            rendererGpu.device.copy_from(dev.Get());
            rendererGpu.context.copy_from(ctx.Get());
            HWND window=CreateWindowExW(0,L"STATIC",L"Atrium glass baseline",WS_POPUP|WS_VISIBLE,
                                        -10000,-10000,int(W),int(H),nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            if(!window) throw std::runtime_error("GlassRenderer window creation failed");
            {
                GlassRenderer renderer(rendererGpu,window,int(W),int(H),false,false);
                renderer.UpdateBackdrop(bg.Get(),0,0);
                baselineCapturePath=out/L"144_glass_renderer_present.png";
                if(!renderer.Present(Base(),{})) throw std::runtime_error("GlassRenderer Present failed");
                baselineCapturePath=out/L"145_glass_renderer_multi_shape.png";
                auto a=Base(),b=Base();
                a.shapeL=70;a.shapeT=55;a.shapeR=370;a.shapeB=245;a.radius=42;
                b.shapeL=280;b.shapeT=85;b.shapeR=565;b.shapeB=225;b.radius=24;b.tintMix=.6f;
                b.tintR=.25f;b.tintG=.6f;b.tintB=1;
                if(!renderer.Present(std::vector<GlassParams>{a,b},{})) throw std::runtime_error("GlassRenderer multi shape Present failed");
                baselineCapturePath=out/L"146_glass_renderer_complex_parts.png";
                auto complex=Base();complex.partCount=2;complex.melt=12;
                complex.parts[0][0]=70;complex.parts[0][1]=55;complex.parts[0][2]=370;complex.parts[0][3]=245;
                complex.parts[1][0]=280;complex.parts[1][1]=85;complex.parts[1][2]=565;complex.parts[1][3]=225;
                for(auto& radii:complex.partRadii) for(float& corner:radii) corner=30;
                if(!renderer.Present(complex,{})) throw std::runtime_error("GlassRenderer complex shape Present failed");
                baselineCapturePath.clear();
            }
            DestroyWindow(window);
        }
        Direct2DScenes(out,dev.Get(),ctx.Get());
        HoverWaveScene(out/L"019_hover_wave_mapping.png");
        RegionScene(out/L"025_continuous_input_region.png");
        {
            GlassMenu menu(GetModuleHandleW(nullptr));
            std::vector<MenuEntry> entries;
            MenuEntry heading{};heading.text=L"Atrium menu";heading.header=true;entries.push_back(heading);
            MenuEntry open{};open.text=L"Open settings";open.id=1;entries.push_back(open);
            MenuEntry checked{};checked.text=L"Keep on top";checked.id=2;checked.checked=true;entries.push_back(checked);
            entries.push_back(MenuEntry{});
            MenuEntry disabled{};disabled.text=L"Unavailable action";disabled.id=3;disabled.disabled=true;entries.push_back(disabled);
            baselineCapturePath=out/L"127_glass_menu.png";
            menu.Open(std::move(entries),POINT{200,200},MenuPlace::Below,[](int){});
            baselineCapturePath.clear();
            menu.Close();
        }
        {
            CalendarPanel calendar(GetModuleHandleW(nullptr));
            baselineCapturePath=out/L"128_calendar_panel.png";
            calendar.Open(RECT{500,900,600,950},RECT{100,950,600,1020});
            baselineCapturePath.clear();
            calendar.Close();
        }
        {
            Notifications notices(GetModuleHandleW(nullptr));
            notices.bellRect=[] {return RECT{520,930,550,960};};
            notices.barRect=[] {return RECT{100,960,600,1030};};
            Notice n{};n.id=1;n.source=L"test";n.app=L"Atrium";
            n.title=L"Visual baseline";n.body=L"Notification card with reproducible text";
            baselineCapturePath=out/L"129_notification_popup.png";
            notices.Post(n,false);
            baselineCapturePath=out/L"130_notification_center.png";
            notices.ToggleCenter(true);
            baselineCapturePath.clear();
        }
        {
            ControlCenter center(GetModuleHandleW(nullptr));
            SystemStatus status{};status.connected=true;status.online=true;status.wifi=true;status.signal=70;
            status.battery=true;status.batteryPercent=80;status.audio=true;status.volume=50;
            center.SetStatus(status);
            baselineCapturePath=out/L"131_control_center_main.png";
            center.Open(RECT{530,920,580,960},RECT{100,960,600,1030});
            const wchar_t* pages[]{L"wifi",L"bluetooth",L"sound",L"cast"};
            for(int i=0;i<4;++i) {
                wchar_t name[96]{};swprintf_s(name,L"%03d_control_center_%s.png",132+i,pages[i]);
                baselineCapturePath=out/name;
                center.BaselineShowPage(i+1);
            }
            baselineCapturePath.clear();center.Close();
        }
        {
            HWND sample=CreateWindowExW(0,L"STATIC",L"Atrium sample window",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                        -10000,-10000,480,320,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            if(!sample) throw std::runtime_error("Sample window creation failed");
            IconImage icon{};icon.width=64;icon.height=64;icon.bgra.assign(64*64*4,255);
            WindowPreview preview(GetModuleHandleW(nullptr));
            baselineCapturePath=out/L"136_window_preview.png";
            preview.Show(L"sample",std::vector<HWND>{sample},POINT{350,950},icon);
            baselineCapturePath.clear();preview.Close();DestroyWindow(sample);
        }
        {
            SetEnvironmentVariableW(L"ATRIUM_PRIVACY",L"1");
            TaskModel tasks(nullptr,[]{});
            AppCatalog catalog(nullptr,0);
            StartMenu start(GetModuleHandleW(nullptr),tasks,catalog);
            baselineCapturePath=out/L"137_start_menu.png";
            start.Open(RECT{100,960,600,1030},RECT{110,970,155,1015},nullptr);
            Sleep(700); start.Refresh();
            const wchar_t* views[]{L"all_apps",L"power",L"search"};
            for(int i=0;i<3;++i) {
                wchar_t name[80]{};swprintf_s(name,L"%03d_start_%s.png",139+i,views[i]);
                baselineCapturePath=out/name;start.BaselineView(i+1);
                Sleep(350);RedrawWindow(start.Hwnd(),nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
            }
            baselineCapturePath.clear();start.Close(false);
        }
        {
            TrayModel model(nullptr,0);
            TrayPanel tray(GetModuleHandleW(nullptr),model);
            baselineCapturePath=out/L"138_tray_panel.png";
            tray.Open(RECT{500,940,550,960},RECT{100,960,600,1030},nullptr);
            Sleep(450);tray.Refresh();
            baselineCapturePath.clear();tray.Close();
        }
        {
            TaskModel tasks(nullptr,[]{});
            StageManager stage(GetModuleHandleW(nullptr),tasks);
            baselineCapturePath=out/L"142_stage_picture_perspective.png";
            stage.BaselineRender();
            baselineCapturePath.clear();
        }
        {
            DesktopWidgets widgets(GetModuleHandleW(nullptr));
            baselineCapturePath=out/L"143_desktop_widget_container.png";
            widgets.BaselineRender();
            baselineCapturePath.clear();
        }
        std::ofstream info(out/L"manifest.txt");
        info << "Atrium graphics baseline; source shaders from native/core/render/shaders; 640x320 BGRA8; deterministic synthetic backdrop.\n";
        info << "000: synthetic input; 001-011 and 020-023: original glass.hlsl VS/PS on D3D11; 012-013: original CPU shape functions; 014: compare.hlsl CS; 015: backdrop_edge.hlsl PS; 016-018: original Direct2D corners and Wi-Fi; 019: HoverWave; 024: DirectWrite font; 025: input region; 026: BarPainter; 027-104: 78 widget variants; 105-126: 22 Canvas API scenes; 127: GlassMenu; 128: CalendarPanel; 129-130: Notifications; 131-135: ControlCenter pages; 136: WindowPreview overlay; 137 and 139-141: StartMenu pages; 138: TrayPanel; 142: StageManager; 143: DesktopWidgets; 144-146: GlassRenderer Present (single, layered, complex).\n";
        info << "GPU rendering time is NOT measured. Live capture, window composition output, and system interaction states are not covered.\n";
        CoUninitialize(); return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
