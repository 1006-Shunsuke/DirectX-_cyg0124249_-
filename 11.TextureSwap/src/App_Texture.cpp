//-----------------------------------------------------------------------------
// File : App_Texture.cpp
// Desc : テクスチャの読み込みと貼り替え。起動時と、Inspector で選んだときの両方で使う
//-----------------------------------------------------------------------------
#include "App.h"
#include <ResourceUploadBatch.h>    // DirectXTK12：テクスチャをGPUへ転送する
#include <DDSTextureLoader.h>       // DirectXTK12：DDS画像を読み込む
#include <WICTextureLoader.h>       // DirectXTK12：PNG・JPG などを読み込む

namespace
{
    // 拡張子を返す。"a.png" なら ".png"、無ければ空
    std::wstring GetExtension(const std::wstring& path)
    {
        const auto pos = path.find_last_of(L'.');
        return (pos == std::wstring::npos) ? std::wstring() : path.substr(pos);
    }

    // 拡張子が .dds かどうか。大文字・小文字は区別しないので ".DDS" も OK
    bool IsDdsFile(const std::wstring& path)
    {
        return _wcsicmp(GetExtension(path).c_str(), L".dds") == 0;
    }

    // このエンジンで読み込める画像ファイルかどうか
    bool IsTextureFile(const std::wstring& path)
    {
        const wchar_t* extensions[] = { L".dds", L".png", L".jpg", L".jpeg", L".bmp" };
        const auto ext = GetExtension(path);
        for (const auto* e : extensions)
        {
            if (_wcsicmp(ext.c_str(), e) == 0)
            {
                return true;
            }
        }
        return false;
    }

    // wchar_t の文字列を UTF-8 に変換する。ImGui は UTF-8 の文字列を表示するため
    std::string ToUtf8(const std::wstring& value)
    {
        if (value.empty())
        {
            return std::string();
        }

        // 1回目: 変換後に何バイト必要かを調べる。末尾の '\0' を含む
        const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (length <= 0)
        {
            return std::string();
        }

        // 2回目: 確保した領域に実際に変換する
        std::string result(length, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &result[0], length, nullptr, nullptr);
        result.resize(length - 1);  // 末尾の '\0' の分を取り除く
        return result;
    }
}

//-----------------------------------------------------------------------------
// テクスチャを読み込み、ヒープの [FrameCount] 番目に SRV を作る
//   path   : 読み込む画像ファイル。DDS・PNG・JPG・BMP
//   戻り値 : 成功なら S_OK。失敗したときは、今のテクスチャがそのまま残る
//-----------------------------------------------------------------------------
HRESULT App::LoadTextureResource(const std::wstring& path)
{
    // 1. 新しいテクスチャを一時的な変数に読み込む。失敗しても今のテクスチャを壊さないため
    //    拡張子で読み込み方を変え、DirectXTK12 で Upload → Default のコピーとバリアまでまとめて行う
    //    DDS はそのまま読み、PNG・JPG・BMP は Windows の画像読み込み機能である WIC で読む
    ComPtr<ID3D12Resource> pNewTexture;
    DirectX::ResourceUploadBatch resourceUpload(m_pDevice.Get());
    resourceUpload.Begin();

    HRESULT hr = S_OK;
    if (IsDdsFile(path))
    {
        hr = DirectX::CreateDDSTextureFromFileEx(
            m_pDevice.Get(),
            resourceUpload,
            path.c_str(),
            0,                                  // 最大サイズ。0なら制限なし
            D3D12_RESOURCE_FLAG_NONE,
            DirectX::DDS_LOADER_FORCE_SRGB,     // RTVが sRGB なので画像も sRGB として読む
            pNewTexture.GetAddressOf());
    }
    else
    {
        hr = DirectX::CreateWICTextureFromFileEx(
            m_pDevice.Get(),
            resourceUpload,
            path.c_str(),
            0,                                                                  // 最大サイズ。0なら制限なし
            D3D12_RESOURCE_FLAG_NONE,
            DirectX::WIC_LOADER_FORCE_SRGB | DirectX::WIC_LOADER_MIP_AUTOGEN,   // sRGB として読み、ミップマップも作る
            pNewTexture.GetAddressOf());
    }

    // 2. GPU への転送を実行し、終わるまで待つ。失敗したときも、Begin したので End しておく
    auto finished = resourceUpload.End(m_pQueue.Get());
    finished.wait();
    if (FAILED(hr))
    {
        return hr;
    }

    // 3. 今のテクスチャと入れ替える。古いテクスチャはここで解放される
    //    ※ 貼り替えのときは、呼ぶ前に WaitGpu() で GPU が古いテクスチャを使い終わっている必要がある
    m_pTexture = pNewTexture;
    m_pTexture->SetName(path.c_str());  // ファイルのパスを名前にして、PIXで見分けやすくする

    // 4. SRVをヒープの [FrameCount] 番目に書き込む。SRVはテクスチャの読み方の説明書きにあたる
    //    場所は毎回同じなので、描画側の m_HandleGPU_SRV は変えなくてよい
    const auto cbvSrvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const auto textureDesc = m_pTexture->GetDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;                                    // リソースと同じ形式
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    auto srvCpuHandle = m_pHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart();  // 書き込む時に使う
    m_HandleGPU_SRV = m_pHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart();    // 描画時にルートへ渡す
    srvCpuHandle.ptr += cbvSrvDescriptorSize * FrameCount;
    m_HandleGPU_SRV.ptr += cbvSrvDescriptorSize * FrameCount;

    m_pDevice->CreateShaderResourceView(m_pTexture.Get(), &srvDesc, srvCpuHandle);

    return S_OK;
}

