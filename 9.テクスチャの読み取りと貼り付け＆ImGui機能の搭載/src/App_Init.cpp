//-----------------------------------------------------------------------------
// File : App_Init.cpp
// Desc : 初期化の入口・ウィンドウ・Direct3D の初期化(コンストラクタ・InitApp・InitWnd・InitD3D)
//-----------------------------------------------------------------------------
#include "App.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <Timer.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"


using namespace DirectX;

// ウィンドウクラス名とウィンドウ名の定義
namespace/*anonymous*/ {
    // ClassName は終了処理(TermWnd)でも使うので App.h の static メンバにした
    const auto WindowName = TEXT("SampleWindow");//ウィンドウ名
}

template<typename T>
void SafeRelease(T*& ptr)
{
    if (ptr != nullptr)
    {
        ptr->Release();
        ptr = nullptr;
    }
}

App::App(uint32_t width, uint32_t height)
    :m_hInst(nullptr) //インスタンスハンドル
    , m_hWnd(nullptr) //ウィンドウハンドル
    , m_Width(width)
    , m_Height(height)
    , m_FenceEvent(nullptr)
    , m_FenceCounter{}
    , m_FrameIndex(0)
    , m_HandleRTV{}
    , m_VBV{}
    , m_Viewport{}
    , m_Scissor{}
{
}

App::~App()
{

}

//初期化処理
bool App::InitApp()
{
    // COMの初期化(WICで画像を読み込むのに必要)
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
    {
        MessageBoxW(nullptr, L"CoInitializeEx で失敗", L"エラー", MB_OK);
        return false;
    }
    //ウィンドウの初期化
    if (!InitWnd())
    {
        MessageBoxW(nullptr, L"InitWnd で失敗", L"エラー", MB_OK);
        return false;
    }
    //Direct3D12の初期化
    if (!InitD3D())
    {
        MessageBoxW(nullptr, L"InitD3D で失敗", L"エラー", MB_OK);
        return false;
    }
    // ImGuiの初期化
    if (!InitImGui())
    {
        MessageBoxW(nullptr, L"InitImGui で失敗", L"エラー", MB_OK);
        return false;
    }
    if (!OnInit())
    {
        MessageBoxW(nullptr, L"OnInit で失敗", L"エラー", MB_OK);
        return false;
    }
    m_Timer.Init();

    return true;
}

bool App::InitWnd()//ウィンドウの初期化処理
{
    auto hInst = GetModuleHandle(nullptr);
    if (hInst == nullptr)return false;

    //ウィンドウの設定
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;//ウィンドウのスタイル(再描画)
    wc.lpfnWndProc = WndProc;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);//アイコンの設定
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);//カーソルの設定
    wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);//背景色の設定
    wc.lpszMenuName = nullptr;//メニューの設定
    wc.lpszClassName = ClassName;//クラス名の設定
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);//小さいアイコンの設定

    //ウィンドウの登録
    if (!RegisterClassEx(&wc))return false;

    //インスタンスハンドル設定
    m_hInst = hInst;

    //ウィンドウのサイズの調整
    auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;

    RECT rc = {};
    rc.right = static_cast<LONG>(m_Width);
    rc.bottom = static_cast<LONG>(m_Height);
    AdjustWindowRect(&rc, style, FALSE);

    //ウィンドウの作成
    m_hWnd = CreateWindowEx(
        0,//拡張スタイル
        ClassName,//クラス名
        WindowName,//ウィンドウ名
        style,//ウィンドウスタイル
        CW_USEDEFAULT,//X座標
        CW_USEDEFAULT,//Y座標
        rc.right - rc.left,//幅
        rc.bottom - rc.top,//高さ
        nullptr,//親ウィンドウハンドル
        nullptr,//メニューハンドル
        m_hInst,//インスタンスハンドル
        nullptr//追加パラメータ
    );
    if (m_hWnd == nullptr)
    {
        return false;
    }
    //ウィンドウの表示
    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);

    //ウィンドウにフォーカスを設定
    SetFocus(m_hWnd);

    return true;
}

bool App::InitD3D()
{
    //0.デバッグのチェック
#if defined(DEBUG)||defined(_DEBUG)
    {
        ComPtr<ID3D12Debug>debug;
        auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf()));

        //デバッグレイヤーを有効化
        if (SUCCEEDED(hr))
        {
            debug->EnableDebugLayer();
        }
    }
#endif
//1.デバイスの生成
auto hr = D3D12CreateDevice(
    nullptr,//デフォルトアダプタ
    D3D_FEATURE_LEVEL_11_0,//機能レベル
    IID_PPV_ARGS(m_pDevice.GetAddressOf())
);
if (FAILED(hr))return false;
//2.コマンドキューの生成
{
    D3D12_COMMAND_QUEUE_DESC queue = {};
    queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue.NodeMask = 0;

    hr = m_pDevice->CreateCommandQueue(&queue, IID_PPV_ARGS(&m_pQueue));
    if (FAILED(hr))return false;
}
//3.Swap Chain
// 3.1 DXGI Factoryの生成(SwapChainはDXGIの機能)
IDXGIFactory4* pFactory = nullptr;
hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
if (FAILED(hr))return false;

