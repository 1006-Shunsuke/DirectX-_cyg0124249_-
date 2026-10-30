struct VSInput
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

cbuffer Transform : register(b0)
{
    float4x4 World : packoffset(c0);//ワールド行列
    float4x4 View : packoffset(c4);//ビュー行列
    float4x4 Proj : packoffset(c8);//射影行列

}
VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    
    float4 localPos = float4(input.Position, 1.0f);// ローカル座標：オブジェクトの中心が原点
    float4 worldPos = mul(World, localPos);// ワールド座標：世界のどこにいるか
    float4 viewPos = mul(View, worldPos);//カメラの位置を原点とした座標
    float4 projPos = mul(Proj, viewPos);//
    
    output.Position = projPos;
    output.TexCoord = input.TexCoord;
    
    return output;
}