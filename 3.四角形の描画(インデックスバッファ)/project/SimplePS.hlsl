struct VSOutput
{
    float4 Position : SV_Position;//位置座標
    float4 Color : COLOR;//頂点カラー
};
struct PSOutput
{
    float4 Color : SV_TARGET0; //ピクセルカラー
};

PSOutput main(VSOutput input)
{
    PSOutput output = (PSOutput)0;
    output.Color = input.Color;
    return output;
}