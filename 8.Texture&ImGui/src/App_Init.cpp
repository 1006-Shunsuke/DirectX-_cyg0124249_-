//-----------------------------------------------------------------------------
// File : App_Init.cpp
// Desc : 初期化の入口・ウィンドウ・Direct3D の初期化。コンストラクタ・InitApp・InitWnd・InitD3D
//-----------------------------------------------------------------------------
#include "App.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"


using namespace DirectX;

// ウィンドウクラス名とウィンドウ名の定義
namespace/*anonymous*/
{
    // ClassName は終了処理の TermWnd でも使うので App.h の static メンバにした
    const auto WindowName = TEXT("SampleWindow"); // ウィンドウ名
}

App::App(uint32_t width, uint32_t height)
    :m_hInst(nullptr) // インスタンスハンドル
    , m_hWnd(nullptr) // ウィンドウハンドル
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

// 初期化処理
bool App::InitApp()
{
    // COMの初期化。WICで画像を読み込むのに必要
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
    {
        MessageBoxW(nullptr, L"CoInitializeEx で失敗", L"エラー", MB_OK);
        return false;
    }
    // ウィンドウの初期化
    if (!InitWnd())
    {
        MessageBoxW(nullptr, L"InitWnd で失敗", L"エラー", MB_OK);
        return false;
    }
    // Direct3D12の初期化
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

bool App::InitWnd() // ウィンドウの初期化処理
{
    auto hInst = GetModuleHandle(nullptr);
    if (hInst == nullptr)
    {
        return false;
    }

    // ウィンドウの設定
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW; // ウィンドウのスタイル：サイズが変わったら再描画する
    wc.lpfnWndProc = WndProc;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // アイコンの設定
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); // カーソルの設定
    wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND); // 背景色の設定
    wc.lpszMenuName = nullptr; // メニューの設定
    wc.lpszClassName = ClassName; // クラス名の設定
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION); // 小さいアイコンの設定

    // ウィンドウの登録
    if (!RegisterClassEx(&wc))
    {
        return false;
    }

    // インスタンスハンドル設定
    m_hInst = hInst;

    // ウィンドウのサイズの調整
    auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;

    RECT rc = {};
    rc.right = static_cast<LONG>(m_Width);
    rc.bottom = static_cast<LONG>(m_Height);
    AdjustWindowRect(&rc, style, FALSE);

    // ウィンドウの作成
    m_hWnd = CreateWindowEx(
        0, // 拡張スタイル
        ClassName, // クラス名
        WindowName, // ウィンドウ名
        style, // ウィンドウスタイル
        CW_USEDEFAULT, // X座標
        CW_USEDEFAULT, // Y座標
        rc.right - rc.left, // 幅
        rc.bottom - rc.top, // 高さ
        nullptr, // 親ウィンドウハンドル
        nullptr, // メニューハンドル
        m_hInst, // インスタンスハンドル
        nullptr // 追加パラメータ
    );
    if (m_hWnd == nullptr)
    {
        return false;
    }
    // ウィンドウの表示
    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);

    // ウィンドウにフォーカスを設定
    SetFocus(m_hWnd);

    return true;
}

bool App::InitD3D()
{
    // 0.デバッグのチェック
#if defined(DEBUG)||defined(_DEBUG)
    {
        ComPtr<ID3D12Debug>debug;
        auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf()));

        // デバッグレイヤーを有効化
        if (SUCCEEDED(hr))
        {
            debug->EnableDebugLayer();
        }
    }
