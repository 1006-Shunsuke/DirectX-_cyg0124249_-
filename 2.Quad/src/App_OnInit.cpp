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
    XMFLOAT4 Color;
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
// 頂点シェーダ(IA)の入力データに合致するように頂点バッファを作成する。
// 頂点データをGPU側が扱える頂点バッファの中に入れてGPUへの渡し方まで用意
//-----------------------------------------------------------------------------
bool App::CreateVertexBuffer()
{
    // 頂点データ(中身)
    Vertex vertices[] = {
        { XMFLOAT3(-1.0f, 1.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f, 0.0f),XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
    };

    m_VertexCount = _countof(vertices);
    // 1-1,リソースの生成

    // ヒーププロパティ
    // どの種類のメモリに置くか,本件だとCPUから書ける実メモリに置く
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // GPUへ
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // お任せ
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; // お任せ
    heapProps.CreationNodeMask = 1; // 単一GPU
    heapProps.VisibleNodeMask = 1; // 単一GPU

    // リソース(空の入れ物)の設定
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

    // 頂点データをマッピング先に設定 .
    memcpy(ptr, vertices, sizeof(vertices));

    // マッピング解除 .
    m_pVB->Unmap(0, nullptr);

    // 1-3View構造体を作成
    // 頂点バッファビューの設定 .(描画コマンドのために)
    m_VBV.BufferLocation = m_pVB->GetGPUVirtualAddress(); // GPU仮想アドレス
    m_VBV.SizeInBytes = static_cast<UINT>(sizeof(vertices)); // 頂点バッファ全体のサイズ
    m_VBV.StrideInBytes = static_cast<UINT>(sizeof(Vertex)); // 1頂点あたりのサイズ

    return true;
}

//-----------------------------------------------------------------------------
// 2. インデックスバッファの生成
// 頂点シェーダ(IA)の入力データに合致するように頂点バッファを作成する。
// 頂点データをGPU側が扱える頂点バッファの中に入れてGPUへの渡し方まで用意
//-----------------------------------------------------------------------------
bool App::CreateIndexBuffer()
{
    uint32_t indices[] = { 0,1,2, 0,2,3 };
    m_IndexCount = _countof(indices);

    // 2-1,リソースの生成

    // ヒーププロパティ
    // どの種類のメモリに置くか,本件だとCPUから書ける実メモリに置く
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // GPUへ
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // お任せ
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; // お任せ
    heapProps.CreationNodeMask = 1; // 単一GPU
    heapProps.VisibleNodeMask = 1; // 単一GPU

    // リソース(空の入れ物)の設定
    // この場合のメモリはGPUがアクセスできるメモリ上の「空の入れ物」
    // 確保するメモリの「形・サイズ・種類」をドライバに伝える
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファ
    resourceDesc.Alignment = 0;
    resourceDesc.Width = sizeof(indices); // Bufferの場合はバッファサイズ,Textureの場合は横幅
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
    // m_pIBはリソースを操作するためのオブジェクトを指すポインタ
    // 確保した空メモリにCPU側のインデックスデータを実際に書き込む

    void* ptr = nullptr;
    hr = m_pIB->Map(0, nullptr, &ptr);
    if (FAILED(hr))
    {
        ShowError(L"2-2 インデックスバッファMap", hr);
        return false;
    }

    // インデックスデータをマッピング先に設定 .
    memcpy(ptr, indices, sizeof(indices));

    // マッピング解除 .
    m_pIB->Unmap(0, nullptr);

    // 2-3View構造体を作成
    // インデックスバッファビューの設定 .(描画コマンドのために)
    m_IBV.BufferLocation = m_pIB->GetGPUVirtualAddress(); // GPU仮想アドレス
    m_IBV.Format = DXGI_FORMAT_R32_UINT; // インデックス1個の型(uint32_t なので R32_UINT)
    m_IBV.SizeInBytes = static_cast<UINT>(sizeof(indices)); // インデックスバッファ全体のサイズ

    return true;
}

//-----------------------------------------------------------------------------
// 3. 定数バッファの生成
// ディスクリプタヒープの生成
// シェーダーのregister(b0)に結び付ける必要がある
//-----------------------------------------------------------------------------
bool App::CreateConstantBuffers()
{
    {
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvHeapDesc.NumDescriptors = 1 * FrameCount;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        cbvHeapDesc.NodeMask = 0;

        auto hr = m_pDevice->CreateDescriptorHeap(
            &cbvHeapDesc,
            IID_PPV_ARGS(m_pHeapCBV.GetAddressOf()));
        if (FAILED(hr))
        {
            ShowError(L"3-4 CBVヒープ生成", hr);
            return false;
        }
    }

    // メモリ領域やリソースの設定
    {

        // ヒーププロパティ .
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        // リソースの設定 .
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

        auto cbvDescriptorSize = m_pDevice
            ->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        for (auto i = 0u; i < FrameCount; ++i)
        {
            // 3-1.リソースの作成
            auto hr = m_pDevice->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_pCB[i].GetAddressOf()));
            if (FAILED(hr))
            {
                ShowError(L"3-1 定数バッファ生成", hr);
                return false;
            }

            // 3-5.ディスクリプタの作成
            auto address = m_pCB[i]->GetGPUVirtualAddress();
            auto handleCPU = m_pHeapCBV->GetCPUDescriptorHandleForHeapStart();
            auto handleGPU = m_pHeapCBV->GetGPUDescriptorHandleForHeapStart();

            handleCPU.ptr += cbvDescriptorSize * i;
            handleGPU.ptr += cbvDescriptorSize * i;

            // 定数バッファビューの設定 .
            m_CBV[i].HandleCPU = handleCPU;
            m_CBV[i].HandleGPU = handleGPU;
            m_CBV[i].Desc.BufferLocation = address;
            m_CBV[i].Desc.SizeInBytes = sizeof(Transform);

            // 定数バッファビューを生成 .
            m_pDevice->CreateConstantBufferView(&m_CBV[i].Desc, handleCPU);

            // 3-2.マッピング
            hr = m_pCB[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_CBV[i].pBuffer));
            if (FAILED(hr))
            {
                ShowError(L"3-2 定数バッファMap", hr);
                return false;
            }

            auto eyePos = DirectX::XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f);
            auto targetPos = DirectX::XMVectorZero();
            auto upward = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

            constexpr auto fovY = DirectX::XMConvertToRadians(37.5f);
            auto aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);

            // 変換行列の設定 .
            m_CBV[i].pBuffer->World = DirectX::XMMatrixIdentity();
            m_CBV[i].pBuffer->View = DirectX::XMMatrixLookAtRH(eyePos, targetPos, upward);
            m_CBV[i].pBuffer->Proj = DirectX::XMMatrixPerspectiveFovRH(fovY, aspect, 1.0f, 1000.0f);
        }
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

    // ルートパラメータの設定 .
    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.Descriptor.ShaderRegister = 0;
    rootParameter.Descriptor.RegisterSpace = 0;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // ルートシグニチャの設定 .
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pParameters = &rootParameter;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = rootSignatureFlags;

    ComPtr<ID3DBlob> pSignatureBlob;
    ComPtr<ID3DBlob> pErrorBlob;

    // シリアライズ .
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

    // ルートシグニチャを生成 .
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
    // 入力レイアウトの設定 .
    D3D12_INPUT_ELEMENT_DESC inputElements[2];
    inputElements[0].SemanticName = "POSITION";
    inputElements[0].SemanticIndex = 0;
    inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElements[0].InputSlot = 0;
    inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElements[0].InstanceDataStepRate = 0;

    inputElements[1].SemanticName = "COLOR";
    inputElements[1].SemanticIndex = 0;
    inputElements[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElements[1].InputSlot = 0;
    inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElements[1].InstanceDataStepRate = 0;

    // ラスタライザーステートの設定 .
    D3D12_RASTERIZER_DESC rasterizerDesc;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = FALSE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // レンダーターゲットのブレンド設定 .
    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };

    // ブレンドステートの設定 .
    D3D12_BLEND_DESC blendDesc;
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
        blendDesc.RenderTarget[i] = renderTargetBlendDesc;
    }

    ComPtr<ID3DBlob> pVSBlob;
    ComPtr<ID3DBlob> pPSBlob;

    // 頂点シェーダ読み込み .
    auto vsPath = GetExeDirectory() + L"SimpleVS.cso";
    auto hr = D3DReadFileToBlob(vsPath.c_str(), pVSBlob.GetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(nullptr, vsPath.c_str(), L"VS読み込み失敗", MB_OK);
        return false;
    }

    // ピクセルシェーダ読み込み .
    auto psPath = GetExeDirectory() + L"SimplePS.cso";
    hr = D3DReadFileToBlob(psPath.c_str(), pPSBlob.GetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(nullptr, psPath.c_str(), L"PS読み込み失敗", MB_OK);
        return false;
    }

    // パイプラインステートの設定 .
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

    // パイプラインステートを生成 .
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
