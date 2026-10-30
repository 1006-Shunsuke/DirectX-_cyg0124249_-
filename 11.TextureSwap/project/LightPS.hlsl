//-----------------------------------------------------------------------------
// File : LightPS.hlsl
// Desc : ライティング用のピクセルシェーダ
//        平行光源1つで 環境光 + Lambert 拡散反射 + Blinn-Phong 鏡面反射 を計算する
//-----------------------------------------------------------------------------

// 頂点シェーダからの入力。LightVS.hlsl の VSOutput と同じ
struct VSOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
    float3 WorldPos : WORLD_POS;
    float3 Normal   : NORMAL;
};

struct PSOutput
{
    float4 Color : SV_TARGET0;
};

// b1 : ライトの設定。App.h の LightParam と同じ並びで、float3 + float で16バイトずつ
cbuffer LightBuffer : register(b1)
{
    float3 LightDir          : packoffset(c0);   // 光が進む向き。正規化済み
    float  LightIntensity    : packoffset(c0.w); // 光の強さ
    float3 LightColor        : packoffset(c1);   // 光の色
    float  Shininess         : packoffset(c1.w); // 鏡面反射の鋭さ。大きいほどハイライトが小さく鋭い
    float3 CameraPos         : packoffset(c2);   // カメラの位置
    float  SpecularIntensity : packoffset(c2.w); // 鏡面反射の強さ
    float3 AmbientColor      : packoffset(c3);   // 環境光の色。光が当たらない面の明るさ
}

SamplerState ColorSmp : register(s0);
Texture2D    ColorMap : register(t0);

PSOutput main(VSOutput input)
{
    PSOutput output = (PSOutput) 0;

    // 1. 計算に使う向き。すべて長さ1にそろえる
    float3 N = normalize(input.Normal);                     // 法線。補間で長さが1でなくなるので正規化し直す
    float3 L = normalize(-LightDir);                        // 表面から「光が来る方向」へ向かう向き
    float3 V = normalize(CameraPos - input.WorldPos);       // 表面から「カメラ」へ向かう向き
    float3 H = normalize(L + V);                            // L と V の真ん中の向き。ハーフベクトル

    // 2. テクスチャから表面の色を取る
    float4 albedo = ColorMap.Sample(ColorSmp, input.TexCoord);

    // 3. Lambert 拡散反射 : 光に正面を向くほど明るい。内積が負になる裏側は0にする
    float  NdotL   = saturate(dot(N, L));
    float3 diffuse = albedo.rgb * LightColor * LightIntensity * NdotL;

    // 4. Blinn-Phong 鏡面反射 : 法線がハーフベクトルに近いほど強いハイライト
    //    NdotL = 0 の光が当たっていない面にはハイライトを出さない
    float  NdotH    = saturate(dot(N, H));
    float  spec     = pow(NdotH, Shininess) * step(0.0001f, NdotL);
    float3 specular = LightColor * LightIntensity * SpecularIntensity * spec;

    // 5. 環境光 : 光が当たらない面が真っ黒にならないように、少しだけ足す
    float3 ambient = albedo.rgb * AmbientColor;

    // 6. 合計
    output.Color = float4(ambient + diffuse + specular, albedo.a);

    return output;
}
