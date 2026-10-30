//-----------------------------------------------------------------------------
// File : App_OnInit.cpp
// Desc : 描画に使うGPUリソースの初期化。OnInit
//-----------------------------------------------------------------------------
#include "App.h"
#include <cstdio>
#include <string>
#include <ResourceUploadBatch.h>    // DirectXTK12：テクスチャをGPUへ転送する
#include <DDSTextureLoader.h>       // DirectXTK12：PNG・JPGなどの画像を読み込む

using namespace DirectX;


namespace
{
    // Shaderパスの受け取り
    std::wstring GetExeDirectory()
    {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring s(path);
        return s.substr(0, s.find_last_of(L"\\/") + 1);
    }

    // ファイルを探す。作業フォルダ → exeのフォルダ → その親 … の順に調べる
    // VSから実行すると作業フォルダは project\ 、exe直接なら x64\Debug\ になるため
    bool SearchFilePath(const wchar_t* filename, std::wstring& result)
    {
        auto exists = [](const std::wstring& path)
        {
            DWORD attr = GetFileAttributesW(path.c_str());
            return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
        };
        if (exists(filename))
        {
            result = filename;
            return true;
        }

        std::wstring dir = GetExeDirectory();
        for (int i = 0; i < 5; ++i)   // 最大5階層上まで
        {
            if (exists(dir + filename))
            {
                result = dir + filename;
                return true;
            }
            dir += L"..\\";
        }
        return false;
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
    // 
{
    XMFLOAT3 Position;
    XMFLOAT2 TexCoord;  // テクスチャ座標 UV。左上が (0,0)、右下が (1,1)
};

//-----------------------------------------------------------------------------
// 初期化時の処理
// 描画に必要な GPU リソースを、下の 1〜9 の順番に作る
//-----------------------------------------------------------------------------
bool App::OnInit()
{
    // どれかが失敗したら、そこで中断する
    if (!CreateVertexBuffer())      { return false; }   // 1. 頂点バッファ
    if (!CreateIndexBuffer())       { return false; }   // 2. インデックスバッファ
    if (!CreateCbvSrvHeap())        { return false; }   // 3. 定数バッファとテクスチャ用のディスクリプタヒープ
    if (!CreateConstantBuffers())   { return false; }   // 4. 定数バッファ
    if (!LoadTexture())             { return false; }   // 5. テクスチャと SRV
    if (!CreateRootSignature())     { return false; }   // 6. ルートシグニチャ
    if (!CreateDepthBuffer())       { return false; }   // 7. 深度バッファ
    if (!CreatePipelineState())     { return false; }   // 8. パイプラインステート
    InitViewport();                                     // 9. ビューポートとシザー矩形

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
        // 各面の4頂点は「外から見て 左上・右上・右下・左下」の順なので、UVは全面 (0,0)(1,0)(1,1)(0,1) でよい
        // +Z 前面
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT2(0.0f, 1.0f) },
        // -Z 背面
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT2(0.0f, 1.0f) },
        // +X 右面
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT2(0.0f, 1.0f) },
        // -X 左面
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT2(0.0f, 1.0f) },
        // +Y 上面
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT2(0.0f, 1.0f) },
        // -Y 下面
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT2(0.0f, 1.0f) },
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
    uint32_t indices[] = {
        0, 1, 2,   0, 2, 3,   // +Z
        4, 5, 6,   4, 6, 7,   // -Z
        8, 9,10,   8,10,11,   // +X
        12,13,14,  12,14,15,   // -X
        16,17,18,  16,18,19,   // +Y
        20,21,22,  20,22,23,   // -Y
    };

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

    // 描画時に使うインデックスの数を覚えておく
    m_IndexCount = _countof(indices);

    return true;
}

