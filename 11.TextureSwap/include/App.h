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
#include "Timer.h" // 経過時間を測る Timer クラス
#include <vector>
#include "Mesh.h"  // メッシュの読み込み：MeshVertex・LoadMesh

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
    DirectX::XMMATRIX   WorldInvTrans; // ワールド行列の逆行列の転置。法線の変換に使う。4つで 64×4 = 256バイト
};

///////////////////////////////////////////////////////////////////////////////
// LightParam structure : ライトの設定。PS の b1 に渡す
// HLSL の定数バッファは float4 の16バイト単位で並ぶので、float3 + float で1組にする
///////////////////////////////////////////////////////////////////////////////
struct alignas(256) LightParam
{
    DirectX::XMFLOAT3   LightDir;           // 光が進む向き：正規化済み          c0.xyz
    float               LightIntensity;     // 光の強さ                          c0.w
    DirectX::XMFLOAT3   LightColor;         // 光の色                            c1.xyz
    float               Shininess;          // 鏡面反射の鋭さ                    c1.w
    DirectX::XMFLOAT3   CameraPos;          // カメラの位置                      c2.xyz
    float               SpecularIntensity;  // 鏡面反射の強さ                    c2.w
    DirectX::XMFLOAT3   AmbientColor;       // 環境光の色                        c3.xyz
    float               Padding;            // 16バイトにそろえるための詰め物    c3.w
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

    // ルートパラメータの番号。ルートシグニチャの何番目の引数かを表す
    // 作るときの CreateRootSignature と使うときの Render で同じ番号を使うために名前を付ける
    // ※ HLSL の register 番号 b0・t0・b1 とは別物
    enum RootParam
    {
        RootParam_Transform = 0,    // b0 : 変換行列。ディスクリプタテーブルで渡す
        RootParam_Texture,          // t0 : テクスチャ。ディスクリプタテーブルで渡す
        RootParam_Light,            // b1 : ライト。ルートCBVで渡す
        RootParam_Count,            // ルートパラメータの数
    };

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
    ComPtr<ID3D12DescriptorHeap>           m_pHeapRTV;                     // RTV用のディスクリプタヒープ
    ComPtr<ID3D12Resource>                 m_pVB;                          // 頂点バッファ
    ComPtr<ID3D12Resource>                 m_pIB;                          // インデックスバッファ
    ComPtr<ID3D12Resource>                 m_pCB[FrameCount];              // 定数バッファ
    ComPtr<ID3D12Resource>                 m_pLightCB[FrameCount];         // ライト用の定数バッファ。フレーム数分
    ComPtr<ID3D12DescriptorHeap>           m_pHeapDSV;                     // DSV用のディスクリプタヒープ
    ComPtr<ID3D12Resource>                 m_pDepthBuffer;                 // 深度バッファ
    ComPtr<ID3D12RootSignature>            m_pRootSignature;               // ルートシグニチャ
    ComPtr<ID3D12PipelineState>            m_pPSO;                         // パイプラインステート
    ComPtr<ID3D12DescriptorHeap>           m_pHeapCBV_SRV_UAV;             // CBV・SRV・UAV用のディスクリプタヒープ。シェーダから見える
    ComPtr<ID3D12Resource>                 m_pTexture;                     // テクスチャ
    D3D12_GPU_DESCRIPTOR_HANDLE            m_HandleGPU_SRV{};              // テクスチャのSRVのGPUハンドル。描画時に使う
    


    HANDLE                          m_FenceEvent;                   // フェンスイベント
    uint64_t                        m_FenceCounter[FrameCount];     // フェンスカウンター
    uint32_t                        m_FrameIndex;                   // フレーム番号
    D3D12_CPU_DESCRIPTOR_HANDLE     m_HandleRTV[FrameCount];        // レンダーターゲットビューのCPUディスクリプタ
    D3D12_VERTEX_BUFFER_VIEW        m_VBV;                          // 頂点バッファビュー
    D3D12_CPU_DESCRIPTOR_HANDLE     m_HandleDSV{};                  // 深度バッファのDSVのCPUハンドル。描画時に OMSetRenderTargets と ClearDepthStencilView で使う
    D3D12_VIEWPORT                  m_Viewport;                     // ビューポート
    D3D12_RECT                      m_Scissor;                      // シザー矩形
    D3D12_INDEX_BUFFER_VIEW         m_IBV;                          // インデックスバッファビュー
    Transform*                      m_pTransform[FrameCount] = {};  // 変換行列用の定数バッファ b0 の書き込み先
    LightParam*                     m_pLight[FrameCount] = {};     // ライト用定数バッファの書き込み先

