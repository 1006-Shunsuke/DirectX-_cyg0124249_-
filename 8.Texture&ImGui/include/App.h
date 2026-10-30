//-----------------------------------------------------------------------------
// File : App.h
// Desc : アプリケーション本体
//-----------------------------------------------------------------------------
#pragma once

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "Timer.h" // Timerクラスのヘッダをインクルード

//-----------------------------------------------------------------------------
// Linker
//-----------------------------------------------------------------------------
#pragma comment( lib, "d3d12.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )

//-----------------------------------------------------------------------------
// Type Alias
//-----------------------------------------------------------------------------
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;


///////////////////////////////////////////////////////////////////////////////
// Transform structure
///////////////////////////////////////////////////////////////////////////////
struct alignas(256) Transform
{
    DirectX::XMMATRIX   World;      // ワールド行列
    DirectX::XMMATRIX   View;       // ビュー行列
    DirectX::XMMATRIX   Proj;       // 射影行列
};

///////////////////////////////////////////////////////////////////////////////
// App class
///////////////////////////////////////////////////////////////////////////////
class App
{
    //=========================================================================
    // フレンドクラス・関数
    //=========================================================================
    /* NOTHING */

public:
    //=========================================================================
    // public 変数
    //=========================================================================
    /* NOTHING */

    //=========================================================================
    // public 関数
    //=========================================================================
    App(uint32_t width, uint32_t height);
    ~App();
    void Run();

private:
    //=========================================================================
    // private 変数
    //=========================================================================
    static const uint32_t FrameCount = 2;   // フレームバッファ数
    static constexpr const TCHAR* ClassName = TEXT("SampleWindowClass");   // ウィンドウクラス名。InitWnd と TermWnd で使う

    HINSTANCE   m_hInst;        // インスタンスハンドル
    HWND        m_hWnd;         // ウィンドウハンドル
    uint32_t    m_Width;        // ウィンドウの横幅
    uint32_t    m_Height;       // ウィンドウの縦幅

    ComPtr<ID3D12Device>                   m_pDevice;                      // デバイス
    ComPtr<ID3D12CommandQueue>             m_pQueue;                       // コマンドキュー
    ComPtr<IDXGISwapChain3>                m_pSwapChain;                   // スワップチェイン
    ComPtr<ID3D12Resource>                 m_pColorBuffer[FrameCount];     // カラーバッファ
    ComPtr<ID3D12CommandAllocator>         m_pCommandAllocator[FrameCount];    // コマンドアロケータ
    ComPtr<ID3D12GraphicsCommandList>      m_pCommandList;                     // コマンドリスト
    ComPtr<ID3D12Fence>                    m_pFence;                       // フェンス
    ComPtr<ID3D12DescriptorHeap>           m_pHeapRTV;                     // ディスクリプタヒープ：CBV・SRV・UAV用
    ComPtr<ID3D12Resource>                 m_pVB;                          // 頂点バッファ
    ComPtr<ID3D12Resource>                 m_pIB;                          // インデックスバッファ
    ComPtr<ID3D12Resource>                 m_pCB[FrameCount];              // 定数バッファ
    ComPtr<ID3D12DescriptorHeap>           m_pHeapDSV;                     // DSV用ディスクリプタヒープ
    ComPtr<ID3D12Resource>                 m_pDepthBuffer;                 // 深度バッファ
    ComPtr<ID3D12RootSignature>            m_pRootSignature;               // ルートシグニチャ
    ComPtr<ID3D12PipelineState>            m_pPSO;                         // パイプラインステート
    ComPtr<ID3D12DescriptorHeap>           m_pHeapCBV_SRV_UAV;             // CBV・SRV・UAV用ディスクリプタヒープ。シェーダから見える
    ComPtr<ID3D12Resource>                 m_pTexture;                     // テクスチャ
    D3D12_GPU_DESCRIPTOR_HANDLE            m_HandleGPU_SRV{};              // テクスチャのSRVのGPUハンドル。描画時に使う
    


    HANDLE                          m_FenceEvent;                   // フェンスイベント
    uint64_t                        m_FenceCounter[FrameCount];     // フェンスカウンター
    uint32_t                        m_FrameIndex;                   // フレーム番号
    D3D12_CPU_DESCRIPTOR_HANDLE     m_HandleRTV[FrameCount];        // レンダーターゲットビューのCPUディスクリプタ
    D3D12_VERTEX_BUFFER_VIEW        m_VBV;                          // 頂点バッファビュー
    D3D12_CPU_DESCRIPTOR_HANDLE     m_HandleDSV{};                  // 深度ハンドル。図形ごとにIDが必要
    D3D12_VIEWPORT                  m_Viewport;                     // ビューポート
    D3D12_RECT                      m_Scissor;                      // シザー矩形
    D3D12_INDEX_BUFFER_VIEW         m_IBV;                          // インデックスバッファビュー
    UINT                            m_IndexCount = 0;               // 描くインデックスの数(DrawIndexedInstanced で使う)
    Transform*                      m_pTransform[FrameCount] = {};

    Timer                           m_Timer; // Timerクラスのメンバ変数

    // ImGui
    ComPtr<ID3D12DescriptorHeap>    m_pHeapImGui;          // ImGui専用のSRVヒープ。シェーダーから見える
    UINT                            m_ImGuiHeapNext = 0;   // 次に割り当てる位置
    UINT                            m_ImGuiDescSize = 0;   // ディスクリプタ1個のサイズ

    // エディタで編集する状態。Inspectorから書き換える
    DirectX::XMFLOAT3               m_Position = { 0, 0, 0 };
    DirectX::XMFLOAT3               m_RotationDeg = { 0, 0, 0 };
    DirectX::XMFLOAT3               m_Scale = { 1, 1, 1 };
    bool                            m_AutoRotate = true;
    float                           m_ClearColor[4] = { 0.2f, 0.2f, 0.25f, 1.0f };

    //=========================================================================
    // private 関数
    //=========================================================================
    bool InitApp();
    void TermApp();
    bool InitWnd();
    void TermWnd();
    void MainLoop();

    bool InitD3D();
    void TermD3D();
    void Render();
    void WaitGpu();
    void Present(uint32_t interval);

    bool OnInit();
    void OnTerm();

    // OnInit から順番に呼ぶ初期化処理(App_OnInit.cpp)
    bool CreateVertexBuffer();
    bool CreateIndexBuffer();
    bool CreateCbvSrvHeap();
    bool CreateConstantBuffers();
    bool LoadTexture();
    bool CreateRootSignature();
    bool CreateDepthBuffer();
    bool CreatePipelineState();
    void InitViewport();

    bool InitImGui();   // ImGuiの初期化。InitD3D のあとに行う
    void TermImGui();   // ImGuiの終了処理。TermD3D の前に行う
    void DrawImGui();   // ImGui per-frame UI and draw

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
};