//-----------------------------------------------------------------------------
// 3. 定数バッファとテクスチャ用のディスクリプタヒープの生成
// 定数バッファとテクスチャの生成
// CBV_SRV_UAV のディスクリプタヒープを1つ作り、次のように並べる
// [0] CBV : m_pCB[0] フレーム0用の Transform  → b0
// [1] CBV : m_pCB[1] フレーム1用の Transform  → b0
// [2] SRV : m_pTexture                          → t0
//-----------------------------------------------------------------------------
bool App::CreateCbvSrvHeap()
{
    // 3-1 ディスクリプタヒープの生成
    D3D12_DESCRIPTOR_HEAP_DESC cbvSrvHeapDesc = {};
    cbvSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvSrvHeapDesc.NumDescriptors = FrameCount + 1;                 // CBV×2 + SRV×1
    cbvSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // シェーダーから見えるヒープにする。ここがRTV・DSVとの違い
    cbvSrvHeapDesc.NodeMask = 0;

    auto hr = m_pDevice->CreateDescriptorHeap(&cbvSrvHeapDesc, IID_PPV_ARGS(m_pHeapCBV_SRV_UAV.GetAddressOf()));
    if (FAILED(hr))
    {
        ShowError(L"3-1 CBV_SRV_UAVヒープ生成", hr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// 4. 定数バッファの生成
// フレームの数だけ作り、Map したまま持ち続ける
//-----------------------------------------------------------------------------
bool App::CreateConstantBuffers()
{
    // 4-0 1スロット分のバイト数。GPUによって違うので必ず問い合わせる
    // CBVとSRVで確保する1スロットのサイズは変わらないので,CBVとSRVは同じディスクリプタに置くことができる
    const auto incSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 4-1 ヒーププロパティ
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;                      // CPUから書けてGPUが読めるメモリ
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // 4-2 リソースの設定。GPUから見えるメモリの塊で、中身はただのバイト列
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = sizeof(Transform);                          // alignas(256) なので256の倍数
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // 4-3 カメラ設定
    auto eyePos = XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f);
    auto targetPos = XMVectorZero();
    auto upward = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    constexpr auto fovY = XMConvertToRadians(37.5f);
    auto aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);

    // 4-4 定数バッファとCBVをフレームの数だけ作る
    for (auto i = 0u; i < FrameCount; ++i)
    {
        // 4-4-1 リソースを作成してメモリを確保する
        auto hr = m_pDevice->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_pCB[i].GetAddressOf()));
        if (FAILED(hr))
        {
            ShowError(L"4-4-1 定数バッファ生成", hr);
            return false;
        }

        // 4-4-2 CBVをヒープの i 番目に書き込む。CBVは定数バッファの読み方の説明書きにあたる
        auto cbvHandle = m_pHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart();
        cbvHandle.ptr += incSize * i;

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_pCB[i]->GetGPUVirtualAddress(); // どのバッファか
        cbvDesc.SizeInBytes = static_cast<UINT>(resourceDesc.Width);       // バイト数。256の倍数でなければならない
        m_pDevice->CreateConstantBufferView(&cbvDesc, cbvHandle);

        // 4-4-3 マッピング。CPUから書き込めるポインタを持ち続ける
        hr = m_pCB[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_pTransform[i]));
        if (FAILED(hr))
        {
            ShowError(L"4-4-3 定数バッファMap", hr);
            return false;
        }

        // 4-4-4 変換行列の設定
        m_pTransform[i]->World = XMMatrixIdentity();
        m_pTransform[i]->View = XMMatrixLookAtRH(eyePos, targetPos, upward);
        m_pTransform[i]->Proj = XMMatrixPerspectiveFovRH(fovY, aspect, 1.0f, 1000.0f);
    }

    return true;
}

//-----------------------------------------------------------------------------
// 5. テクスチャの読み込みと SRV の作成
//-----------------------------------------------------------------------------
bool App::LoadTexture()
{
    // 5-0 1スロット分のバイト数。GPUによって違うので必ず問い合わせる
    // CBVとSRVで確保する1スロットのサイズは変わらないので,CBVとSRVは同じディスクリプタに置くことができる
    const auto incSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 5-1 テクスチャの読み込み
    // 5-1-1 画像ファイルを探す。作業フォルダ → exeのフォルダ → その親…の順
    std::wstring texturePath;
    if (!SearchFilePath(L"res\\texture\\SampleTexture.dds", texturePath))
    {
        MessageBoxW(nullptr, L"res\\texture\\SampleTexture.ddsが見つかりません", L"テクスチャ読み込み失敗", MB_OK);
        return false;
    }

    // 5-1-2 DirectXTK12 でDDSを読み込み、Upload → Default のコピーとバリアまでまとめて行う
    DirectX::ResourceUploadBatch resourceUpload(m_pDevice.Get());
    resourceUpload.Begin();

    auto hr = DirectX::CreateDDSTextureFromFileEx(
        m_pDevice.Get(),
        resourceUpload,
        texturePath.c_str(),
        0,                                  // 最大サイズ。0なら制限なし
        D3D12_RESOURCE_FLAG_NONE,
        DirectX::DDS_LOADER_FORCE_SRGB,     // RTVが sRGB なので画像も sRGB として読む
        m_pTexture.GetAddressOf());
    if (FAILED(hr))
    {
        ShowError(L"5-1-2 テクスチャ読み込み", hr);
        return false;
    }

    auto finished = resourceUpload.End(m_pQueue.Get());  // コマンドを実行して転送開始
    finished.wait();                            // 転送が終わるまで待つ

    m_pTexture->SetName(L"SampleTexture");      // PIXで見分けやすくする

    // 5-2 SRVをヒープの [FrameCount] 番目に書き込む。SRVはテクスチャの読み方の説明書きにあたる
    auto textureDesc = m_pTexture->GetDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;                                    // リソースと同じ形式
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    auto srvCpuHandle = m_pHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(); // 書き込む時に使う
    m_HandleGPU_SRV = m_pHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(); // 描画時にルートへ渡す
    srvCpuHandle.ptr += incSize * FrameCount; // CPUハンドルの設定
    m_HandleGPU_SRV.ptr += incSize * FrameCount; // GPUハンドルの設定

    m_pDevice->CreateShaderResourceView(m_pTexture.Get(), &srvDesc, srvCpuHandle);

    return true;
}

