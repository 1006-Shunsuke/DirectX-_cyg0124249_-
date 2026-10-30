//-----------------------------------------------------------------------------
// File : Mesh.cpp
// Desc : メッシュの読み込み。assimp で読んだデータを自前の Mesh / Material に変換する
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#define NOMINMAX            // Windows.h の min/max マクロが assimp の std::min/max とぶつからないようにする
#include "Mesh.h"
#include <Windows.h>        // WideCharToMultiByte で文字コードを変換する
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace DirectX;

//-----------------------------------------------------------------------------
// MeshVertex の入力レイアウト
// 頂点シェーダの VSInput と「セマンティクス名・型・順番」をそろえる
//-----------------------------------------------------------------------------
const D3D12_INPUT_ELEMENT_DESC MeshVertex::InputElements[MeshVertex::InputElementCount] =
{
    // 名前        番号  型                               スロット  オフセット：前の要素の直後         頂点ごとのデータ
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

// PSO にはこの「配列の先頭 + 個数」を渡す
const D3D12_INPUT_LAYOUT_DESC MeshVertex::InputLayout =
{
    MeshVertex::InputElements,
    MeshVertex::InputElementCount
};

// 頂点1個は 4バイトの float × 11個 = 44バイト。内訳は位置3・法線3・UV2・接線3。ずれていたらビルドエラーにする
static_assert(sizeof(MeshVertex) == 44, "MeshVertex のサイズが入力レイアウトと合っていません");


namespace {

    //-------------------------------------------------------------------------
    // wchar_t(UTF-16) の文字列を UTF-8 の std::string に変換する
    // assimp はファイルパスを UTF-8 の char* でしか受け取らないため
    // ※ <codecvt> は C++17 で非推奨になり、SDLチェックONだとエラーになるので Windows API を使う
    //-------------------------------------------------------------------------
    std::string ToUTF8(const std::wstring& value)
    {
        if (value.empty())
        {
            return std::string();
        }

        // 1回目：変換後に何バイト必要かを調べる。末尾の '\0' も含む
        auto length = WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (length <= 0)
        {
            return std::string();
        }

        // 2回目：確保した領域に実際に変換する
        std::string result(length, '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), -1, &result[0], length, nullptr, nullptr);

        result.resize(length - 1);  // 末尾の '\0' の分を取り除く
        return result;
    }

    ///////////////////////////////////////////////////////////////////////////
    // MeshLoader : assimp でファイルを読み、Mesh / Material に詰め替えるクラス
    ///////////////////////////////////////////////////////////////////////////
    class MeshLoader
    {
    public:
        MeshLoader() = default;
        ~MeshLoader() = default;

        bool Load(
            const wchar_t*          filename,
            std::vector<Mesh>&      meshes,
            std::vector<Material>&  materials);

    private:
        void ParseMesh(Mesh& dstMesh, const aiMesh* pSrcMesh);
        void ParseMaterial(Material& dstMaterial, const aiMaterial* pSrcMaterial);
    };

    //-------------------------------------------------------------------------
    // メッシュのロード
    //-------------------------------------------------------------------------
    bool MeshLoader::Load
    (
        const wchar_t*          filename,
        std::vector<Mesh>&      meshes,
        std::vector<Material>&  materials
    )
    {
        if (filename == nullptr)
        {
            return false;
        }

        // wchar_t から UTF-8 の char 型に変換する
        auto path = ToUTF8(filename);

        // 読み込み時に assimp にやってもらうポストプロセス
        Assimp::Importer importer;
        unsigned int flag = 0;
        flag |= aiProcess_Triangulate;              // 四角形などをすべて三角形に分割
        flag |= aiProcess_PreTransformVertices;     // ノードの行列を頂点に焼き込み、階層をなくす
        flag |= aiProcess_CalcTangentSpace;         // 接線を計算
        flag |= aiProcess_GenSmoothNormals;         // 法線が無ければなめらかな法線を作る
        flag |= aiProcess_GenUVCoords;
        flag |= aiProcess_RemoveRedundantMaterials; // 重複したマテリアルをまとめる
        flag |= aiProcess_OptimizeMeshes;           // 細かいメッシュをまとめて描画回数を減らす
        flag |= aiProcess_FlipUVs;                  // UVの上下を反転する。OBJなどは下が v=0、DirectX は上が v=0 のため

        // ファイルを読み込む。pScene の中身は importer が持っていて、importer が消えると一緒に消える
        auto pScene = importer.ReadFile(path, flag);

        // チェック
        if (pScene == nullptr)
        {
            // 失敗した理由を Visual Studio の「出力」ウィンドウに出す
            OutputDebugStringA("[LoadMesh] ");
            OutputDebugStringA(importer.GetErrorString());
            OutputDebugStringA("\n");
            return false;
        }

        // メッシュのメモリを確保
        meshes.clear();
        meshes.resize(pScene->mNumMeshes);

        // メッシュデータを変換
        for (size_t i = 0; i < meshes.size(); ++i)
        {
            const auto pMesh = pScene->mMeshes[i];
            ParseMesh(meshes[i], pMesh);
        }

        // マテリアルのメモリを確保
        materials.clear();
        materials.resize(pScene->mNumMaterials);

        // マテリアルデータを変換
        for (size_t i = 0; i < materials.size(); ++i)
        {
            const auto pMaterial = pScene->mMaterials[i];
            ParseMaterial(materials[i], pMaterial);
        }

        // 不要になったのでクリア
        pScene = nullptr;

        // 正常終了
        return true;
    }

    //-------------------------------------------------------------------------
    // メッシュデータの解析：aiMesh → Mesh
    //-------------------------------------------------------------------------
    void MeshLoader::ParseMesh(Mesh& dstMesh, const aiMesh* pSrcMesh)
    {
        // マテリアル番号を設定
        dstMesh.MaterialId = pSrcMesh->mMaterialIndex;

        // 情報が無かった時に使う「0ベクトル」
        aiVector3D zero3D(0.0f, 0.0f, 0.0f);

        // 1. 頂点データ
        dstMesh.Vertices.resize(pSrcMesh->mNumVertices);

        for (auto i = 0u; i < pSrcMesh->mNumVertices; ++i)
        {
            // assimp は「位置の配列」「法線の配列」…と要素ごとに別の配列で持っているので、
            // 同じ番号 i を取り出して1つの頂点にまとめる
            auto pPosition = &(pSrcMesh->mVertices[i]);
            auto pNormal   = (pSrcMesh->HasNormals())               ? &(pSrcMesh->mNormals[i])          : &zero3D;
            auto pTexCoord = (pSrcMesh->HasTextureCoords(0))        ? &(pSrcMesh->mTextureCoords[0][i]) : &zero3D; // UVは0番目のセットだけ使う
            auto pTangent  = (pSrcMesh->HasTangentsAndBitangents()) ? &(pSrcMesh->mTangents[i])         : &zero3D;

            dstMesh.Vertices[i] = MeshVertex(
                XMFLOAT3(pPosition->x, pPosition->y, pPosition->z),
                XMFLOAT3(pNormal->x,   pNormal->y,   pNormal->z),
                XMFLOAT2(pTexCoord->x, pTexCoord->y),           // UVは x,y だけ使う。z は3Dテクスチャ用
                XMFLOAT3(pTangent->x,  pTangent->y,  pTangent->z));
        }

        // 2. インデックスデータ
        dstMesh.Indices.clear();
        dstMesh.Indices.reserve(pSrcMesh->mNumFaces * 3);

        for (auto i = 0u; i < pSrcMesh->mNumFaces; ++i)
        {
            const auto& face = pSrcMesh->mFaces[i];

            // Triangulate で三角形にしているが、点や線が混ざっていることがあるので三角形以外は飛ばす
            if (face.mNumIndices != 3)
            {
                continue;
            }

            dstMesh.Indices.push_back(face.mIndices[0]);
            dstMesh.Indices.push_back(face.mIndices[1]);
            dstMesh.Indices.push_back(face.mIndices[2]);
        }
    }

    //-------------------------------------------------------------------------
    // マテリアルデータの解析：aiMaterial → Material
    // ファイルに値が無い項目は、先に入れた既定値のままにする
    //-------------------------------------------------------------------------
    void MeshLoader::ParseMaterial(Material& dstMaterial, const aiMaterial* pSrcMaterial)
    {
        // 既定値
        dstMaterial.Diffuse    = XMFLOAT3(0.5f, 0.5f, 0.5f);
        dstMaterial.Specular   = XMFLOAT3(0.0f, 0.0f, 0.0f);
        dstMaterial.Alpha      = 1.0f;
        dstMaterial.Shininess  = 0.0f;
        dstMaterial.DiffuseMap.clear();

        // 拡散反射色
        {
            aiColor3D color(0.0f, 0.0f, 0.0f);
            if (pSrcMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
            {
                dstMaterial.Diffuse = XMFLOAT3(color.r, color.g, color.b);
            }
        }

        // 鏡面反射色
        {
            aiColor3D color(0.0f, 0.0f, 0.0f);
            if (pSrcMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
            {
                dstMaterial.Specular = XMFLOAT3(color.r, color.g, color.b);
            }
        }

        // 鏡面反射の鋭さ
        {
            auto shininess = 0.0f;
            if (pSrcMaterial->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
            {
                dstMaterial.Shininess = shininess;
            }
        }

        // 不透明度
        {
            auto opacity = 1.0f;
            if (pSrcMaterial->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
            {
                dstMaterial.Alpha = opacity;
            }
        }

        // ディフューズマップのテクスチャのパス
        {
            aiString path;
            if (pSrcMaterial->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), path) == AI_SUCCESS)
            {
                dstMaterial.DiffuseMap = std::string(path.C_Str());
            }
        }
    }

} // namespace


//-----------------------------------------------------------------------------
// メッシュを読み込む。外から呼ぶのはこの関数だけ
//-----------------------------------------------------------------------------
bool LoadMesh
(
    const wchar_t*          filename,
    std::vector<Mesh>&      meshes,
    std::vector<Material>&  materials
)
{
    MeshLoader loader;
    return loader.Load(filename, meshes, materials);
}
