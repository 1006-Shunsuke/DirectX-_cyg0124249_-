//-----------------------------------------------------------------------------
// File : App_OnInit.cpp
// Desc : 描画に使うGPUリソースの初期化(OnInit と、そこから順番に呼ぶ関数)
//-----------------------------------------------------------------------------
#include "App.h"
#include <cstdio>
#include <string>

using namespace DirectX;

namespace
{
    std::wstring GetExeDirectory()
    {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring s(path);
        return s.substr(0, s.find_last_of(L"\\/") + 1);
    }
}

namespace
{
    // どこで失敗したかと HRESULT を表示する
    void ShowError(const wchar_t* where, HRESULT hr)
    {
        wchar_t buf[256];
        swprintf_s(buf, L"%s で失敗\nhr = 0x%08X", where, static_cast<unsigned>(hr));
        MessageBoxW(nullptr, buf, L"エラー", MB_OK);
    }
}

struct Vertex
{
    XMFLOAT3 Position;
    uint8_t Color[4];
};

//-----------------------------------------------------------------------------
// 初期化時の処理
// 描画に必要な GPU リソースを、下の 1〜6 の順番に作る
//-----------------------------------------------------------------------------
bool App::OnInit()
{
    // どれかが失敗したら、そこで中断する
    if (!CreateVertexBuffer())      { return false; }   // 1. 頂点バッファ
    if (!CreateIndexBuffer())       { return false; }   // 2. インデックスバッファ
    if (!CreateConstantBuffers())   { return false; }   // 3. 定数バッファ
    if (!CreateRootSignature())     { return false; }   // 4. ルートシグニチャ
    if (!CreatePipelineState())     { return false; }   // 5. パイプラインステート
    InitViewport();                                     // 6. ビューポートとシザー矩形

    return true;
}

//-----------------------------------------------------------------------------
// 1. 頂点バッファの生成
// 本来頂点情報はCPUにあるが、GPUが扱えるようにするために、GPU側が読み取れるような「リソース」を作る必要がある
// 頂点シェーダの入力データに合うように頂点バッファを作成する。
// GPUが読むとき、必要な情報は「アドレス・サイズ・ストライド」のみであるため、ディスクリプタは不要で「ビュー」と呼ばれる構造体を作るだけでよい
// 頂点データをGPU側が扱える頂点バッファの中に入れてGPUへの渡し方まで用意
// ここではGPU側では何の処理も行わない
//-----------------------------------------------------------------------------
bool App::CreateVertexBuffer()
{
    // 頂点データの中身
    Vertex vertices[] = {
        { XMFLOAT3(-1.0f, 1.0f, 0.0f), {0, 255, 255, 255} }, // floatとuint8_t
        { XMFLOAT3(-1.0f, -1.0f, 0.0f),{0, 0, 255, 255} },
        { XMFLOAT3(1.0f, -1.0f, 0.0f), {0, 0, 255, 255} },
        { XMFLOAT3(1.0f,  1.0f, 0.0f), {255, 0, 0, 255} },
    };

    // 1-1,リソースの生成

    // ヒーププロパティ
    // どの種類のメモリに置くか,本件だとCPUから書ける実メモリに置く
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // GPUへ
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // お任せ
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; // お任せ
    heapProps.CreationNodeMask = 1; // 単一GPU
    heapProps.VisibleNodeMask = 1; // 単一GPU

    // リソースの設定：中身はまだ空の入れ物
    // この場合のメモリはGPUがアクセスできるメモリ上の「空の入れ物」
    // 確保するメモリの「形・サイズ・種類」をドライバに伝える
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファ
    resourceDesc.Alignment = 0;
    resourceDesc.Width = sizeof(vertices); // Bufferの場合はバッファサイズ,Textureの場合は横幅
    resourceDesc.Height = 1; // Bufferの場合は1,Textureの場合は縦幅
    resourceDesc.DepthOrArraySize = 1; // Bufferの場合は1,Textureの場合は配列数,3次元テクスチャの場合は奥行き
    resourceDesc.MipLevels = 1; // Textureの場合はミップマップレベル数
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN; // Textureの場合はピクセルフォーマット
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0; // Textureの場合はマルチサンプルを設定
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // レイアウト
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // 生成
    // 設定をもとに実際にGPUメモリを確保し、リソースを作る
    // このCreateCommittedResource関数は、ドライバを介してメモリを確保しているため、GPUからもアクセスできるようになっている
    auto hr = m_pDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, // リソースの初期状態
        nullptr,
        IID_PPV_ARGS(m_pVB.GetAddressOf())); // メモリの確保とリソースの生成
    if (FAILED(hr))
    {
        ShowError(L"1-1 頂点バッファ生成", hr);
        return false;
    }


    // 1-2頂点データを書き込む
    // マッピング
    // m_pVBはリソースを操作するためのオブジェクトを指すポインタ
    // 確保した空メモリにCPU側の頂点データを実際に書き込む

    void* ptr = nullptr;
    hr = m_pVB->Map(0, nullptr, &ptr);
    if (FAILED(hr))
    {
        ShowError(L"1-2 頂点バッファMap", hr);
        return false;
    }

    // 頂点データをマッピング先に設定
    memcpy(ptr, vertices, sizeof(vertices));

    // マッピング解除
    m_pVB->Unmap(0, nullptr);

    // 1-3View構造体を作成
    // 頂点バッファビューの設定。ただし、ビュー自体はGPU側から読めない
    m_VBV.BufferLocation = m_pVB->GetGPUVirtualAddress(); // GPU仮想アドレス
    m_VBV.SizeInBytes = static_cast<UINT>(sizeof(vertices)); // 頂点バッファ全体のサイズ
    m_VBV.StrideInBytes = static_cast<UINT>(sizeof(Vertex)); // 1頂点あたりのサイズ

    return true;
}

