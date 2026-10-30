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
#include <Timer.h> // Timerクラスのヘッダをインクルード

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
// ConstantBufferView structure
///////////////////////////////////////////////////////////////////////////////
template<typename T>
struct ConstantBufferView
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC Desc;               // 定数バッファの構成設定
    D3D12_CPU_DESCRIPTOR_HANDLE     HandleCPU;          // CPUディスクリプタハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE     HandleGPU;          // GPUディスクリプタハンドル
    T* pBuffer;            // バッファ先頭へのポインタ
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
    ComPtr<ID3D12DescriptorHeap>           m_pHeapRTV;                     // ディスクリプタヒープ：レンダーターゲットビュー用
    ComPtr<ID3D12Fence>                    m_pFence;                       // フェンス
    ComPtr<ID3D12DescriptorHeap>           m_pHeapCBV;                     // ディスクリプタヒープ：CBV・SRV・UAV用
    ComPtr<ID3D12Resource>                 m_pVB;                          // 頂点バッファ
    ComPtr<ID3D12Resource>                 m_pCB[FrameCount];              // 定数バッファ
    ComPtr<ID3D12RootSignature>            m_pRootSignature;               // ルートシグニチャ
    ComPtr<ID3D12PipelineState>            m_pPSO;                         // パイプラインステート

    HANDLE                          m_FenceEvent;                   // フェンスイベント
    uint64_t                        m_FenceCounter[FrameCount];     // フェンスカウンター
    uint32_t                        m_FrameIndex;                   // フレーム番号
    D3D12_CPU_DESCRIPTOR_HANDLE     m_HandleRTV[FrameCount];        // レンダーターゲットビューのCPUディスクリプタ
    D3D12_VERTEX_BUFFER_VIEW        m_VBV;                          // 頂点バッファビュー
    D3D12_VIEWPORT                  m_Viewport;                     // ビューポート
    D3D12_RECT                      m_Scissor;                      // シザー矩形
    ConstantBufferView<Transform>   m_CBV[FrameCount];              // 定数バッファビュー
    float                           m_RotateAngle;

    Timer                           m_Timer; // Timerクラスのメンバ変数

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

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
};