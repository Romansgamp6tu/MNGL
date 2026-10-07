Texture2D<float4> sprite_texture : register(t0, space2);
SamplerState sprite_sampler : register(s0, space2);

cbuffer MaterialUniforms : register(b0, space3)
{
    uint4 material_data;
};

struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 tint : TEXCOORD1;
    float2 local_uv : TEXCOORD2;
    float2 effect_parameters : TEXCOORD3;
};

float4 main(FragmentInput input) : SV_Target0
{
    float4 sample_color = sprite_texture.Sample(sprite_sampler, input.uv);
    float coverage = 1.0f;
    const float2 centered_uv = input.local_uv * 2.0f - 1.0f;
    if (material_data.x == 1 || material_data.x == 2)
    {
        const float distance_from_center = length(centered_uv) - 1.0f;
        const float edge_width = max(fwidth(distance_from_center), 0.0001f);
        coverage = 1.0f - smoothstep(-edge_width, edge_width, distance_from_center);
    }
    else if (material_data.x == 3)
    {
        const float radius = saturate(input.effect_parameters.x);
        const float2 box_distance = abs(centered_uv) - (1.0f - radius);
        const float distance_from_edge =
            length(max(box_distance, 0.0f)) +
            min(max(box_distance.x, box_distance.y), 0.0f) -
            radius;
        const float edge_width = max(fwidth(distance_from_edge), 0.0001f);
        coverage = 1.0f - smoothstep(-edge_width, edge_width, distance_from_edge);
    }
    sample_color.a *= coverage;
    return sample_color * input.tint;
}
