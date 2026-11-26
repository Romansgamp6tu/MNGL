Texture2D<float4> first_texture : register(t0, space2);
Texture2D<float4> second_texture : register(t1, space2);
SamplerState first_sampler : register(s0, space2);
SamplerState second_sampler : register(s1, space2);

cbuffer FilterUniforms : register(b0, space3)
{
    uint4 control;
    float4 parameters;
    float4 clip_uv;
    float4 rows[5];
};

bool inside(float2 uv, float4 bounds)
{
    return all(uv >= bounds.xy) && all(uv <= bounds.zw);
}

float4 sample_first(float2 uv)
{
    return inside(uv, float4(0, 0, 1, 1)) ? first_texture.SampleLevel(first_sampler, uv, 0) : 0;
}

float4 sample_second(float2 uv)
{
    return inside(uv, float4(0, 0, 1, 1)) ? second_texture.SampleLevel(second_sampler, uv, 0) : 0;
}

int wrap_index(int value, int minimum, int extent)
{
    const int remainder = (value - minimum) % extent;
    return minimum + (remainder < 0 ? remainder + extent : remainder);
}

bool blur_texel_bounds(out int2 minimum, out int2 maximum, out int2 dimensions)
{
    uint width;
    uint height;
    first_texture.GetDimensions(width, height);
    dimensions = int2(width, height);
    if (any(rows[0].xy > rows[0].zw))
        return false;

    const float2 size = float2(width, height);
    minimum = max(int2(ceil(rows[0].xy * size - 0.5)), int2(0, 0));
    maximum = min(int2(floor(rows[0].zw * size - 0.5)), dimensions - 1);
    return all(minimum <= maximum);
}

float4 sample_blur(float2 uv)
{
    int2 minimum;
    int2 maximum;
    int2 dimensions;
    if (!blur_texel_bounds(minimum, maximum, dimensions))
        return 0;

    const int axis = int(control.w);
    const int other_axis = 1 - axis;
    const float2 texel = uv * float2(dimensions) - 0.5;
    const int other_index = int(floor(texel[other_axis] + 0.5));
    if (other_index < minimum[other_axis] || other_index > maximum[other_axis])
        return 0;

    if (control.y == 0)
        return inside(uv, rows[0]) ? first_texture.SampleLevel(first_sampler, uv, 0) : 0;

    if (control.y == 1)
    {
        const float2 lower_center = (float2(minimum) + 0.5) / float2(dimensions);
        const float2 upper_center = (float2(maximum) + 0.5) / float2(dimensions);
        float2 clamped_uv = uv;
        clamped_uv[axis] = clamp(uv[axis], lower_center[axis], upper_center[axis]);
        return first_texture.SampleLevel(first_sampler, clamped_uv, 0);
    }

    const float fraction = frac(texel[axis]);
    const int low = int(floor(texel[axis]));
    const int high = low + 1;
    int2 low_index = int2(floor(texel + 0.5));
    int2 high_index = low_index;
    low_index[axis] = wrap_index(low, minimum[axis], maximum[axis] - minimum[axis] + 1);
    high_index[axis] = wrap_index(high, minimum[axis], maximum[axis] - minimum[axis] + 1);
    const float4 low_sample = first_texture.Load(int3(low_index, 0));
    const float4 high_sample = first_texture.Load(int3(high_index, 0));
    return lerp(low_sample, high_sample, fraction);
}

float3 unpremultiply(float4 value)
{
    return value.a > 0 ? value.rgb / value.a : 0;
}

float3 to_linear(float3 value)
{
    return lerp(value / 12.92, pow((value + 0.055) / 1.055, 2.4), step(0.04045, value));
}

float3 to_srgb(float3 value)
{
    return lerp(value * 12.92, 1.055 * pow(value, 1.0 / 2.4) - 0.055, step(0.0031308, value));
}

float4 over(float4 source, float4 destination)
{
    return source + destination * (1 - source.a);
}

float4 composite(float4 a, float4 b)
{
    switch (control.y)
    {
    case 1: return a * b.a;
    case 2: return a * (1 - b.a);
    case 3: return a * b.a + b * (1 - a.a);
    case 4: return a * (1 - b.a) + b * (1 - a.a);
    case 5:
        {
            const float4 value = saturate(parameters.x * a * b + parameters.y * a +
                                          parameters.z * b + parameters.w);
            return float4(min(value.rgb, value.a), value.a);
        }
    default: return over(a, b);
    }
}

float4 blend(float4 a, float4 b)
{
    if (control.y == 0)
        return over(a, b);
    const float3 source = unpremultiply(a);
    const float3 backdrop = unpremultiply(b);
    float3 mixed = source;
    switch (control.y)
    {
    case 1: mixed = source * backdrop; break;
    case 2: mixed = source + backdrop - source * backdrop; break;
    case 3: mixed = min(source, backdrop); break;
    case 4: mixed = max(source, backdrop); break;
    }
    return float4(a.rgb * (1 - b.a) + b.rgb * (1 - a.a) + mixed * (a.a * b.a),
                  a.a + b.a - a.a * b.a);
}

// Working textures contain premultiplied pixels. Only the final pass adapts to the caller's blend mode.
float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    if (!inside(uv, clip_uv))
        return 0;

    const float4 a = sample_first(uv);
    switch (control.x)
    {
    case 1:
        return float4(0, 0, 0, a.a);
    case 2:
        {
            float4 sum = sample_blur(uv);
            float total = 1;
            [loop]
            for (int i = 1; i <= int(control.z); ++i)
            {
                const float distance = i / parameters.z;
                const float weight = exp(-0.5 * distance * distance);
                const float2 delta = i * parameters.xy;
                sum += weight * (sample_blur(uv - delta) + sample_blur(uv + delta));
                total += 2 * weight;
            }
            return sum / total;
        }
    case 3:
        return sample_first(uv - parameters.xy);
    case 4:
        return float4(parameters.rgb * parameters.a, parameters.a);
    case 5:
        {
            const float4 straight = float4(unpremultiply(a), a.a);
            float4 color = saturate(float4(dot(rows[0], straight), dot(rows[1], straight),
                                          dot(rows[2], straight), dot(rows[3], straight)) + rows[4]);
            return float4(color.rgb * color.a, color.a);
        }
    case 6:
        return composite(a, sample_second(uv));
    case 7:
        return blend(a, sample_second(uv));
    case 8:
        {
            const float alpha = sample_second(uv - parameters.xy).a * rows[0].a;
            return over(a, float4(rows[0].rgb * alpha, alpha));
        }
    case 9:
        {
            const float4 mask = sample_second(uv);
            float weight = mask.a;
            if (control.y == 1)
                weight = dot(mask.rgb, float3(0.2125, 0.7154, 0.0721));
            if (control.y == 2)
                weight = dot(to_linear(unpremultiply(mask)), float3(0.2125, 0.7154, 0.0721)) * mask.a;
            return a * weight;
        }
    case 10:
        return float4(to_linear(unpremultiply(a)) * a.a, a.a);
    case 11:
        return float4(to_srgb(unpremultiply(a)) * a.a, a.a);
    case 12:
        return control.y != 0 ? a : float4(unpremultiply(a), a.a);
    default:
        return a;
    }
}
