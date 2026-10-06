Texture2D<float4> storage_texture : register(t0, space2);

struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 tint : TEXCOORD1;
    float2 local_uv : TEXCOORD2;
};

float4 main(FragmentInput input) : SV_Target0
{
    return storage_texture.Load(int3(0, 0, 0)) * input.tint;
}
