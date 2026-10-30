// Include
#include "App.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <Timer.h>

using namespace DirectX;


namespace {
    //Shaderパスの受け取り
    std::wstring GetExeDirectory()
    {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring s(path);
        return s.substr(0, s.find_last_of(L"\\/") + 1);
    }
}

namespace {
    // どこで失敗したかと HRESULT を表示する
    void ShowError(const wchar_t* where, HRESULT hr)
    {
        wchar_t buf[256];
        swprintf_s(buf, L"%s で失敗\nhr = 0x%08X", where, static_cast<unsigned>(hr));
        MessageBoxW(nullptr, buf, L"エラー", MB_OK);
    }
}

// ウィンドウクラス名とウィンドウ名の定義
namespace/*anonymous*/ {
    const auto ClassName = TEXT("SampleWindowClass");//ウィンドウクラス名
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
struct Vertex
{
    XMFLOAT3 Position;
    uint8_t Color[4];
};

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

void App::Run()
{
    if (InitApp())
    {
        MainLoop();
    }
    TermApp();
}

//初期化処理
bool App::InitApp()
{
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
    if (!OnInit())
    {
        MessageBoxW(nullptr, L"OnInit で失敗", L"エラー", MB_OK);
        return false;
    }
    m_Timer.Init();

    return true;
}

void App::TermApp()
{
    OnTerm();
    TermD3D();
    TermWnd();
}


bool App::InitWnd()//ウィンドウの初期化処理
{
    auto hInst = GetModuleHandle(nullptr);
    if (hInst == nullptr)return false;

    //ウィンドウの設定
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;// ウィンドウのスタイル：サイズが変わったら再描画する
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

void App::TermWnd()
{
    if (m_hInst != nullptr)
    {
        UnregisterClass(ClassName, m_hInst);
    }
    m_hInst = nullptr;
    m_hWnd = nullptr;

}

void App::MainLoop()
{
    MSG msg = {};

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) == TRUE)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            m_Timer.Tick();
            Render();
        }
    }
}

// ウィンドウプロシージャ
LRESULT App::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_DESTROY:
    { PostQuitMessage(0); }
    break;
    default:
    {}
    break;
    }
    return DefWindowProc(hWnd, msg, wp, lp);
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
// 3.1 DXGI Factoryの生成。スワップチェーンはDXGIの機能なので先に作る
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

// 4. コマンドアロケータとコマンドリストの生成
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

// 5. RTV用のディスクリプタヒープ
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
// RTV用ディスクリプタヒープの先頭のCPUディスクリプタハンドルを取得する
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

