//-----------------------------------------------------------------------------
// File : LightVS.hlsl
// Desc : ライティング用の頂点シェーダ
//        位置を画面の座標に変換し、ライティングに使う「ワールド座標」と「法線」をPSへ渡す
//-----------------------------------------------------------------------------

// 頂点シェーダへの入力。MeshVertex::InputLayout と名前・型・順番をそろえる
struct VSInput
{
    float3 Position : POSITION;     // 位置
    float3 Normal   : NORMAL;       // 法線：面の向き
    float2 TexCoord : TEXCOORD;     // テクスチャ座標 UV
    float3 Tangent  : TANGENT;      // 接線：今は未使用で、法線マップで使う
};

// ピクセルシェーダへ渡すデータ。三角形の中で補間されてから届く
struct VSOutput
{
    float4 Position : SV_Position;  // 画面上の位置
    float2 TexCoord : TEXCOORD;     // UV
    float3 WorldPos : WORLD_POS;    // ワールド座標での位置。カメラへの向きを求めるのに使う
    float3 Normal   : NORMAL;       // ワールド座標での法線
};

// b0 : 変換行列。App.h の Transform と同じ並び
cbuffer Transform : register(b0)
{
    float4x4 World         : packoffset(c0);    // ワールド行列
    float4x4 View          : packoffset(c4);    // ビュー行列
    float4x4 Proj          : packoffset(c8);    // 射影行列
    float4x4 WorldInvTrans : packoffset(c12);   // ワールド行列の逆行列の転置。法線の変換に使う
}

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;

    // 位置 : ローカル → ワールド → ビュー → 射影
    float4 localPos = float4(input.Position, 1.0f);
    float4 worldPos = mul(World, localPos);
    float4 viewPos  = mul(View, worldPos);
    float4 projPos  = mul(Proj, viewPos);

    // 法線 : ワールド行列そのままではなく「逆行列の転置」で変換する
    //        拡大率がXYZでバラバラだと、そのまま変換した法線は面に垂直でなくなるため
    //        w = 0 にして平行移動の影響を受けないようにする
    float3 worldNormal = mul(WorldInvTrans, float4(input.Normal, 0.0f)).xyz;

    output.Position = projPos;
    output.TexCoord = input.TexCoord;
    output.WorldPos = worldPos.xyz;
    output.Normal   = normalize(worldNormal);

    return output;
}
