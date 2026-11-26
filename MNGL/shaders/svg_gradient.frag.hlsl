struct GradientStop
{
    float4 color;
    float4 parameters;
};

StructuredBuffer<GradientStop> gradient_stops : register(t0, space2);

cbuffer GradientUniforms : register(b0, space3)
{
    float4 coordinate_row_x;
    float4 coordinate_row_y;
    float4 gradient_parameters0;
    float4 gradient_parameters1;
    uint4 gradient_control;
};

struct FragmentInput
{
    float2 uv : TEXCOORD0;
    float4 tint : TEXCOORD1;
    float2 local_uv : TEXCOORD2;
    float2 effect_parameters : TEXCOORD3;
};

float apply_spread(float value, uint spread_method)
{
    if (spread_method == 1)
    {
        return value - floor(value);
    }
    if (spread_method == 2)
    {
        const float mirrored = value - floor(value * 0.5f) * 2.0f;
        return 1.0f - abs(mirrored - 1.0f);
    }
    return saturate(value);
}

float linear_parameter(float2 position)
{
    const float2 start = gradient_parameters0.xy;
    const float2 direction = gradient_parameters0.zw - start;
    const float direction_scale = max(abs(direction.x), abs(direction.y));
    if (direction_scale == 0.0f)
    {
        return 1.0f;
    }
    const float2 normalized_direction = direction / direction_scale;
    const float2 normalized_position = (position - start) / direction_scale;
    return dot(normalized_position, normalized_direction) / dot(normalized_direction, normalized_direction);
}

bool linear_degenerate()
{
    const float2 direction = gradient_parameters0.zw - gradient_parameters0.xy;
    return all(direction == 0.0f);
}

float radial_parameter(float2 position)
{
    const float2 center = gradient_parameters0.xy;
    const float radius = max(gradient_parameters0.z, 0.0f);
    const float focal_radius = clamp(gradient_parameters1.z, 0.0f, radius);
    const float coordinate_scale = max(radius, length(center - gradient_parameters1.xy));
    float2 focus = (gradient_parameters1.xy - center) / coordinate_scale;
    const float2 normalized_position = (position - center) / coordinate_scale;
    const float normalized_radius = radius / coordinate_scale;
    const float normalized_focal_radius = focal_radius / coordinate_scale;
    const float2 center_to_focus = -focus;
    const float center_distance = length(center_to_focus);
    const float radius_delta = normalized_radius - normalized_focal_radius;
    if (center_distance > radius_delta && center_distance > 0.0f)
    {
        focus = -center_to_focus * (radius_delta / center_distance);
    }

    const float2 circle_delta = -focus;
    const float2 point_delta = normalized_position - focus;
    const float a = dot(circle_delta, circle_delta) - radius_delta * radius_delta;
    const float b = -2.0f * dot(point_delta, circle_delta) - 2.0f * normalized_focal_radius * radius_delta;
    const float c = dot(point_delta, point_delta) - normalized_focal_radius * normalized_focal_radius;
    if (c <= 0.0f)
    {
        return 0.0f;
    }
    if (abs(a) <= 1.0e-7f)
    {
        return abs(b) > 1.0e-7f ? -c / b : 1.0f;
    }

    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f)
    {
        return 1.0f;
    }
    return (-b - sqrt(discriminant)) / (2.0f * a);
}

bool radial_degenerate()
{
    return max(gradient_parameters0.z, 0.0f) == 0.0f;
}

float3 srgb_to_linear(float3 color)
{
    const float3 low = color / 12.92f;
    const float3 high = pow((color + 0.055f) / 1.055f, 2.4f);
    return lerp(high, low, step(color, 0.04045f));
}

float3 linear_to_srgb(float3 color)
{
    const float3 low = color * 12.92f;
    const float3 high = 1.055f * pow(max(color, 0.0f), 1.0f / 2.4f) - 0.055f;
    return lerp(high, low, step(color, 0.0031308f));
}

float4 interpolate_gradient_colors(float4 first, float4 second, float weight)
{
    if (gradient_control.w == 0u)
    {
        return lerp(first, second, weight);
    }
    float4 result = lerp(float4(srgb_to_linear(first.rgb), first.a),
                         float4(srgb_to_linear(second.rgb), second.a), weight);
    result.rgb = linear_to_srgb(result.rgb);
    return result;
}

float4 sample_gradient(float value)
{
    const uint stop_count = max(gradient_control.z, 1u);
    uint first_greater = 0;
    uint search_end = stop_count;
    [loop]
    while (first_greater < search_end)
    {
        const uint middle = first_greater + (search_end - first_greater) / 2;
        if (value < gradient_stops[middle].parameters.x)
        {
            search_end = middle;
        }
        else
        {
            // Advance past equal offsets so hard transitions select their final stop.
            first_greater = middle + 1;
        }
    }

    if (first_greater == 0)
    {
        return gradient_stops[0].color;
    }
    if (first_greater >= stop_count)
    {
        return gradient_stops[stop_count - 1].color;
    }

    const uint previous_index = first_greater - 1;
    const float previous_offset = gradient_stops[previous_index].parameters.x;
    if (value == previous_offset)
    {
        return gradient_stops[previous_index].color;
    }
    const float next_offset = gradient_stops[first_greater].parameters.x;
    const float span = next_offset - previous_offset;
    const float weight = span > 0.0f ? saturate((value - previous_offset) / span) : 0.0f;
    return interpolate_gradient_colors(gradient_stops[previous_index].color,
                                       gradient_stops[first_greater].color, weight);
}

float4 main(FragmentInput input) : SV_Target0
{
	// Keep the full varying signature aligned with batcher2d.vert.hlsl after DXIL packing.
	// Geometry tagged for culling is sent off-screen by the vertex shader, so this guard
	// only discards invalid interpolants and has no effect on visible gradients.
	if (any(isnan(input.uv)) || any(isnan(input.effect_parameters)))
		discard;

    const float2 position = float2(dot(coordinate_row_x.xyz, float3(input.local_uv, 1.0f)),
                                   dot(coordinate_row_y.xyz, float3(input.local_uv, 1.0f)));
    const bool degenerate = gradient_control.x == 0 ? linear_degenerate() : radial_degenerate();
    if (degenerate)
    {
        return sample_gradient(1.0f) * input.tint;
    }
    float parameter = gradient_control.x == 0 ? linear_parameter(position) : radial_parameter(position);
    parameter = apply_spread(parameter, gradient_control.y);
    return sample_gradient(parameter) * input.tint;
}
