struct VSInput
{
    float3 Position : POSITION;//位置座標(x,y,z)
    float4 Color : COLOR;//頂点カラー
};
struct VSOutput
{
    float4 Position:SV_POSITION;//位置座標
    float4 Color : COLOR;//頂点カラー
};
//定数バッファ
cbuffer Transform : register(b0)//Transformという名前の定数バッファに0を割り当てる
{
    //それぞれ行列変換
    float4x4 World:packoffset(c0);
    float4x4 View:packoffset(c4);
    float4x4 Proj:packoffset(c8);
}
VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    
    float4 localPos = float4(input.Position, 1.0f);
    float4 worldPos = mul(World,localPos);
    float4 viewPos = mul(View, worldPos);
    float4 projPos = mul(Proj, viewPos);
    
    output.Position = projPos;
    output.Color = input.Color;
    
    return output;
}