#endif
    // 1.デバイスの生成
    auto hr = D3D12CreateDevice(
        nullptr, // デフォルトアダプタ
        D3D_FEATURE_LEVEL_11_0, // 機能レベル
        IID_PPV_ARGS(m_pDevice.GetAddressOf())
    );
    if (FAILED(hr))
    {
        return false;
    }
    // 2.コマンドキューの生成
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;

        hr = m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pQueue));
        if (FAILED(hr))
        {
            return false;
        }
    }
    // 3.Swap Chain
    // 3.1 DXGI Factoryの生成。スワップチェーンはDXGIの機能なので先に作る
    // ComPtr なので、関数を抜けるときに自動で解放される
    ComPtr<IDXGIFactory4> pFactory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(pFactory.GetAddressOf()));
    if (FAILED(hr))
    {
        return false;
    }

    // 3.2 Swap Chainの設定
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferDesc.Width = m_Width;
    swapChainDesc.BufferDesc.Height = m_Height;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.OutputWindow = m_hWnd;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // 生成
    ComPtr<IDXGISwapChain> pSwapChain;
    hr = pFactory->CreateSwapChain(
        m_pQueue.Get(), // コマンドキュー
        &swapChainDesc, // Swap Chainの設定
        pSwapChain.GetAddressOf() // Swap Chainの実体が入る
    );
    if (FAILED(hr))
    {
        return false;
    }

    // IDXGISwapChain3を取得
    hr = pSwapChain->QueryInterface(IID_PPV_ARGS(&m_pSwapChain));
    if (FAILED(hr))
    {
        return false;
    }
    m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

    // 4. コマンドアロケータとコマンドリストの生成
    {
        for (auto i = 0u; i < FrameCount; i++)
        {
            hr = m_pDevice->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(m_pCommandAllocator[i].GetAddressOf()));
            if (FAILED(hr))
            {
                return false;
            }
        }
    }
    // 生成
    {
        hr = m_pDevice->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_pCommandAllocator[m_FrameIndex].Get(),
            nullptr,
            IID_PPV_ARGS(m_pCommandList.GetAddressOf()));
        if (FAILED(hr))
        {
            return false;
        }
    }
    m_pCommandList->Close();

    // 5. RTV用のディスクリプタヒープ
    // 本来,ディスクリプタヒープは引数としてリソースとディスクリプタヒープが必要だが,スワップチェインで作成したバッファをリソースとするため、リソースを作成する必要はない
    // ディスクリプタヒープはRTV用の情報を入れる配列
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー用
    rtvHeapDesc.NumDescriptors = FrameCount; // リソース数と同じ
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    // デバイスの機能を使いディスクリプタヒープの生成
    hr = m_pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_pHeapRTV.GetAddressOf()));
    if (FAILED(hr))
    {
        return false;
    }
    // 生成
    // RTV用ディスクリプタヒープの先頭のCPUディスクリプタハンドルを取得する
    auto rtvHandle = m_pHeapRTV->GetCPUDescriptorHandleForHeapStart();
    // RTVディスクリプタ1個分のバイトサイズを取得,アドレス間の間隔サイズ
    auto rtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (auto i = 0u; i < FrameCount; ++i)
    {
        hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(m_pColorBuffer[i].GetAddressOf())); // バックバッファのポインタの取得
        if (FAILED(hr))
        {
            return false;
        }
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // sRGB
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2次元テクスチャとしてアクセス
        rtvDesc.Texture2D.MipSlice = 0;
        rtvDesc.Texture2D.PlaneSlice = 0;
        // 生成
        m_pDevice->CreateRenderTargetView(m_pColorBuffer[i].Get(), &rtvDesc, rtvHandle);
        m_HandleRTV[i] = rtvHandle;
        rtvHandle.ptr += rtvDescriptorSize;
    }

    // 6. フェンスの生成：CPUとGPUの同期に使う
    // 生成
    hr = m_pDevice->CreateFence(
        m_FenceCounter[m_FrameIndex],
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&m_pFence)
    );
    if (FAILED(hr))
    {
        return false;
    }

    m_FenceCounter[m_FrameIndex]++;
    // イベントの生成
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_FenceEvent == nullptr)
    {
        return false;
    }

    return true;
}

bool App::InitImGui()
{
    // 1. ImGui専用のSRVヒープ。シェーダーから見える
    D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
    imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    imguiHeapDesc.NumDescriptors = 64;   // 1.92以降はフォント以外にも使うので余裕を持たせる
    imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_pDevice->CreateDescriptorHeap(&imguiHeapDesc, IID_PPV_ARGS(m_pHeapImGui.GetAddressOf()))))
    {
        return false;
    }
    m_ImGuiDescSize = m_pDevice->GetDescriptorHandleIncrementSize(imguiHeapDesc.Type);
    m_ImGuiHeapNext = 0;

    // 2. コンテキスト作成とWin32側の初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!ImGui_ImplWin32_Init(m_hWnd))
    {
        return false;
    }

    // 3. DX12側の初期化
    ImGui_ImplDX12_InitInfo initInfo;
    initInfo.Device = m_pDevice.Get();
    initInfo.CommandQueue = m_pQueue.Get();
    initInfo.NumFramesInFlight = FrameCount;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;  // 267行目のRTVと同じ
    initInfo.DSVFormat = DXGI_FORMAT_D32_FLOAT;            // Render でDSVを設定しているため
    initInfo.UserData = this;
    initInfo.SrvDescriptorHeap = m_pHeapImGui.Get();
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* i,
        D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
        {
            auto* app = static_cast<App*>(i->UserData);
            const UINT idx = app->m_ImGuiHeapNext++;
            *outCpu = app->m_pHeapImGui->GetCPUDescriptorHandleForHeapStart();
            *outGpu = app->m_pHeapImGui->GetGPUDescriptorHandleForHeapStart();
            outCpu->ptr += SIZE_T(idx) * app->m_ImGuiDescSize;
            outGpu->ptr += UINT64(idx) * app->m_ImGuiDescSize;
        };
    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*,
        D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};  // 今回は解放しない
    return ImGui_ImplDX12_Init(&initInfo);
}