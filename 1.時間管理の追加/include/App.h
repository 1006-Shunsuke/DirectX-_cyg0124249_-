//-----------------------------------------------------------------------------
// File : App.h
// Desc : Application Module.
// Copyright(c) Pocol. All right reserved.
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
#include <Timer.h> // 追加: Timerクラスのヘッダをインクルード

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
    DirectX::XMMATRIX   World;      // ワールド行列です.
    DirectX::XMMATRIX   View;       // ビュー行列です.
    DirectX::XMMATRIX   Proj;       // 射影行列です.
};

///////////////////////////////////////////////////////////////////////////////
// ConstantBufferView structure
///////////////////////////////////////////////////////////////////////////////
template<typename T>
struct ConstantBufferView
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC Desc;               // 定数バッファの構成設定です.
    D3D12_CPU_DESCRIPTOR_HANDLE     HandleCPU;          // CPUディスクリプタハンドルです.
    D3D12_GPU_DESCRIPTOR_HANDLE     HandleGPU;          // GPUディスクリプタハンドルです.
    T* pBuffer;            // バッファ先頭へのポインタです.
};


///////////////////////////////////////////////////////////////////////////////
// App class
///////////////////////////////////////////////////////////////////////////////
class App
{
    //=========================================================================
    // list of friend classes and methods.
    //=========================================================================
    /* NOTHING */

public:
    //=========================================================================
    // public variables.
    //=========================================================================
    /* NOTHING */

    //=========================================================================
    // public methods.
    //=========================================================================
    App(uint32_t width, uint32_t height);
    ~App();
    void Run();

private:
    //=========================================================================
    // private variables.
    //=========================================================================
    static const uint32_t FrameCount = 2;   // フレームバッファ数です.

    HINSTANCE   m_hInst;        // インスタンスハンドルです.
    HWND        m_hWnd;         // ウィンドウハンドルです.
    uint32_t    m_Width;        // ウィンドウの横幅です.
    uint32_t    m_Height;       // ウィンドウの縦幅です.

    ComPtr<ID3D12Device>                   m_pDevice;                      // デバイス
    ComPtr<ID3D12CommandQueue>             m_pQueue;                       // コマンドキュー
    ComPtr<IDXGISwapChain3>                m_pSwapChain;                   // スワップチェイン
    ComPtr<ID3D12Resource>                 m_pColorBuffer[FrameCount];     // カラーバッファ
    ComPtr<ID3D12CommandAllocator>         m_pCommandAllocator[FrameCount];    // コマンドアロケータ
    ComPtr<ID3D12GraphicsCommandList>      m_pCommandList;                     // コマンドリスト
    ComPtr<ID3D12DescriptorHeap>           m_pHeapRTV;                     // ディスクリプタヒープ(レンダーターゲットビュー).
    ComPtr<ID3D12Fence>                    m_pFence;                       // フェンス
    ComPtr<ID3D12DescriptorHeap>           m_pHeapCBV;                     // ディスクリプタヒープ(定数バッファビュー・シェーダリソースビュー・アンオーダードアクセスビュー)
    ComPtr<ID3D12Resource>                 m_pVB;                          // 頂点バッファ
    ComPtr<ID3D12Resource>                 m_pCB[FrameCount];              // 定数バッファ
    ComPtr<ID3D12RootSignature>            m_pRootSignature;               // ルートシグニチャ
    ComPtr<ID3D12PipelineState>            m_pPSO;                         // パイプラインステート

    HANDLE                          m_FenceEvent;                   // フェンスイベント
    uint64_t                        m_FenceCounter[FrameCount];     // フェンスカウンター
    uint32_t                        m_FrameIndex;                   // フレーム番号
    D3D12_CPU_DESCRIPTOR_HANDLE     m_HandleRTV[FrameCount];        // CPUディスクリプタ(レンダーターゲットビュー)
    D3D12_VERTEX_BUFFER_VIEW        m_VBV;                          // 頂点バッファビュー
    D3D12_VIEWPORT                  m_Viewport;                     // ビューポート
    D3D12_RECT                      m_Scissor;                      // シザー矩形
    ConstantBufferView<Transform>   m_CBV[FrameCount];              // 定数バッファビュー
    float                           m_RotateAngle;

    Timer                           m_Timer; // 追加: Timerクラスのメンバ変数

    //=========================================================================
    // private methods.
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

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
};