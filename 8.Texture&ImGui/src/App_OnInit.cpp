//-----------------------------------------------------------------------------
// File : App_OnInit.cpp
// Desc : 描画に使うGPUリソースの初期化。OnInit
//-----------------------------------------------------------------------------
#include "App.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <Timer.h>
#include <ResourceUploadBatch.h>    // DirectXTK12：テクスチャをGPUへ転送する
#include <DDSTextureLoader.h>       // DirectXTK12：PNG・JPGなどの画像を読み込む

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

    // ファイルを探す。作業フォルダ → exeのフォルダ → その親 … の順に調べる
    // VSから実行すると作業フォルダは project\ 、exe直接なら x64\Debug\ になるため
    bool SearchFilePath(const wchar_t* filename, std::wstring& result)
    {
        auto exists = [](const std::wstring& path) {
            DWORD attr = GetFileAttributesW(path.c_str());
            return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
        };
        if (exists(filename)) { result = filename; return true; }

        std::wstring dir = GetExeDirectory();
        for (int i = 0; i < 5; ++i)   // 最大5階層上まで
        {
            if (exists(dir + filename)) { result = dir + filename; return true; }
            dir += L"..\\";
        }
        return false;
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

struct Vertex
    //* 変更
{
    XMFLOAT3 Position;
    XMFLOAT2 TexCoord;  // テクスチャ座標 UV。左上が (0,0)、右下が (1,1)
};

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

    //2.定数バッファとテクスチャの生成
    // CBV_SRV_UAV のディスクリプタヒープを1つ作り、次のように並べる
    //   [0] CBV : m_pCB[0] フレーム0用の Transform  → b0
    //   [1] CBV : m_pCB[1] フレーム1用の Transform  → b0
    //   [2] SRV : m_pTexture                          → t0
    {
        // 2.0 1スロット分のバイト数。GPUによって違うので必ず問い合わせる
        // CBVとSRVで確保する1スロットのサイズは変わらないので,CBVとSRVは同じディスクリプタに置くことができる
        const auto incSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // 2.1 ディスクリプタヒープの生成
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = FrameCount + 1;                 // CBV×2 + SRV×1
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // シェーダーから見えるヒープにする。ここがRTV・DSVとの違い
            desc.NodeMask = 0;

            auto hr = m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_pHeapCBV_SRV_UAV.GetAddressOf()));
            if (FAILED(hr))
            {
                ShowError(L"2.1 CBV_SRV_UAVヒープ生成", hr);
                return false;
            }
        }

        //2.2 ヒーププロパティ
        D3D12_HEAP_PROPERTIES prop = {};
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;                      // CPUから書けてGPUが読めるメモリ
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask = 1;
        prop.VisibleNodeMask = 1;

        // 2.3 リソースの設定。GPUから見えるメモリの塊で、中身はただのバイト列
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = sizeof(Transform);                          // alignas(256) なので256の倍数
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        //2.4 カメラ設定
        auto eyePos = XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f);
        auto targetPos = XMVectorZero();
        auto upward = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        constexpr auto fovY = XMConvertToRadians(37.5f);
        auto aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);

        // 2.5 定数バッファとCBVをフレームの数だけ作る
        for (auto i = 0u; i < FrameCount; ++i)
        {
            // 2.5-1 リソースを作成してメモリを確保する
            auto hr = m_pDevice->CreateCommittedResource(
                &prop,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_pCB[i].GetAddressOf()));
            if (FAILED(hr))
            {
                ShowError(L"2.5-1 定数バッファ生成", hr);
                return false;
            }

            // 2.5-2 CBVをヒープの i 番目に書き込む。CBVは定数バッファの読み方の説明書きにあたる
            auto handleCPU = m_pHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart();
            handleCPU.ptr += incSize * i;

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
            cbv.BufferLocation = m_pCB[i]->GetGPUVirtualAddress(); // どのバッファか
            cbv.SizeInBytes = static_cast<UINT>(desc.Width);       // バイト数。256の倍数でなければならない
            m_pDevice->CreateConstantBufferView(&cbv, handleCPU);

            // 2.5-3 マッピング。CPUから書き込めるポインタを持ち続ける
            hr = m_pCB[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_pTransform[i]));
            if (FAILED(hr))
            {
                ShowError(L"2.5-3 定数バッファMap", hr);
                return false;
            }

            // 2.5-4 変換行列の設定
            m_pTransform[i]->World = XMMatrixIdentity();
            m_pTransform[i]->View = XMMatrixLookAtRH(eyePos, targetPos, upward);
            m_pTransform[i]->Proj = XMMatrixPerspectiveFovRH(fovY, aspect, 1.0f, 1000.0f);
        }

        // 2.6 テクスチャの読み込み
        {
            // 2.6-1 画像ファイルを探す。作業フォルダ → exeのフォルダ → その親…の順
            std::wstring texturePath;
            if (!SearchFilePath(L"res\\texture\\SampleTexture.dds", texturePath))
            {
                MessageBoxW(nullptr, L"res\\texture\\SampleTexture.ddsが見つかりません", L"テクスチャ読み込み失敗", MB_OK);
                return false;
            }

            // 2.6-2 DirectXTK12 でDDSを読み込み、Upload → Default のコピーとバリアまでまとめて行う
            DirectX::ResourceUploadBatch batch(m_pDevice.Get());
            batch.Begin();

            auto hr = DirectX::CreateDDSTextureFromFileEx(
                m_pDevice.Get(),
                batch,
                texturePath.c_str(),
                0,                                  // 最大サイズ。0なら制限なし
                D3D12_RESOURCE_FLAG_NONE,
                DirectX::DDS_LOADER_FORCE_SRGB,     // RTVが sRGB なので画像も sRGB として読む
                m_pTexture.GetAddressOf());
            if (FAILED(hr))
            {
                ShowError(L"2.6-2 テクスチャ読み込み", hr);
                return false;
            }

            auto finished = batch.End(m_pQueue.Get());  // コマンドを実行して転送開始
            finished.wait();                            // 転送が終わるまで待つ

            m_pTexture->SetName(L"SampleTexture");      // PIXで見分けやすくする
        }

        // 2.7 SRVをヒープの [FrameCount] 番目に書き込む。SRVはテクスチャの読み方の説明書きにあたる
        {
            auto texDesc = m_pTexture->GetDesc();

            D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
            srv.Format = texDesc.Format;                                    // リソースと同じ形式
            srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv.Texture2D.MostDetailedMip = 0;
            srv.Texture2D.MipLevels = texDesc.MipLevels;
            srv.Texture2D.PlaneSlice = 0;
            srv.Texture2D.ResourceMinLODClamp = 0.0f;

            m_HandleCPU_SRV = m_pHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(); // 書き込む時に使う
            m_HandleGPU_SRV = m_pHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(); // 描画時にルートへ渡す
            m_HandleCPU_SRV.ptr += incSize * FrameCount;// CPUハンドルの設定
            m_HandleGPU_SRV.ptr += incSize * FrameCount;// GPUハンドルの設定

            m_pDevice->CreateShaderResourceView(m_pTexture.Get(), &srv, m_HandleCPU_SRV);
        }
    }

    //3.ルートシグニチャの生成
    //ルートシグニチャは定数バッファやテクスチャ,サンプラーといったシェーダ内で使用するリソースのレイアウトを決めるオブジェクト
    {
        //3-1 ルートシグニチャのフラグ
        // ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : 頂点バッファと入力レイアウトを使う
        // DENY_xxx_SHADER_ROOT_ACCESS        : 使わないドメイン・ハル・ジオメトリシェーダーからは見えなくし、少し速くする
        auto flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        // 3-2 ルートパラメータの配列。シェーダーへの「入口」の一覧
        // [0] b0 : Transform → ディスクリプタテーブル。ヒープの CBV を指す
        // [1] t0 : テクスチャ → ディスクリプタテーブル。ヒープの SRV を指す
        // ※ ルートパラメータの番号と register の番号は別物
        //    番号は描画時の SetGraphicsRoot○○(番号, …) で使う
        D3D12_ROOT_PARAMETER params[2] = {};

        // 3-3 [0] b0 : Transform の定数バッファ
        // 3-3-1 ディスクリプタレンジ : 「ヒープの中の CBV を1個、b0 として使う」という範囲指定
        D3D12_DESCRIPTOR_RANGE rangeCBV = {};
        rangeCBV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;    // 範囲の中身は CBV
        rangeCBV.NumDescriptors = 1;                             // 1個
        rangeCBV.BaseShaderRegister = 0;                         // b0 から
        rangeCBV.RegisterSpace = 0;                              // space0。HLSL で space を書かなければ 0 になる
        rangeCBV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // 前のレンジの続きに置く。今は1個なので先頭になる

        // 3-3-2 ルートパラメータ : 上のレンジを持つディスクリプタテーブル
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].DescriptorTable.NumDescriptorRanges = 1;
        params[0].DescriptorTable.pDescriptorRanges = &rangeCBV;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;    // 頂点シェーダーだけが使う

        // 3-5 [1] t0 : テクスチャの SRV
        // 3-5-1 ディスクリプタレンジ : 「ヒープの中の SRV を1個、t0 として使う」
        D3D12_DESCRIPTOR_RANGE rangeSRV = {};
        rangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;    // 範囲の中身は SRV
        rangeSRV.NumDescriptors = 1;                             // 1個
        rangeSRV.BaseShaderRegister = 0;                         // t0 から
        rangeSRV.RegisterSpace = 0;
        rangeSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // 3-5-2 ルートパラメータ : 上のレンジを持つディスクリプタテーブル
        // テクスチャはルートCBVのようにアドレスを直接渡せないので、テーブル経由で渡す
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = 1;
        params[1].DescriptorTable.pDescriptorRanges = &rangeSRV;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;     // ピクセルシェーダーだけが使う

        // 3-6 s0 : 静的サンプラー。テクスチャの「読み方」を決める
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

        // 3-7 ルートシグニチャの設定。3-1～3-6 をまとめる
        D3D12_ROOT_SIGNATURE_DESC desc = {};
        desc.NumParameters = _countof(params);                   // ルートパラメータの数は3
        desc.NumStaticSamplers = 1;                              // 静的サンプラーの数
        desc.pParameters = params;
        desc.pStaticSamplers = &sampler;
        desc.Flags = flag;

        ComPtr<ID3DBlob> pBlob;         // シリアライズ結果のバイト列
        ComPtr<ID3DBlob> pErrorBlob;    // 失敗した時のエラーメッセージ

        // 3-8 シリアライズ：設定を GPU ドライバに渡せるバイト列に変換する
        auto hr = D3D12SerializeRootSignature(
            &desc,
            D3D_ROOT_SIGNATURE_VERSION_1_0,
            pBlob.GetAddressOf(),
            pErrorBlob.GetAddressOf());
        if (FAILED(hr))
        {
            ShowError(L"3-8 ルートシグニチャ シリアライズ", hr);
            return false;
        }

        // 3-9 ルートシグニチャの生成：バイト列からオブジェクトを作る
        hr = m_pDevice->CreateRootSignature(
            0,                              // ノードマスク。GPU1枚なので0
            pBlob->GetBufferPointer(),
            pBlob->GetBufferSize(),
            IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
        if (FAILED(hr))
        {
            ShowError(L"3-9 ルートシグニチャ生成", hr);
            return false;
        }
    }

    // 4.深度バッファの準備
    {
        constexpr DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;

        // 4.1 DSV用ディスクリプタヒープ
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

        // 4.2 GPUが深度値を保存するテクスチャ
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

        // 4.3 リソース生成
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

        // 4.4  ヒープ内の1スロットにDSVを書き込む
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
        dsv.Format = depthFormat;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv.Flags = D3D12_DSV_FLAG_NONE;
        dsv.Texture2D.MipSlice = 0;

        m_pDevice->CreateDepthStencilView(
            m_pDepthBuffer.Get(), &dsv, m_HandleDSV);
    }



    //5.パイプラインステートの生成
    {
        // 5.0入力レイアウトの設定
        // 頂点シェーダの入力に合わせて設定する必要がある
        D3D12_INPUT_ELEMENT_DESC elements[2];
        // 5.1頂点座標の設定
        elements[0].SemanticName = "POSITION";
        elements[0].SemanticIndex = 0;//同じSemantic名のものがない限り0
        elements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;//頂点シェーダの入力に合わせる
        elements[0].InputSlot = 0;//IASetVertexBuffers()で何番スロットにどのバッファを置くかを決める.→ここでその頂点バッファのスロット番号を決定する
        elements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;//前の要素の直後に自動配置
        elements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        elements[0].InstanceDataStepRate = 0;//入力要素が頂点ごとなら0,インスタンスごとなら1以上の値を設定する

        // 5.2 頂点カラー・UV座標の設定
        elements[1].SemanticName = "TEXCOORD";
        elements[1].SemanticIndex = 0;
        elements[1].Format = DXGI_FORMAT_R32G32_FLOAT; // XMFLOAT2 に合わせる
        elements[1].InputSlot = 0;
        elements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        elements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        elements[1].InstanceDataStepRate = 0;

        // 5.3 ラスタライザーステートの設定：ラスタライズを行うときの状態
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

        // 5.4レンダーターゲットのブレンド設定
        D3D12_RENDER_TARGET_BLEND_DESC descRTBS = {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL
        };

        // 5.5ブレンドステートの設定
        D3D12_BLEND_DESC descBS;
        descBS.AlphaToCoverageEnable = FALSE;
        descBS.IndependentBlendEnable = FALSE;
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            descBS.RenderTarget[i] = descRTBS;
        }

        ComPtr<ID3DBlob> pVSBlob;
        ComPtr<ID3DBlob> pPSBlob;

        // 5.6頂点シェーダ読み込み
        auto vsPath = GetExeDirectory() + L"SimpleTexVS.cso";
        auto hr = D3DReadFileToBlob(vsPath.c_str(), pVSBlob.GetAddressOf());
        if (FAILED(hr))
        {
            MessageBoxW(nullptr, vsPath.c_str(), L"VS読み込み失敗", MB_OK);
            return false;
        }

        // 5.7ピクセルシェーダ読み込み
        auto psPath = GetExeDirectory() + L"SimpleTexPS.cso";
        hr = D3DReadFileToBlob(psPath.c_str(), pPSBlob.GetAddressOf());
        if (FAILED(hr))
        {
            MessageBoxW(nullptr, psPath.c_str(), L"PS読み込み失敗", MB_OK);
            return false;
        }
        // 5.8深度ステンシルステートの設定
        D3D12_DEPTH_STENCIL_DESC descDSS = {};
        descDSS.DepthEnable = TRUE;// 深度テストを有効にする
        descDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;// テストを通過した部分の深度を書き込む
        descDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;// 保存済みの深度より小さい＝手前なら通過
        descDSS.StencilEnable = FALSE;// 今回はステンシルを使わない

        // 5.9パイプラインステートの設定
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

        // 5.10パイプラインステートを生成
        hr = m_pDevice->CreateGraphicsPipelineState(
            &desc,
            IID_PPV_ARGS(m_pPSO.GetAddressOf()));
        if (FAILED(hr))
        {
            ShowError(L"4 PSO生成", hr);
            return false;
        }
    }

    //6.ビューポートとシザー矩形の設定
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
