cbuffer MaterialUniforms : register(b0, space3)
{
    float4 fill_color;
};

struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 tint : TEXCOORD1;
    float2 local_uv : TEXCOORD2;
};

float4 main(FragmentInput input) : SV_Target0
{
    return fill_color * input.tint;
}