//-----------------------------------------------------------------------------
// 6. ルートシグニチャの生成
//-----------------------------------------------------------------------------
bool App::CreateRootSignature()
{
    // 6-1 ルートシグニチャのフラグ
    // ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : 頂点バッファと入力レイアウトを使う
    // DENY_xxx_SHADER_ROOT_ACCESS        : 使わないドメイン・ハル・ジオメトリシェーダーからは見えなくし、少し速くする
    auto rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
    rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
    rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    // 6-2 ルートパラメータの配列。シェーダーへの「入口」の一覧
    // [0] b0 : Transform → ディスクリプタテーブル。ヒープの CBV を指す
    // [1] t0 : テクスチャ → ディスクリプタテーブル。ヒープの SRV を指す
    // ※ ルートパラメータの番号と register の番号は別物
    //    番号は描画時の SetGraphicsRoot○○(番号, …) で使う
    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    // 6-3 [0] b0 : Transform の定数バッファ
    // 6-3-1 ディスクリプタレンジ : 「ヒープの中の CBV を1個、b0 として使う」という範囲指定
    D3D12_DESCRIPTOR_RANGE cbvRange = {};
    cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;    // 範囲の中身は CBV
    cbvRange.NumDescriptors = 1;                             // 1個
    cbvRange.BaseShaderRegister = 0;                         // b0 から
    cbvRange.RegisterSpace = 0;                              // space0。HLSL で space を書かなければ 0 になる
    cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // 前のレンジの続きに置く。今は1個なので先頭になる

    // 6-3-2 ルートパラメータ : 上のレンジを持つディスクリプタテーブル
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &cbvRange;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;    // 頂点シェーダーだけが使う

    // 6-5 [1] t0 : テクスチャの SRV
    // 6-5-1 ディスクリプタレンジ : 「ヒープの中の SRV を1個、t0 として使う」
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;    // 範囲の中身は SRV
    srvRange.NumDescriptors = 1;                             // 1個
    srvRange.BaseShaderRegister = 0;                         // t0 から
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // 6-5-2 ルートパラメータ : 上のレンジを持つディスクリプタテーブル
    // テクスチャはルートCBVのようにアドレスを直接渡せないので、テーブル経由で渡す
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;     // ピクセルシェーダーだけが使う

    // 6-6 s0 : 静的サンプラー。テクスチャの「読み方」を決める
    // ルートシグニチャに直接埋め込むので、サンプラー用ヒープは不要
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;        // 縮小・拡大・ミップ間すべて線形補間
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;     // UVが0～1の外なら端の色
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = D3D12_DEFAULT_MIP_LOD_BIAS;         // ミップレベルのずらし量は0
    sampler.MaxAnisotropy = 1;                               // 異方性フィルタ用。今は未使用
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;    // 比較サンプラー用。今は未使用
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; // BORDER モード時の色。今は未使用
    sampler.MinLOD = 0.0f;                                   // 使うミップの範囲：最も細かい 0 から
    sampler.MaxLOD = D3D12_FLOAT32_MAX;                      // 最も粗いものまで
    sampler.ShaderRegister = 0;                              // s0
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 6-7 ルートシグニチャの設定。3-1～3-6 をまとめる
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParameters);                   // ルートパラメータの数は2
    rootSignatureDesc.NumStaticSamplers = 1;                              // 静的サンプラーの数
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.pStaticSamplers = &sampler;
    rootSignatureDesc.Flags = rootSignatureFlags;

    ComPtr<ID3DBlob> pSignatureBlob;         // シリアライズ結果のバイト列
    ComPtr<ID3DBlob> pErrorBlob;    // 失敗した時のエラーメッセージ

    // 6-8 シリアライズ：設定を GPU ドライバに渡せるバイト列に変換する
    auto hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1_0,
        pSignatureBlob.GetAddressOf(),
        pErrorBlob.GetAddressOf());
    if (FAILED(hr))
    {
        ShowError(L"6-8 ルートシグニチャ シリアライズ", hr);
        return false;
    }

    // 6-9 ルートシグニチャの生成：バイト列からオブジェクトを作る
    hr = m_pDevice->CreateRootSignature(
        0,                              // ノードマスク。GPU1枚なので0
        pSignatureBlob->GetBufferPointer(),
        pSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
    if (FAILED(hr))
    {
        ShowError(L"6-9 ルートシグニチャ生成", hr);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
// 7. 深度バッファの生成
//-----------------------------------------------------------------------------
bool App::CreateDepthBuffer()
{
    constexpr DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;

    // 7-1 DSV用ディスクリプタヒープ
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1; //
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; //

    auto hr = m_pDevice->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(m_pHeapDSV.GetAddressOf()));
    if (FAILED(hr))
    {
        ShowError(L"DSVヒープ生成", hr);
        return false;
    }

    m_HandleDSV = m_pHeapDSV->GetCPUDescriptorHandleForHeapStart(); // ハンドルはアドレスの定数値

    // 7-2 GPUが深度値を保存するテクスチャ
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC depthDesc = {}; // デプス画像のサイズ,形式,サンプル数
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = m_Width;
    depthDesc.Height = m_Height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = depthFormat;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClearValue = {}; // クリア値によく使う値
    depthClearValue.Format = depthFormat;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;

    // 7-3 リソース生成
    hr = m_pDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(m_pDepthBuffer.GetAddressOf()));
    if (FAILED(hr))
    {
        ShowError(L"深度バッファ生成", hr);
        return false;
    }

    // 7-4  ヒープ内の1スロットにDSVを書き込む
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = depthFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.Texture2D.MipSlice = 0;

    m_pDevice->CreateDepthStencilView(
        m_pDepthBuffer.Get(), &dsvDesc, m_HandleDSV);

    return true;
}