    // メッシュの描画範囲
    // 全メッシュの頂点・インデックスは1本ずつのバッファにまとめて入れ、
    // 「インデックスの何番目から何個を、頂点の何番目を0番として描くか」をメッシュごとに覚えておく
    struct DrawRange
    {
        UINT    IndexCount;     // 描くインデックスの数
        UINT    StartIndex;     // インデックスバッファの何番目から読むか
        INT     BaseVertex;     // 読んだインデックスに足す値。このメッシュの頂点の開始位置にあたる
    };
    std::vector<DrawRange>          m_DrawRanges;

    Timer                           m_Timer; // フレーム間の経過時間を測るタイマー

    // ImGui
    ComPtr<ID3D12DescriptorHeap>    m_pHeapImGui;          // ImGui専用のSRVヒープ。シェーダーから見える
    UINT                            m_ImGuiHeapNext = 0;   // 次に割り当てる位置
    UINT                            m_ImGuiDescSize = 0;   // ディスクリプタ1個のサイズ

    // エディタで編集する状態。Inspectorから書き換えられる
    DirectX::XMFLOAT3               m_Position = { 0, 0, 0 };
    DirectX::XMFLOAT3               m_RotationDeg = { 0, 0, 0 };
    DirectX::XMFLOAT3               m_Scale = { 1, 1, 1 };
    bool                            m_AutoRotate = true;
    float                           m_ClearColor[4] = { 0.2f, 0.2f, 0.25f, 1.0f };

    // カメラとライト。Inspector から書き換えられる
    DirectX::XMFLOAT3               m_CameraPos = { 0.0f, 0.0f, 5.0f };         // カメラの位置。ビュー行列と鏡面反射の両方で使う
    DirectX::XMFLOAT3               m_LightDir = { -1.0f, -1.0f, -1.0f };       // 光が進む向き。右上手前から奥へ
    float                           m_LightColor[3] = { 1.0f, 1.0f, 1.0f };     // 光の色
    float                           m_LightIntensity = 1.0f;                    // 光の強さ
    float                           m_AmbientColor[3] = { 0.1f, 0.1f, 0.1f };   // 環境光の色
    float                           m_SpecularIntensity = 0.5f;                 // 鏡面反射の強さ
    float                           m_Shininess = 32.0f;                        // 鏡面反射の鋭さ

    // テクスチャの貼り替え。Inspector で選んだら予約だけして、次のフレームの最初に反映する
    std::vector<std::wstring>       m_TextureFiles;             // 選べる画像ファイルのパス
    std::vector<std::string>        m_TextureNames;             // Inspector に表示する名前。UTF-8
    int                             m_CurrentTexture = -1;      // 今貼っているテクスチャの番号。-1 なら一覧にない
    int                             m_PendingTexture = -1;      // 貼り替えの予約。-1 なら予約なし

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

    // OnInit から順番に呼ぶ初期化処理。App_OnInit.cpp
    bool LoadMeshData(std::vector<MeshVertex>& vertices, std::vector<uint32_t>& indices, std::wstring& texturePath);  // 1. メッシュの読み込み
    bool CreateVertexBuffer(const std::vector<MeshVertex>& vertices);                      // 2. 頂点バッファ
    bool CreateIndexBuffer(const std::vector<uint32_t>& indices);                          // 3. インデックスバッファ
    bool CreateCbvSrvHeap();                                                               // 4. CBV/SRV 用のディスクリプタヒープ
    bool CreateConstantBuffers();                                                          // 5. 定数バッファ：変換行列・ライト
    bool LoadTexture(const std::wstring& modelTexturePath);                                // 6. テクスチャと SRV
    bool CreateRootSignature();                                                            // 7. ルートシグニチャ
    bool CreateDepthBuffer();                                                              // 8. 深度バッファ
    bool CreatePipelineState();                                                            // 9. パイプラインステート
    void InitViewport();                                                                   // 10. ビューポートとシザー矩形

    // テクスチャの読み込みと貼り替え。App_Texture.cpp
    HRESULT LoadTextureResource(const std::wstring& path);      // テクスチャを読み込み、SRV を作る
    void FindTextureFiles(const std::wstring& currentPath);     // 同じフォルダの画像を一覧にする
    void ApplyPendingTexture();                                 // 予約された貼り替えを実行する

    // GPU リソース作成の共通処理。App_Helper.cpp
    HRESULT CreateUploadBuffer(UINT64 size, const void* pInitData, ComPtr<ID3D12Resource>& pBuffer);
    HRESULT CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, D3D12_DESCRIPTOR_HEAP_FLAGS flags, ComPtr<ID3D12DescriptorHeap>& pHeap);

    bool InitImGui();   // ImGuiの初期化。InitD3D のあとに行う
    void TermImGui();   // ImGuiの終了処理。TermD3D の前に行う
    void DrawImGui();   // ImGui per-frame UI and draw

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
};