//-----------------------------------------------------------------------------
// 2. インデックスバッファの生成
// 頂点を結ぶ順番(インデックス)をGPU側が読めるバッファに入れ、GPUへの渡し方まで用意する
// 同じ頂点を何度も書かずに済む
//-----------------------------------------------------------------------------
bool App::CreateIndexBuffer()
{
    uint32_t indices[] = { 0,1,2, 0,2,3 };
    m_IndexCount = _countof(indices);

    // 2-1,リソースの生成

    // ヒーププロパティ
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // GPUへ
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // お任せ
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; // お任せ
    heapProps.CreationNodeMask = 1; // 単一GPU
    heapProps.VisibleNodeMask = 1; // 単一GPU

    // リソースの設定
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファ
    resourceDesc.Alignment = 0;
    resourceDesc.Width = sizeof(indices);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // 生成
    auto hr = m_pDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, // リソースの初期状態
        nullptr,
        IID_PPV_ARGS(m_pIB.GetAddressOf()));
    if (FAILED(hr))
    {
        ShowError(L"2-1 インデックスバッファ生成", hr);
        return false;
    }


    // 2-2インデックスデータを書き込む
    // マッピング

    void* ptr = nullptr;
    hr = m_pIB->Map(0, nullptr, &ptr);
    if (FAILED(hr))
    {
        ShowError(L"2-2 インデックスバッファMap", hr);
        return false;
    }

    // インデックスデータをマッピング先に設定
    memcpy(ptr, indices, sizeof(indices));

    // マッピング解除
    m_pIB->Unmap(0, nullptr);

    // 2-3View構造体を作成
    // インデックスバッファビューの設定
    m_IBV.BufferLocation = m_pIB->GetGPUVirtualAddress(); // GPU仮想アドレス
    m_IBV.Format = DXGI_FORMAT_R32_UINT; // インデックス1個の型(uint32_t なので R32_UINT)
    m_IBV.SizeInBytes = static_cast<UINT>(sizeof(indices)); // インデックスバッファ全体のサイズ

    return true;
}

//-----------------------------------------------------------------------------
// 3. 定数バッファの生成
// b0: Transform → ルートCBV。GPU仮想アドレスをルートに直接渡すので、ディスクリプタヒープは不要
//-----------------------------------------------------------------------------
bool App::CreateConstantBuffers()
{
    // ヒープの設定
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // CPUから書けてGPUが読み込めるメモリ
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // Typeに応じて自動で決定
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; // Typeに応じて自動で決定
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // リソースの設定
    // リソース・・・GPUから見えるメモリの塊,中身はただのバイト列
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = sizeof(Transform);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // カメラ設定
    auto eyePos = XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f);
    auto targetPos = XMVectorZero();
    auto upward = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    constexpr auto fovY = XMConvertToRadians(37.5f);
    auto aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);

    for (auto i = 0u; i < FrameCount; ++i)
    {
        // 3-1. リソースを作成して実メモリを確保する
        // CPU内にヒープ1個とその中にGPUから読み込めるリソース1個を作る
        auto hr = m_pDevice->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_pCB[i].GetAddressOf())); //
        if (FAILED(hr))
        {
            ShowError(L"3-1 定数バッファ生成", hr);
            return false;
        }

        // 3-2.マッピング
        hr = m_pCB[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_pTransform[i]));
        if (FAILED(hr))
        {
            ShowError(L"3-2 定数バッファMap", hr);
            return false;
        }

        // 変換行列の設定
        m_pTransform[i]->World = XMMatrixIdentity();
        m_pTransform[i]->View = XMMatrixLookAtRH(eyePos, targetPos, upward);
        m_pTransform[i]->Proj = XMMatrixPerspectiveFovRH(fovY, aspect, 1.0f, 1000.0f);

    }

    return true;
}