//-----------------------------------------------------------------------------
// 8. パイプラインステートの生成
//-----------------------------------------------------------------------------
bool App::CreatePipelineState()
{
    // 8-0入力レイアウトの設定
    // 頂点シェーダの入力に合わせて設定する必要がある
    D3D12_INPUT_ELEMENT_DESC inputElements[2];
    // 8-1頂点座標の設定
    inputElements[0].SemanticName = "POSITION";
    inputElements[0].SemanticIndex = 0; // 同じSemantic名のものがない限り0
    inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT; // 頂点シェーダの入力に合わせる
    inputElements[0].InputSlot = 0; // IASetVertexBuffers()で何番スロットにどのバッファを置くかを決める.→ここでその頂点バッファのスロット番号を決定する
    inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT; // 前の要素の直後に自動配置
    inputElements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElements[0].InstanceDataStepRate = 0; // 入力要素が頂点ごとなら0,インスタンスごとなら1以上の値を設定する

    // 8-2 頂点カラー・UV座標の設定
    inputElements[1].SemanticName = "TEXCOORD";
    inputElements[1].SemanticIndex = 0;
    inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT; // XMFLOAT2 に合わせる
    inputElements[1].InputSlot = 0;
    inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElements[1].InstanceDataStepRate = 0;

    // 8-3 ラスタライザーステートの設定：ラスタライズを行うときの状態
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

    // 8-4レンダーターゲットのブレンド設定
    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };

    // 8-5ブレンドステートの設定
    D3D12_BLEND_DESC blendDesc;
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
        blendDesc.RenderTarget[i] = renderTargetBlendDesc;
    }

    ComPtr<ID3DBlob> pVSBlob;
    ComPtr<ID3DBlob> pPSBlob;

    // 8-6頂点シェーダ読み込み
    auto vsPath = GetExeDirectory() + L"SimpleTexVS.cso";
    auto hr = D3DReadFileToBlob(vsPath.c_str(), pVSBlob.GetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(nullptr, vsPath.c_str(), L"VS読み込み失敗", MB_OK);
        return false;
    }

    // 8-7ピクセルシェーダ読み込み
    auto psPath = GetExeDirectory() + L"SimpleTexPS.cso";
    hr = D3DReadFileToBlob(psPath.c_str(), pPSBlob.GetAddressOf());
    if (FAILED(hr))
    {
        MessageBoxW(nullptr, psPath.c_str(), L"PS読み込み失敗", MB_OK);
        return false;
    }
    // 8-8深度ステンシルステートの設定
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE; // 深度テストを有効にする
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // テストを通過した部分の深度を書き込む
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 保存済みの深度より小さい＝手前なら通過
    depthStencilDesc.StencilEnable = FALSE; // 今回はステンシルを使わない

    // 8-9パイプラインステートの設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElements, _countof(inputElements) };
    psoDesc.pRootSignature = m_pRootSignature.Get();
    psoDesc.VS = { pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize() };
    psoDesc.PS = { pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize() };
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState = depthStencilDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    // 8-10パイプラインステートを生成
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
// 9. ビューポートとシザー矩形の設定
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
