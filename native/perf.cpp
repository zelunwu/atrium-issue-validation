#include "perf.h"
#include "render/glass_renderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cwchar>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace atrium {
namespace {
using Clock = std::chrono::steady_clock;
constexpr int kMarks = 11;
constexpr int kWarmup = 3;
constexpr int kSamples = 20;

struct Timers {
    ID3D11DeviceContext* context = nullptr;
    winrt::com_ptr<ID3D11Query> disjoint;
    std::array<winrt::com_ptr<ID3D11Query>, kMarks> stamps;
    std::array<bool, kMarks> seen{};

    explicit Timers(const Gpu& gpu) : context(gpu.context.get()) {
        D3D11_QUERY_DESC desc{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
        winrt::check_hresult(gpu.device->CreateQuery(&desc, disjoint.put()));
        desc.Query = D3D11_QUERY_TIMESTAMP;
        for (auto& stamp : stamps)
            winrt::check_hresult(gpu.device->CreateQuery(&desc, stamp.put()));
    }
    void Begin() {
        seen.fill(false);
        context->Begin(disjoint.get());
        context->End(stamps[0].get());
        seen[0] = true;
    }
    std::array<double, kMarks> Finish() {
        context->End(stamps[6].get());
        context->End(disjoint.get());
        context->Flush();
        const auto deadline = Clock::now() + std::chrono::seconds(10);
        auto ready = [&](ID3D11Query* query, void* data, UINT size) {
            while (context->GetData(query, data, size, 0) == S_FALSE) {
                if (Clock::now() > deadline) throw std::runtime_error("GPU timestamp timeout");
                Sleep(1);
            }
        };
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT info{};
        ready(disjoint.get(), &info, sizeof(info));
        if (info.Disjoint || !info.Frequency) throw std::runtime_error("GPU clock disjoint; rerun profile");
        std::array<UINT64, kMarks> ticks{};
        for (int i = 0; i < kMarks; ++i) {
            if (i > 6 && !seen[i]) continue;
            ready(stamps[i].get(), &ticks[i], sizeof(ticks[i]));
        }
        std::array<double, kMarks> ms{};
        for (int i = 1; i <= 6; ++i) {
            if (ticks[i] < ticks[i-1]) throw std::runtime_error("GPU timestamps out of order");
            ms[i] = double(ticks[i] - ticks[i-1]) * 1000.0 / double(info.Frequency);
        }
        if (seen[7] && seen[8]) ms[7] = double(ticks[8] - ticks[7]) * 1000.0 / double(info.Frequency);
        if (seen[9] && seen[10]) ms[8] = double(ticks[10] - ticks[9]) * 1000.0 / double(info.Frequency);
        return ms;
    }
};

Timers* active = nullptr;

winrt::com_ptr<ID3D11Texture2D> Backdrop(const Gpu& gpu, int width, int height, int variant) {
    std::vector<uint32_t> pixels(size_t(width) * size_t(height));
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const unsigned r = unsigned((x * 7 + y * 3 + variant * 19) & 255);
            const unsigned g = unsigned((x * 3 + y * 5 + variant * 11) & 255);
            const unsigned b = unsigned((x * 5 + y * 2 + variant * 13) & 255);
            pixels[size_t(y) * width + x] = 0xff000000u | (r << 16) | (g << 8) | b;
        }
    D3D11_TEXTURE2D_DESC desc{UINT(width), UINT(height), 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                              {1,0}, D3D11_USAGE_DEFAULT, 0, 0, 0};
    D3D11_SUBRESOURCE_DATA data{pixels.data(), UINT(width * 4), 0};
    winrt::com_ptr<ID3D11Texture2D> result;
    winrt::check_hresult(gpu.device->CreateTexture2D(&desc, &data, result.put()));
    return result;
}

GlassParams Params() {
    GlassParams p{};
    p.radius=42; p.edge=18; p.refract=12; p.dispersion=.035f;
    p.blurLod=4.5f; p.blurMix=.7f; p.highlight=.5f; p.shadow=.45f;
    p.lightX=-.6f; p.lightY=-.8f; p.inset=16;
    p.tintR=.55f; p.tintG=.7f; p.tintB=.95f; p.tintA=1; p.tintMix=.12f;
    return p;
}

struct Case { int width, height; bool edge, multi, zeroDispersion, complexParts; const char* name; };
}

void BaselinePerfMark(int index) {
    if (active && index > 0 && index < kMarks) {
        active->context->End(active->stamps[index].get());
        active->seen[index] = true;
    }
}

bool BaselineUseBasicShader() { return GetEnvironmentVariableW(L"ATRIUM_BASELINE_DISABLE_BASIC", nullptr, 0) == 0; }