//-----------------------------------------------------------------------------
// 4. ルートシグニチャの生成
//-----------------------------------------------------------------------------
bool App::CreateRootSignature()
{
    auto rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
    rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
    rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    // ルートパラメータの設定
    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // ルートシグニチャの設定
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = rootSignatureFlags;

    ComPtr<ID3DBlob> pSignatureBlob;
    ComPtr<ID3DBlob> pErrorBlob;

    // シリアライズ
    auto hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1_0,
        pSignatureBlob.GetAddressOf(),
        pErrorBlob.GetAddressOf());
    if (FAILED(hr))
    {
        ShowError(L"3 ルートシグニチャ シリアライズ", hr);
        return false;
    }

    // ルートシグニチャを生成
    hr = m_pDevice->CreateRootSignature(
        0,
        pSignatureBlob->GetBufferPointer(),
        pSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
    if (FAILED(hr))
    {
        ShowError(L"3 ルートシグニチャ生成", hr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// 5. パイプラインステートの生成
//-----------------------------------------------------------------------------
bool App::CreatePipelineState()
{
    // 入力レイアウトの設定
    // 頂点シェーダの入力に合わせて設定する必要がある
    D3D12_INPUT_ELEMENT_DESC inputElements[2];
    // 頂点座標の設定
    inputElements[0].SemanticName = "POSITION";
    inputElements[0].SemanticIndex = 0; // 同じSemantic名のものがない限り0
    inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT; // 頂点シェーダの入力に合わせる
    inputElements[0].InputSlot = 0; // IASetVertexBuffers()で何番スロットにどのバッファを置くかを決める.→ここでその頂点バッファのスロット番号を決定する
    inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT; // 前の要素の直後に自動配置
    inputElements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElements[0].InstanceDataStepRate = 0; // 入力要素が頂点ごとなら0,インスタンスごとなら1以上の値を設定する

    // 頂点カラーの設定
    inputElements[1].SemanticName = "COLOR";
    inputElements[1].SemanticIndex = 0;
    inputElements[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    inputElements[1].InputSlot = 0;
    inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElements[1].InstanceDataStepRate = 0;

    // ラスタライザーステートの設定：ラスタライズを行うときの状態
    D3D12_RASTERIZER_DESC rasterizerDesc;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 三角形を塗りつぶし
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // 前面も背面も描画する
    rasterizerDesc.FrontCounterClockwise = FALSE; // 頂点の並び順が反時計回りなら前面とする
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS; // 深度値に加算する値。0でよい
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP; // DepthBias の最大値。0でよい
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS; // 深度値に加算する値の、勾配に応じたスケーリング値。0でよい
    rasterizerDesc.DepthClipEnable = FALSE; // 距離に基づくクリッピングを有効にするか。無効でよい
    rasterizerDesc.MultisampleEnable = FALSE; // マルチサンプリング
    rasterizerDesc.AntialiasedLineEnable = TRUE; // アンチエイリアス
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF; // 厳密判定OFF

    // レンダーターゲットのブレンド設定
    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };

    // ブレンドステートの設定
    D3D12_BLEND_DESC blendDesc;
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
        blendDesc.RenderTarget[i] = renderTargetBlendDesc;
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

    // パイプラインステートの設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElements, _countof(inputElements) };
    psoDesc.pRootSignature = m_pRootSignature.Get();
    psoDesc.VS = { pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize() };
    psoDesc.PS = { pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize() };
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    // パイプラインステートを生成
    hr = m_pDevice->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(m_pPSO.GetAddressOf()));
    if (FAILED(hr))
    {
        ShowError(L"4 PSO生成", hr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// 6. ビューポートとシザー矩形の設定
//-----------------------------------------------------------------------------
void App::InitViewport()
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