// 6. フェンスの生成：CPUとGPUの同期に使う
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
//------------------------------------------------------------------
//              画面に表示し、次のフレームの準備
//------------------------------------------------------------------
void App::Present(uint32_t interval)
{
    // 画面に表示
    m_pSwapChain->Present(interval, 0);

    // シグナル処理
    const auto currentValue = m_FenceCounter[m_FrameIndex];
    m_pQueue->Signal(m_pFence.Get(), currentValue);

    // バックバッファ番号を更新
    m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

    // 次のフレームの描画準備がまだであれば待機する
    if (m_pFence->GetCompletedValue() < m_FenceCounter[m_FrameIndex])
    {
        m_pFence->SetEventOnCompletion(m_FenceCounter[m_FrameIndex], m_FenceEvent);
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    // 次のフレームのフェンスカウンターを増やす
    m_FenceCounter[m_FrameIndex] = currentValue + 1;
}
//-------------------------------------------------------------------
//                  GPUの処理完了を待つ
//-------------------------------------------------------------------
void App::WaitGpu()
{
    if (m_pQueue == nullptr || m_pFence == nullptr || m_FenceEvent == nullptr) return;

    //シグナル処理

    m_pQueue->Signal(m_pFence.Get(), m_FenceCounter[m_FrameIndex]);

    //完了時にイベント設定
    m_pFence->SetEventOnCompletion(m_FenceCounter[m_FrameIndex], m_FenceEvent);

    //待機処理
    WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);

    //カウンターを増やす
    m_FenceCounter[m_FrameIndex]++;
}
//-------------------------------------------------------------------
//                  Direct3Dの終了処理
//-------------------------------------------------------------------
//作成したオブジェクトを一つずつ開放
void App::TermD3D()
{
    //GPU処理の完了を待機
    WaitGpu();


    //イベント破棄
    if (m_FenceEvent != nullptr) {
        CloseHandle(m_FenceEvent);
        m_FenceEvent = nullptr;
    }

    //6.フェンス破棄
    m_pFence.Reset();

    //5.RTV破棄
    m_pHeapRTV.Reset();
    for (auto i = 0u; i < FrameCount; ++i)
    {
        m_pColorBuffer[i].Reset();
    }

    //4.2.コマンドリスト破棄
    m_pCommandList.Reset();


    //4.1.コマンドアロケータ破棄
    for (auto i = 0u; i < FrameCount; ++i)
    {
        m_pCommandAllocator[i].Reset();
    }

    //3.スワップチェイン破棄
    m_pSwapChain.Reset();

    //2.コマンドキュー破棄
    m_pQueue.Reset();

    //1.デバイス破棄
    m_pDevice.Reset();

}
//--------------------------------------------------------
//   「描画に必要なGPUリソースを一通り作る」初期化関数
//--------------------------------------------------------
// 
// 描画に必要なデータは「頂点情報」→「インデックス情報」→「定数バッファ情報」→「ルートシグニチャ情報」→「パイプラインステート情報」の順に作る必要がある
// データ自体は最初頂点データがCPUにあり、それをシェーダーが扱えるようにするために、頂点データ本体はGPU側が読み取れるような「リソース」,そのリソースの使い方として「ディスクリプタ」を作る必要がある
// そのリソース自体は「ヒープ」というGPUがアクセスできるメモリ上の「空の入れ物」に入れる必要がある.「ディスクリプタ」もディスクリプタヒープという空の配列に入れる必要がある
// 
// 
//-----------------------------------------------------------------------------
//      初期化時の処理
//-----------------------------------------------------------------------------
bool App::OnInit()
{
    // 0. 頂点バッファの生成：頂点情報を入れるリソース
	// 本来頂点情報はCPUにあるが、GPUが扱えるようにするために、GPU側が読み取れるような「リソース」を作る必要がある
    // 頂点シェーダの入力データに合うように頂点バッファを作成する。
	// GPUが読むとき、必要な情報は「アドレス・サイズ・ストライド」のみであるため、ディスクリプタは不要で「ビュー」と呼ばれる構造体を作るだけでよい
    // 頂点データをGPU側が扱える頂点バッファの中に入れてGPUへの渡し方まで用意
    //ここではGPU側では何の処理も行わない
    {
        // 頂点データの中身
        Vertex vertices[] = {
            // +Z 前面：赤
            { XMFLOAT3(-0.5f,  0.5f,  0.5f), {255,   0,   0, 255} },
            { XMFLOAT3(0.5f,  0.5f,  0.5f), {255,   0,   0, 255} },
            { XMFLOAT3(0.5f, -0.5f,  0.5f), {255,   0,   0, 255} },
            { XMFLOAT3(-0.5f, -0.5f,  0.5f), {255,   0,   0, 255} },
            // -Z 背面：緑
            { XMFLOAT3(0.5f,  0.5f, -0.5f), {  0, 255,   0, 255} },
            { XMFLOAT3(-0.5f,  0.5f, -0.5f), {  0, 255,   0, 255} },
            { XMFLOAT3(-0.5f, -0.5f, -0.5f), {  0, 255,   0, 255} },
            { XMFLOAT3(0.5f, -0.5f, -0.5f), {  0, 255,   0, 255} },
            // +X 右面：青
            { XMFLOAT3(0.5f,  0.5f,  0.5f), {  0,   0, 255, 255} },
            { XMFLOAT3(0.5f,  0.5f, -0.5f), {  0,   0, 255, 255} },
            { XMFLOAT3(0.5f, -0.5f, -0.5f), {  0,   0, 255, 255} },
            { XMFLOAT3(0.5f, -0.5f,  0.5f), {  0,   0, 255, 255} },
            // -X 左面：黄
            { XMFLOAT3(-0.5f,  0.5f, -0.5f), {255, 255,   0, 255} },
            { XMFLOAT3(-0.5f,  0.5f,  0.5f), {255, 255,   0, 255} },
            { XMFLOAT3(-0.5f, -0.5f,  0.5f), {255, 255,   0, 255} },
            { XMFLOAT3(-0.5f, -0.5f, -0.5f), {255, 255,   0, 255} },
            // +Y 上面：シアン
            { XMFLOAT3(-0.5f,  0.5f, -0.5f), {  0, 255, 255, 255} },
            { XMFLOAT3(0.5f,  0.5f, -0.5f), {  0, 255, 255, 255} },
            { XMFLOAT3(0.5f,  0.5f,  0.5f), {  0, 255, 255, 255} },
            { XMFLOAT3(-0.5f,  0.5f,  0.5f), {  0, 255, 255, 255} },
            // -Y 下面：マゼンタ
            { XMFLOAT3(-0.5f, -0.5f,  0.5f), {255,   0, 255, 255} },
            { XMFLOAT3(0.5f, -0.5f,  0.5f), {255,   0, 255, 255} },
            { XMFLOAT3(0.5f, -0.5f, -0.5f), {255,   0, 255, 255} },
            { XMFLOAT3(-0.5f, -0.5f, -0.5f), {255,   0, 255, 255} },
        };

        //0-1,リソースの生成

        // ヒーププロパティ
        //どの種類のメモリに置くか,本件だとCPUから書ける実メモリに置く
        D3D12_HEAP_PROPERTIES prop = {};
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;//GPUへ
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;//お任せ
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//お任せ
        prop.CreationNodeMask = 1;//単一GPU
        prop.VisibleNodeMask = 1;//単一GPU

        // リソースの設定：中身はまだ空の入れ物
        // この場合のメモリはGPUがアクセスできるメモリ上の「空の入れ物」
        //確保するメモリの「形・サイズ・種類」をドライバに伝える
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;//バッファ
        desc.Alignment = 0;
        desc.Width = sizeof(vertices);//Bufferの場合はバッファサイズ,Textureの場合は横幅
        desc.Height = 1;//Bufferの場合は1,Textureの場合は縦幅
        desc.DepthOrArraySize = 1;//Bufferの場合は1,Textureの場合は配列数,3次元テクスチャの場合は奥行き
        desc.MipLevels = 1;//Textureの場合はミニマップレベル数
        desc.Format = DXGI_FORMAT_UNKNOWN;//Textureの場合はピクセルフォーマット
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;//Textureの場合はマルチサンプルを設定
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;//レイアウト
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        // 生成
        // 設定をもとに実際にGPUメモリを確保し、リソースを作る
        // このCreateCommittedResource関数は、ドライバを介してメモリを確保しているため、GPUからもアクセスできるようになっている
        auto hr = m_pDevice->CreateCommittedResource(
            &prop,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,//リソースの初期状態
            nullptr,
			IID_PPV_ARGS(m_pVB.GetAddressOf()));//メモリの確保とリソースの生成
        if (FAILED(hr))
        {
            ShowError(L"0-1 頂点バッファ生成", hr);
            return false;
        }


        //0-2頂点データを書き込む
        //マッピング
        //m_pVBはリソースを操作するためのオブジェクトを指すポインタ
        //確保した空メモリにCPU側の頂点データを実際に書き込む

        void* ptr = nullptr;
        hr = m_pVB->Map(0, nullptr, &ptr);
        if (FAILED(hr))
        {
            ShowError(L"0-2 頂点バッファMap", hr);
            return false;
        }

        // 頂点データをマッピング先に設定
        memcpy(ptr, vertices, sizeof(vertices));

        // マッピング解除
        m_pVB->Unmap(0, nullptr);

        //0-3View構造体を作成
        // 頂点バッファビューの設定。ただし、ビュー自体はGPU側から読めない
        m_VBV.BufferLocation = m_pVB->GetGPUVirtualAddress();//GPU仮想アドレス
        m_VBV.SizeInBytes = static_cast<UINT>(sizeof(vertices));//頂点バッファ全体のサイズ
        m_VBV.StrideInBytes = static_cast<UINT>(sizeof(Vertex));//1頂点あたりのサイズ
    }
    //1.インデックスバッファの生成
    // 頂点シェーダの入力データに合うように頂点バッファを作成する。
    // 頂点データをGPU側が扱える頂点バッファの中に入れてGPUへの渡し方まで用意
    {

        uint32_t indicies[] = {
        0, 1, 2,   0, 2, 3,   // +Z
        4, 5, 6,   4, 6, 7,   // -Z
        8, 9,10,   8,10,11,   // +X
        12,13,14,  12,14,15,   // -X
        16,17,18,  16,18,19,   // +Y
        20,21,22,  20,22,23,   // -Y
        };
        m_IndexCount = _countof(indicies);

        //1-1,リソースの生成

        // ヒーププロパティ
        D3D12_HEAP_PROPERTIES prop = {};
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;//GPUへ
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;//お任せ
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//お任せ
        prop.CreationNodeMask = 1;//単一GPU
        prop.VisibleNodeMask = 1;//単一GPU

        // リソースの設定
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;//バッファ
        desc.Alignment = 0;
        desc.Width = sizeof(indicies);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        // 生成
        auto hr = m_pDevice->CreateCommittedResource(
            &prop,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,//リソースの初期状態
            nullptr,
            IID_PPV_ARGS(m_pIB.GetAddressOf()));
        if (FAILED(hr))
        {
            ShowError(L"1-1 インデックスバッファ生成", hr);
            return false;
        }


        //1-2頂点データを書き込む
        //マッピング

        void* ptr = nullptr;
        hr = m_pIB->Map(0, nullptr, &ptr);
        if (FAILED(hr))
        {
            ShowError(L"1-2 インデックスバッファMap", hr);
            return false;
        }

        // 頂点データをマッピング先に設定
        memcpy(ptr, indicies, sizeof(indicies));

        // マッピング解除
        m_pIB->Unmap(0, nullptr);

        //1-3View構造体を作成
        // インデックスバッファビューの設定
        m_IBV.BufferLocation = m_pIB->GetGPUVirtualAddress();//GPU仮想アドレス
        m_IBV.Format = DXGI_FORMAT_R32_UINT;//頂点バッファ全体のサイズ
        m_IBV.SizeInBytes = static_cast<UINT>(sizeof(indicies));//1頂点あたりのサイズ
    }

    //2.定数バッファの生成
    //定数バッファはシェーダ内で計算に使う定数値のこと
    // b0: Transform → ルートCBV。GPU仮想アドレスをルートに直接渡すので、ディスクリプタヒープは不要
    // b1: MoveParam → ルート定数。値そのものを描画時にルートへ書き込むので、リソースは不要
    {

        // ヒープの設定
        D3D12_HEAP_PROPERTIES prop = {};
		prop.Type = D3D12_HEAP_TYPE_UPLOAD;//CPUから書けてGPUが読み込めるメモリ
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;//Typeに応じて自動で決定
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//Typeに応じて自動で決定
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;

        // リソースの設定
		// リソース・・・GPUから見えるメモリの塊,中身はただのバイト列
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = sizeof(Transform);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        //カメラ設定
        auto eyePos = XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f);
        auto targetPos = XMVectorZero();
        auto upward = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        constexpr auto fovY = XMConvertToRadians(37.5f);
        auto aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);

        for (auto i = 0u; i < FrameCount; ++i)
        {
            // 2-1. リソースを作成して実メモリを確保する
			//CPU内にヒープ1個とその中にGPUから読み込めるリソース1個を作る
            auto hr = m_pDevice->CreateCommittedResource(
                &prop,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_pCB[i].GetAddressOf()));//
            if (FAILED(hr))
            {
                ShowError(L"2-1 定数バッファ生成", hr);
                return false;
            }

            //2-2.マッピング
            hr = m_pCB[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_pTransform[i]));
            if (FAILED(hr))
            {
                ShowError(L"2-2 定数バッファMap", hr);
                return false;
            }

            // 変換行列の設定
            m_pTransform[i]->World = XMMatrixIdentity();
            m_pTransform[i]->View = XMMatrixLookAtRH(eyePos, targetPos, upward);
            m_pTransform[i]->Proj = XMMatrixPerspectiveFovRH(fovY, aspect, 1.0f, 1000.0f);

        }
        m_MoveParam.OffsetX = 0.0f;
    }

    //3.ルートシグニチャの生成
    //ルートシグニチャは定数バッファやテクスチャ,サンプラーといったシェーダ内で使用するリソースのレイアウトを決めるオブジェクト
    {

        auto flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        // ルートパラメータの設定
        D3D12_ROOT_PARAMETER params[2] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor.ShaderRegister = 0;
		params[0].Descriptor.RegisterSpace = 0;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;


        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[1].Constants.ShaderRegister = 1;
		params[1].Constants.RegisterSpace = 0;
        params[1].Constants.Num32BitValues = sizeof(MoveParam)/4;//
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // ルートシグニチャの設定
        D3D12_ROOT_SIGNATURE_DESC desc = {};
        desc.NumParameters = 2;
        desc.NumStaticSamplers = 0;
        desc.pParameters = params;
        desc.pStaticSamplers = nullptr;
        desc.Flags = flag;

        ComPtr<ID3DBlob> pBlob;
        ComPtr<ID3DBlob> pErrorBlob;

        // シリアライズ
        auto hr = D3D12SerializeRootSignature(
            &desc,
            D3D_ROOT_SIGNATURE_VERSION_1_0,
            pBlob.GetAddressOf(),
            pErrorBlob.GetAddressOf());
        if (FAILED(hr))
        {
            ShowError(L"3 ルートシグニチャ シリアライズ", hr);
            return false;
        }

        // ルートシグニチャを生成
        hr = m_pDevice->CreateRootSignature(
            0,
            pBlob->GetBufferPointer(),
            pBlob->GetBufferSize(),
            IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
        if (FAILED(hr))
        {
            ShowError(L"3 ルートシグニチャ生成", hr);
            return false;
        }
    }

    // 深度バッファの準備
    {
        constexpr DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;

        // 1. DSV用ディスクリプタヒープ
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = 1;//
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//

        auto hr = m_pDevice->CreateDescriptorHeap(
            &desc, IID_PPV_ARGS(m_pHeapDSV.GetAddressOf()));
        if (FAILED(hr))
        {
            ShowError(L"DSVヒープ生成", hr);
            return false;
        }

        m_HandleDSV = m_pHeapDSV->GetCPUDescriptorHandleForHeapStart();//ハンドルはアドレスの定数値

        // 2. GPUが深度値を保存するテクスチャ
        D3D12_HEAP_PROPERTIES tex = {};
        tex.Type = D3D12_HEAP_TYPE_DEFAULT;
        tex.CreationNodeMask = 1;
        tex.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC depth = {};//デプス画像のサイズ,形式,サンプル数
        depth.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depth.Width = m_Width;
        depth.Height = m_Height;
        depth.DepthOrArraySize = 1;
        depth.MipLevels = 1;
        depth.Format = depthFormat;
        depth.SampleDesc.Count = 1;
        depth.SampleDesc.Quality = 0;
        depth.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depth.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear = {};//クリア値によく使う値
        clear.Format = depthFormat;
        clear.DepthStencil.Depth = 1.0f;
        clear.DepthStencil.Stencil = 0;

        //リソース生成
        hr = m_pDevice->CreateCommittedResource(
            &tex,
            D3D12_HEAP_FLAG_NONE,
            &depth,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clear,
            IID_PPV_ARGS(m_pDepthBuffer.GetAddressOf()));
        if (FAILED(hr))
        {
            ShowError(L"深度バッファ生成", hr);
            return false;
        }

        // 3. ヒープ内の1スロットにDSVを書き込む
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
        dsv.Format = depthFormat;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv.Flags = D3D12_DSV_FLAG_NONE;
        dsv.Texture2D.MipSlice = 0;

        m_pDevice->CreateDepthStencilView(
            m_pDepthBuffer.Get(), &dsv, m_HandleDSV);
    }



    //4.パイプラインステートの生成
    {
        // 入力レイアウトの設定
		// 頂点シェーダの入力に合わせて設定する必要がある
        D3D12_INPUT_ELEMENT_DESC elements[2];
        // 頂点座標の設定
        elements[0].SemanticName = "POSITION";
		elements[0].SemanticIndex = 0;//同じSemantic名のものがない限り0
		elements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;//頂点シェーダの入力に合わせる
		elements[0].InputSlot = 0;//IASetVertexBuffers()で何番スロットにどのバッファを置くかを決める.→ここでその頂点バッファのスロット番号を決定する
        elements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;//前の要素の直後に自動配置
        elements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elements[0].InstanceDataStepRate = 0;//入力要素が頂点ごとなら0,インスタンスごとなら1以上の値を設定する

		// 頂点カラーの設定
        elements[1].SemanticName = "COLOR";
        elements[1].SemanticIndex = 0;
		elements[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        elements[1].InputSlot = 0;
        elements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        elements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        elements[1].InstanceDataStepRate = 0;

        // ラスタライザーステートの設定：ラスタライズを行うときの状態
        D3D12_RASTERIZER_DESC descRS;
		descRS.FillMode = D3D12_FILL_MODE_SOLID;//三角形を塗りつぶし
		descRS.CullMode = D3D12_CULL_MODE_NONE;//前面も背面も描画する
		descRS.FrontCounterClockwise = FALSE;//頂点の並び順が反時計回りなら前面とする
		descRS.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;// 深度値に加算する値。0でよい
		descRS.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;// DepthBias の最大値。0でよい
		descRS.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;// 深度値に加算する値の、勾配に応じたスケーリング値。0でよい
		descRS.DepthClipEnable = FALSE;// 距離に基づくクリッピングを有効にするか。無効でよい
        descRS.MultisampleEnable = FALSE;//マルチサンプリング
        descRS.AntialiasedLineEnable = TRUE;//アンチエイリアス
        descRS.ForcedSampleCount = 0;
        descRS.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;//厳密判定OFF

        // レンダーターゲットのブレンド設定
        D3D12_RENDER_TARGET_BLEND_DESC descRTBS = {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL
        };

        // ブレンドステートの設定
        D3D12_BLEND_DESC descBS;
        descBS.AlphaToCoverageEnable = FALSE;
        descBS.IndependentBlendEnable = FALSE;
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            descBS.RenderTarget[i] = descRTBS;
        }

        ComPtr<ID3DBlob> pVSBlob;
        ComPtr<ID3DBlob> pPSBlob;

        // 頂点シェーダ読み込み
        auto vsPath = GetExeDirectory() + L"SimpleVS.cso";
        auto hr = D3DReadFileToBlob(vsPath.c_str(), pVSBlob.GetAddressOf());
        if (FAILED(hr))
        {
            MessageBoxW(nullptr, vsPath.c_str(), L"VS読み込み失敗", MB_OK);
            return false;
        }

        // ピクセルシェーダ読み込み
        auto psPath = GetExeDirectory() + L"SimplePS.cso";
        hr = D3DReadFileToBlob(psPath.c_str(), pPSBlob.GetAddressOf());
        if (FAILED(hr))
        {
            MessageBoxW(nullptr, psPath.c_str(), L"PS読み込み失敗", MB_OK);
            return false;
        }
        //深度ステンシルステートの設定
        D3D12_DEPTH_STENCIL_DESC descDSS = {};
        descDSS.DepthEnable = TRUE;// 深度テストを有効にする
        descDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;// テストを通過した部分の深度を書き込む
        descDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;// 保存済みの深度より小さい＝手前なら通過
        descDSS.StencilEnable = FALSE;// 今回はステンシルを使わない

        // パイプラインステートの設定
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.InputLayout = { elements, _countof(elements) };
        desc.pRootSignature = m_pRootSignature.Get();
        desc.VS = { pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize() };
        desc.PS = { pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize() };
        desc.RasterizerState = descRS;
        desc.BlendState = descBS;
        desc.DepthStencilState = descDSS;
        desc.SampleMask = UINT_MAX;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        // パイプラインステートを生成
        hr = m_pDevice->CreateGraphicsPipelineState(
            &desc,
            IID_PPV_ARGS(m_pPSO.GetAddressOf()));
        if (FAILED(hr))
        {
            ShowError(L"4 PSO生成", hr);
            return false;
        }
    }

    //5.ビューポートとシザー矩形の設定
    {
        m_Viewport.TopLeftX = 0;
        m_Viewport.TopLeftY = 0;
        m_Viewport.Width = static_cast<float>(m_Width);
        m_Viewport.Height = static_cast<float>(m_Height);
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;

        m_Scissor.left = 0;
        m_Scissor.right = m_Width;
        m_Scissor.top = 0;
        m_Scissor.bottom = m_Height;
    }

    return true;
}
//------------------------------------------------------------------------------------
//                            描画　処理
//------------------------------------------------------------------------------------
void App::Render()
{
    //コマンドの記録の開始
    //使い終わったメモリの再利用準備
    m_pCommandAllocator[m_FrameIndex]->Reset();
    m_pCommandList->Reset(m_pCommandAllocator[m_FrameIndex].Get(), nullptr);
    // リソースバリアの設定：表示用から描画用に切り替える
    // 例
    //バックバッファに三角形を描き込む処理A
    //Present()で表示エンジンがそのバックバッファを画面に表示する処理B
    //その処理Aと処理Bが同時に行われないためにリソースバリアは必要
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;//使用用途の遷移
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_pColorBuffer[m_FrameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    //リソースバリアの生成
    m_pCommandList->ResourceBarrier(1, &barrier);

    //レンダーターゲットの設定
    m_pCommandList->OMSetRenderTargets(1, &m_HandleRTV[m_FrameIndex], FALSE, &m_HandleDSV);
    m_pCommandList->ClearDepthStencilView(m_HandleDSV,D3D12_CLEAR_FLAG_DEPTH,1.0f,0,0,nullptr);

    //クリアカラーの設定
    const float value = 0.5f + 0.5f * sinf(m_Timer.GetTotalTime());
    float clearColor[] = { value,value,value,value };

    //レンダーターゲットビューをクリア
    m_pCommandList->ClearRenderTargetView(m_HandleRTV[m_FrameIndex], clearColor, 0, nullptr);


    //パラメータの更新
    //m_MoveParam.OffsetX = sinf(m_Timer.GetTotalTime()) * 1.0f;

    //ワールド行列を毎フレーム書き換える処理
    const float t = m_Timer.GetTotalTime();
    XMMATRIX rot1 = XMMatrixRotationY(t);//回転処理
    XMMATRIX rot2 = XMMatrixRotationZ(t);
    XMMATRIX rot3 = XMMatrixRotationX(t);
    m_pTransform[m_FrameIndex]->World = rot1 * rot2 * rot3;

    //描画処理
    {
        //ポリゴン描画用の処理
        m_pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
        m_pCommandList->SetGraphicsRootConstantBufferView(0, m_pCB[m_FrameIndex]->GetGPUVirtualAddress());
        m_pCommandList->SetGraphicsRoot32BitConstants(1, sizeof(MoveParam) / 4, &m_MoveParam, 0);
        m_pCommandList->SetPipelineState(m_pPSO.Get());

        m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pCommandList->IASetVertexBuffers(0, 1, &m_VBV);
        m_pCommandList->IASetIndexBuffer(&m_IBV);
        m_pCommandList->RSSetViewports(1, &m_Viewport);
        m_pCommandList->RSSetScissorRects(1, &m_Scissor);

        m_pCommandList->DrawIndexedInstanced(m_IndexCount, 1, 0,0,0);
    }
    // リソースバリアの設定：描画用から表示用に戻す
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_pColorBuffer[m_FrameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    //リソースバリア
    m_pCommandList->ResourceBarrier(1, &barrier);

    //コマンドの記録を終了
    m_pCommandList->Close();

    //コマンドを実行
    ID3D12CommandList* ppCommandLists[] = { m_pCommandList.Get() };
    m_pQueue->ExecuteCommandLists(1, ppCommandLists);

    //画面に表示
    Present(1);
}
//-----------------------------------------------------------------------------
//      終了時の処理
//-----------------------------------------------------------------------------
void App::OnTerm()
{
    WaitGpu();

    //Depthの初期化
    m_pDepthBuffer.Reset();
    m_pHeapDSV.Reset();
    m_HandleDSV = {};

    for (auto i = 0u; i < FrameCount; ++i)
    {
        if (m_pCB[i].Get() != nullptr)
        {
            m_pCB[i]->Unmap(0, nullptr);
            m_pTransform[i] = nullptr;
        }
        m_pCB[i].Reset();
    }
    m_pVB.Reset();
    m_pIB.Reset();
    m_pPSO.Reset();
    m_pRootSignature.Reset();
}