void RunGpuProfile(const std::filesystem::path& csv) {
    winrt::check_hresult(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    Gpu gpu{};
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   levels, 2, D3D11_SDK_VERSION, gpu.device.put(), &level, gpu.context.put());
    const bool hardware = SUCCEEDED(hr);
    if (!hardware)
        winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                               levels, 2, D3D11_SDK_VERSION, gpu.device.put(), &level, gpu.context.put()));
    winrt::check_hresult(CreateDXGIFactory1(IID_PPV_ARGS(gpu.factory.put())));
    auto dxgi = gpu.device.as<IDXGIDevice>();
    winrt::com_ptr<IDXGIAdapter> adapter;
    winrt::check_hresult(dxgi->GetAdapter(adapter.put()));
    DXGI_ADAPTER_DESC adapterDesc{};
    winrt::check_hresult(adapter->GetDesc(&adapterDesc));
    std::wcout << L"GPU profile: " << adapterDesc.Description << (hardware ? L" [hardware]\n" : L" [WARP]\n");
    std::cout << "shader selection: basic=" << BaselineUseBasicShader() << '\n';
    wchar_t blurText[32]{};
    const DWORD blurChars=GetEnvironmentVariableW(L"ATRIUM_BASELINE_BLUR_SETTING",blurText,32);
    const double blurSetting=blurChars > 0 && blurChars < 32 ? wcstod(blurText,nullptr) : -1.0;
    if (blurChars && (blurChars >= 32 || blurSetting < 0 || blurSetting > 1))
        throw std::runtime_error("ATRIUM_BASELINE_BLUR_SETTING must be in [0,1]");
    if (blurSetting >= 0) std::cout << "blur setting=" << blurSetting << '\n';

    std::ofstream output(csv);
    if (!output) throw std::runtime_error("Cannot open GPU profile CSV");
    output << "case,content,sample,roi_copy_edge_ms,compare_mips_ms,present_prepare_ms,glass_draw_ms,final_copy_ms,present_queue_ms,gpu_total_ms,cpu_update_present_ms,mip_generation_ms,compare_dispatch_ms\n";
    const Case cases[]{{640,320,false,false,false,false,"640x320"}, {640,320,true,false,false,false,"640x320_edge"},
                       {1920,1080,false,false,false,false,"1920x1080"}, {2560,1440,false,false,false,false,"2560x1440"},
                       {1920,1080,false,true,false,false,"1920x1080_multi"},
                       {1920,1080,false,false,true,false,"1920x1080_zero_dispersion"},
                       {2560,1440,false,false,true,false,"2560x1440_zero_dispersion"},
                       {1920,1080,false,false,false,true,"1920x1080_complex_parts"}};
    for (const auto& scenario : cases) {
        HWND window = CreateWindowExW(0,L"STATIC",L"Atrium GPU profile",WS_POPUP|WS_VISIBLE,
                                      -10000,-10000,scenario.width,scenario.height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if (!window) throw std::runtime_error("GPU profile window creation failed");
        {
            GlassRenderer renderer(gpu,window,scenario.width,scenario.height,false,false);
            auto a = Backdrop(gpu,scenario.width+128,scenario.height+128,0);
            auto b = Backdrop(gpu,scenario.width+128,scenario.height+128,1);
            Timers timers(gpu);
            GlassParams first=Params();
            if (blurSetting >= 0) {
                first.blurMix=float(std::min(1.0,blurSetting*4.0));
                first.blurLod=float(1.0+4.5*blurSetting);
            }
            if (scenario.zeroDispersion) first.dispersion=0;
            if (scenario.complexParts) {
                first.partCount=2;first.melt=12;
                first.parts[0][0]=40;first.parts[0][1]=60;first.parts[0][2]=float(scenario.width)*.58f;first.parts[0][3]=float(scenario.height)-60;
                first.parts[1][0]=float(scenario.width)*.42f;first.parts[1][1]=90;
                first.parts[1][2]=float(scenario.width)-40;first.parts[1][3]=float(scenario.height)-90;
                for (auto& radius : first.partRadii) for (float& corner : radius) corner=42;
            }
            std::vector<GlassParams> shapes{first};
            if (scenario.multi) {
                first.shapeL=40;first.shapeT=50;first.shapeR=float(scenario.width)*.6f;first.shapeB=float(scenario.height)-50;
                GlassParams second=first;
                second.shapeL=float(scenario.width)*.4f;second.shapeT=90;
                second.shapeR=float(scenario.width)-40;second.shapeB=float(scenario.height)-90;
                shapes={first,second};
            }
            for (int changing=0; changing<2; ++changing) {
                for (int sample=-kWarmup; sample<kSamples; ++sample) {
                    auto frame=(changing && (sample&1)) ? b.get() : a.get();
                    const auto cpuStart=Clock::now();
                    timers.Begin();
                    active=&timers;
                    renderer.UpdateBackdrop(frame,scenario.edge?0:64,scenario.edge?0:64);
                    if (!renderer.Present(shapes,{})) throw std::runtime_error("GPU profile Present failed");
                    active=nullptr;
                    const auto cpuEnd=Clock::now();
                    const auto ms=timers.Finish();
                    if (sample>=0) {
                        const double gpuTotal=std::accumulate(ms.begin()+1,ms.begin()+7,0.0);
                        const double cpuMs=std::chrono::duration<double,std::milli>(cpuEnd-cpuStart).count();
                        output << scenario.name << ',' << (changing?"changing":"static") << ',' << sample;
                        for (int i=1;i<=6;++i) output << ',' << ms[i];
                        output << ',' << gpuTotal << ',' << cpuMs << ',' << ms[7] << ',' << ms[8] << '\n';
                    }
                    renderer.Poll();
                }
                std::cout << scenario.name << ' ' << (changing?"changing":"static") << " complete\n";
            }
        }
        DestroyWindow(window);
    }
    CoUninitialize();
}
} // namespace atrium
