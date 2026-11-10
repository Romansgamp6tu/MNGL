cbuffer FrameUniforms : register(b0, space1)
{
    column_major float4x4 view_projection;
};

struct VertexInput
{
    float2 corner : TEXCOORD0;
    float2 transform_x : TEXCOORD1;
    float2 transform_y : TEXCOORD2;
    float2 transform_translation : TEXCOORD3;
    float2 effect_parameters : TEXCOORD4;
    float4 uv_bounds : TEXCOORD5;
    float4 tint : TEXCOORD6;
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
    if (isnan(input.effect_parameters.y))
    {
        output.position = float4(2.0f, 2.0f, 0.0f, 1.0f);
        output.uv = input.uv_bounds.xy;
        output.tint = input.tint;
        output.local_uv = input.corner;
        output.effect_parameters = input.effect_parameters;
        return output;
    }

    const float2 world_xy =
        input.transform_x * input.corner.x +
        input.transform_y * input.corner.y +
        input.transform_translation;
    const float4 world_position = float4(world_xy, 0.0f, 1.0f);

    output.position = mul(view_projection, world_position);
    output.uv = lerp(input.uv_bounds.xy, input.uv_bounds.zw, input.corner);
    output.tint = input.tint;
    output.local_uv = input.corner;
    output.effect_parameters = input.effect_parameters;
    return output;
}
