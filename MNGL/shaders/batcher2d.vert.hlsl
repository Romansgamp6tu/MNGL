cbuffer FrameUniforms : register(b0, space1)
{
    column_major float4x4 view_projection;
};

struct VertexInput
{
    float2 corner : TEXCOORD0;
    float2 destination_position : TEXCOORD1;
    float2 destination_size : TEXCOORD2;
    float2 pivot : TEXCOORD3;
    float2 rotation : TEXCOORD4;
    float2 effect_parameters : TEXCOORD5;
    float4 uv_bounds : TEXCOORD6;
    float4 tint : TEXCOORD7;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 tint : TEXCOORD1;
    float2 local_uv : TEXCOORD2;
    float2 effect_parameters : TEXCOORD3;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    const float2 local_position =
        (input.corner - input.pivot) * input.destination_size;
    const float2 rotated_position = float2(
        input.rotation.x * local_position.x - input.rotation.y * local_position.y,
        input.rotation.y * local_position.x + input.rotation.x * local_position.y);
    const float2 world_position =
        input.destination_position + input.pivot * input.destination_size + rotated_position;

    output.position = mul(view_projection, float4(world_position, 0.0f, 1.0f));
    output.uv = lerp(input.uv_bounds.xy, input.uv_bounds.zw, input.corner);
    output.tint = input.tint;
    output.local_uv = input.corner;
    output.effect_parameters = input.effect_parameters;
    return output;
}
