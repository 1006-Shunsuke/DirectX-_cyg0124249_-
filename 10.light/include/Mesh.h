//-----------------------------------------------------------------------------
// File : Mesh.h
// Desc : 3Dモデルのメッシュの読み込み。assimp を使ってファイルを読み、
//        頂点・インデックス・マテリアルを自前の構造体に変換する
//-----------------------------------------------------------------------------
#pragma once

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <d3d12.h>
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
// 1. MeshVertex : 頂点1個分のデータ
///////////////////////////////////////////////////////////////////////////////
struct MeshVertex
{
    DirectX::XMFLOAT3   Position;   // 位置
    DirectX::XMFLOAT3   Normal;     // 法線：面の向き。ライティングで使う
    DirectX::XMFLOAT2   TexCoord;   // テクスチャ座標 UV
    DirectX::XMFLOAT3   Tangent;    // 接線：法線マップで使う

    MeshVertex() = default;

    MeshVertex(
        DirectX::XMFLOAT3 const& position,
        DirectX::XMFLOAT3 const& normal,
        DirectX::XMFLOAT2 const& texcoord,
        DirectX::XMFLOAT3 const& tangent)
        : Position(position)
        , Normal(normal)
        , TexCoord(texcoord)
        , Tangent(tangent)
    {}

    // PSO に渡す入力レイアウト。この構造体の並びをGPUに教える
    static const D3D12_INPUT_LAYOUT_DESC InputLayout;

private:
    static const int InputElementCount = 4;                              // 要素の数：位置・法線・UV・接線
    static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount]; // 各要素の説明。Mesh.cpp で定義する
};

///////////////////////////////////////////////////////////////////////////////
// 2. Material : 表面の材質
///////////////////////////////////////////////////////////////////////////////
struct Material
{
    DirectX::XMFLOAT3   Diffuse;    // 拡散反射色：基本の色
    DirectX::XMFLOAT3   Specular;   // 鏡面反射色：ハイライトの色
    float               Alpha;      // 不透明度。1で不透明
    float               Shininess;  // 鏡面反射の鋭さ
    std::string         DiffuseMap; // テクスチャのファイルパス。UTF-8で、無ければ空
};

///////////////////////////////////////////////////////////////////////////////
// 3. Mesh : メッシュ1個分。同じマテリアルを使う三角形の集まり
///////////////////////////////////////////////////////////////////////////////
struct Mesh
{
    std::vector<MeshVertex> Vertices;   // 頂点の配列
    std::vector<uint32_t>   Indices;    // インデックスの配列。3個で三角形1枚になる
    uint32_t                MaterialId; // 使うマテリアルの番号。materials[MaterialId] で引く
};

//-----------------------------------------------------------------------------
// 4. メッシュを読み込む
//    filename  : 読み込むファイル。.obj / .fbx / .gltf など assimp が読める形式
//    meshes    : 読み込んだメッシュが入る
//    materials : 読み込んだマテリアルが入る
//    戻り値    : 成功したら true
//-----------------------------------------------------------------------------
bool LoadMesh(
    const wchar_t*          filename,
    std::vector<Mesh>&      meshes,
    std::vector<Material>&  materials);