// 3.2 Swap Chainの設定
DXGI_SWAP_CHAIN_DESC chain = {};
chain.BufferDesc.Width = m_Width;
chain.BufferDesc.Height = m_Height;
chain.BufferDesc.RefreshRate.Numerator = 60;
chain.BufferDesc.RefreshRate.Denominator = 1;
chain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
chain.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
chain.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
chain.SampleDesc.Count = 1;
chain.SampleDesc.Quality = 0;
chain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
chain.BufferCount = FrameCount;
chain.OutputWindow = m_hWnd;
chain.Windowed = TRUE;
chain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
chain.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

//生成
IDXGISwapChain* pSwapChain = nullptr;
hr = pFactory->CreateSwapChain(
    m_pQueue.Get(),//コマンドキュー
    &chain,//Swap Chainの設定
    &pSwapChain//Swap Chainの実体が入る
);
if (FAILED(hr)) {
    SafeRelease(pFactory);
    return false;
}

//IDXGISwapChain3を取得
hr = pSwapChain->QueryInterface(IID_PPV_ARGS(&m_pSwapChain));
if (FAILED(hr)) {
    //解放処理
    SafeRelease(pSwapChain);
    SafeRelease(pFactory);
    return false;
}
m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
SafeRelease(pSwapChain);
SafeRelease(pFactory);

//4.コマンドリスト(コマンドアロケータの生成)
{
    for (auto i = 0u; i < FrameCount; i++)
    {
        hr = m_pDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(m_pCommandAllocator[i].GetAddressOf()));
        if (FAILED(hr)) {
            return false;
        }
    }
}
//生成
{
    hr = m_pDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_pCommandAllocator[m_FrameIndex].Get(),
        nullptr,
        IID_PPV_ARGS(m_pCommandList.GetAddressOf()));
    if (FAILED(hr)) {
        return false;
    }
}
m_pCommandList->Close();

//5.RTV(ディスクリプタヒープ)
// 本来,ディスクリプタヒープは引数としてリソースとディスクリプタヒープが必要だが,スワップチェインで作成したバッファをリソースとするため、リソースを作成する必要はない
//ディスクリプタヒープはRTV用の情報を入れる配列
D3D12_DESCRIPTOR_HEAP_DESC heap = {};
heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//レンダーターゲットビュー用
heap.NumDescriptors = FrameCount;//リソース数と同じ
heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
heap.NodeMask = 0;
//デバイスの機能を使いディスクリプタヒープの生成
hr = m_pDevice->CreateDescriptorHeap(&heap, IID_PPV_ARGS(m_pHeapRTV.GetAddressOf()));
if (FAILED(hr))return false;
//生成
//RTV用ディスクリプタヒープの先頭のCPUディスクリプタハンドル(アドレス)を取得
auto handle = m_pHeapRTV->GetCPUDescriptorHandleForHeapStart();
//RTVディスクリプタ1個分のバイトサイズを取得,アドレス間の間隔サイズ
auto incrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
for (auto i = 0u; i < FrameCount; ++i) {
    hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(m_pColorBuffer[i].GetAddressOf()));//バックバッファのポインタの取得
    if (FAILED(hr)) { return false; }
    D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
    rtv.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;//sRGB
    rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;//2次元テクスチャとしてアクセス
    rtv.Texture2D.MipSlice = 0;
    rtv.Texture2D.PlaneSlice = 0;
    //生成
    m_pDevice->CreateRenderTargetView(m_pColorBuffer[i].Get(), &rtv, handle);
    m_HandleRTV[i] = handle;
    handle.ptr += incrementSize;
}

//6.フェンス(CPUとGPUの同期処理)
//生成
hr = m_pDevice->CreateFence(
    m_FenceCounter[m_FrameIndex],
    D3D12_FENCE_FLAG_NONE,
    IID_PPV_ARGS(&m_pFence)
);
if (FAILED(hr))return false;

m_FenceCounter[m_FrameIndex]++;
//イベントの生成
m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
if (m_FenceEvent == nullptr)return false;

return true;
}

bool App::InitImGui()
{
    // 1. ImGui専用のSRVヒープ(シェーダーから見える)
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 64;   // 1.92以降はフォント以外にも使うので余裕を持たせる
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_pHeapImGui.GetAddressOf()))))
        return false;
    m_ImGuiDescSize = m_pDevice->GetDescriptorHandleIncrementSize(desc.Type);
    m_ImGuiHeapNext = 0;

    // 2. コンテキスト作成とWin32側の初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!ImGui_ImplWin32_Init(m_hWnd)) return false;

    // 3. DX12側の初期化
    ImGui_ImplDX12_InitInfo info;
    info.Device = m_pDevice.Get();
    info.CommandQueue = m_pQueue.Get();
    info.NumFramesInFlight = FrameCount;
    info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;  // 267行目のRTVと同じ
    info.DSVFormat = DXGI_FORMAT_D32_FLOAT;            // Render でDSVを設定しているため
    info.UserData = this;
    info.SrvDescriptorHeap = m_pHeapImGui.Get();
    info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* i,
        D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
        {
            auto* app = static_cast<App*>(i->UserData);
            const UINT idx = app->m_ImGuiHeapNext++;
            *outCpu = app->m_pHeapImGui->GetCPUDescriptorHandleForHeapStart();
            *outGpu = app->m_pHeapImGui->GetGPUDescriptorHandleForHeapStart();
            outCpu->ptr += SIZE_T(idx) * app->m_ImGuiDescSize;
            outGpu->ptr += UINT64(idx) * app->m_ImGuiDescSize;
        };
    info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*,
        D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};  // 今回は解放しない
    return ImGui_ImplDX12_Init(&info);
}