//-----------------------------------------------------------------------------
// 今のテクスチャと同じフォルダにある画像ファイルを一覧にして、Inspector に表示する
//   currentPath : 今貼っているテクスチャのパス
//-----------------------------------------------------------------------------
void App::FindTextureFiles(const std::wstring& currentPath)
{
    m_TextureFiles.clear();
    m_TextureNames.clear();
    m_CurrentTexture = -1;
    m_PendingTexture = -1;

    // 1. フォルダ部分とファイル名部分に分ける
    const auto pos = currentPath.find_last_of(L"\\/");
    const auto folder = (pos == std::wstring::npos) ? std::wstring() : currentPath.substr(0, pos + 1);
    const auto currentName = (pos == std::wstring::npos) ? currentPath : currentPath.substr(pos + 1);

    // 2. Windows の FindFirstFile / FindNextFile で、フォルダの中のファイルを1つずつ調べる
    WIN32_FIND_DATAW data = {};
    HANDLE hFind = FindFirstFileW((folder + L"*").c_str(), &data);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        // フォルダと、読み込めない形式のファイルは飛ばす
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 || !IsTextureFile(data.cFileName))
        {
            continue;
        }

        // 今貼っているテクスチャなら、コンボボックスで選択済みにするため番号を覚えておく
        if (_wcsicmp(data.cFileName, currentName.c_str()) == 0)
        {
            m_CurrentTexture = static_cast<int>(m_TextureFiles.size());
        }

        m_TextureFiles.push_back(folder + data.cFileName);
        m_TextureNames.push_back(ToUtf8(data.cFileName));
    } while (FindNextFileW(hFind, &data));

    FindClose(hFind);
}

//-----------------------------------------------------------------------------
// 予約されたテクスチャの貼り替えを実行する
// Render の最初、コマンドの記録を始める前に呼ぶ
//-----------------------------------------------------------------------------
void App::ApplyPendingTexture()
{
    // 予約が無ければ何もしない
    if (m_PendingTexture < 0)
    {
        return;
    }
    const int index = m_PendingTexture;
    m_PendingTexture = -1;

    // GPU が古いテクスチャを使い終わるまで待つ
    // フレームリソースが2つあるので、前のフレームの描画がまだ終わっていない可能性がある
    WaitGpu();

    // 読み込みと SRV の書き換え。失敗したときは今のテクスチャのままにする
    const auto hr = LoadTextureResource(m_TextureFiles[index]);
    if (FAILED(hr))
    {
        OutputDebugStringW((L"[ApplyPendingTexture] 読み込みに失敗したので貼り替えない: " + m_TextureFiles[index] + L"\n").c_str());
        return;
    }
    m_CurrentTexture = index;
}
