//-----------------------------------------------------------------------------
// File : App_Helper.cpp
// Desc : GPU リソース作成の共通処理。複数の初期化処理から使う
//-----------------------------------------------------------------------------
#include "App.h"

//-----------------------------------------------------------------------------
// UPLOAD ヒープにバッファを作る
//   size      : バッファの大きさ。単位はバイト
//   pInitData : 最初に書き込むデータ。nullptr なら書き込まない
//   pBuffer   : 作ったバッファを受け取る
//   戻り値    : 成功なら S_OK
//
// UPLOAD ヒープは「CPU から書けて GPU から読めるメモリ」。
// 頂点バッファ・インデックスバッファ・定数バッファのように、CPU 側でデータを用意して GPU に渡すものに使う
//-----------------------------------------------------------------------------
HRESULT App::CreateUploadBuffer(UINT64 size, const void* pInitData, ComPtr<ID3D12Resource>& pBuffer)
{
    // 1. ヒーププロパティ：どの種類のメモリに置くか
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;                        // CPU から書けて GPU が読めるメモリ
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;    // お任せ
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;     // お任せ
    heapProps.CreationNodeMask = 1;                                 // 単一GPU
    heapProps.VisibleNodeMask = 1;                                  // 単一GPU

    // 2. リソースの設定：中身はまだ空の入れ物
    // 確保するメモリの「形・サイズ・種類」をドライバに伝える
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;       // バッファ
    resourceDesc.Alignment = 0;
    resourceDesc.Width = size;                                      // Bufferの場合はバッファサイズ、Textureの場合は横幅
    resourceDesc.Height = 1;                                        // Bufferの場合は1、Textureの場合は縦幅
    resourceDesc.DepthOrArraySize = 1;                              // Bufferの場合は1、Textureの場合は配列数。3次元テクスチャなら奥行き
    resourceDesc.MipLevels = 1;                                     // Textureの場合はミップマップのレベル数
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;                      // Textureの場合はピクセルフォーマット
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;                            // Textureの場合はマルチサンプルを設定
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;           // バッファは必ず ROW_MAJOR
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // 3. 生成
    // 設定をもとに実際に GPU メモリを確保し、リソースを作る。ヒープもこの中で一緒に作られる
    auto hr = m_pDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,  // UPLOAD ヒープのリソースは、この状態で作る決まり
        nullptr,
        IID_PPV_ARGS(pBuffer.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        return hr;
    }

    // 4. 初期データの書き込み。指定されたときだけ行う
    // Map で CPU から書き込めるアドレスを受け取り、データをコピーしてから Unmap する
    if (pInitData != nullptr)
    {
        void* ptr = nullptr;
        hr = pBuffer->Map(0, nullptr, &ptr);
        if (FAILED(hr))
        {
            return hr;
        }
        memcpy(ptr, pInitData, static_cast<size_t>(size));
        pBuffer->Unmap(0, nullptr);
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// ディスクリプタヒープを作る
//   type    : ヒープの種類。RTV / DSV / CBV_SRV_UAV
//   count   : 入れられるディスクリプタの数
//   flags   : SHADER_VISIBLE ならシェーダーから見える。指定できるのは CBV_SRV_UAV のみ
//   pHeap   : 作ったヒープを受け取る
//   戻り値  : 成功なら S_OK
//-----------------------------------------------------------------------------
HRESULT App::CreateDescriptorHeap(
    D3D12_DESCRIPTOR_HEAP_TYPE      type,
    UINT                            count,
    D3D12_DESCRIPTOR_HEAP_FLAGS     flags,
    ComPtr<ID3D12DescriptorHeap>&   pHeap)
{
    // この関数の中ではヒープの設定は1つだけなので desc で十分
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = count;
    desc.Flags = flags;
    desc.NodeMask = 0;     // 単一GPU

    return m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(pHeap.ReleaseAndGetAddressOf()));
}
