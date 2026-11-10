cbuffer InstanceCullingUniforms : register(b0, space2)
{
    column_major float4x4 view_projection;
    uint instance_count;
    uint range_count;
    uint2 padding;
};

struct InstanceRange
{
    uint first_instance;
    uint instance_count;
    float2 bounds_min;
    float2 bounds_max;
    uint bypass;
};

StructuredBuffer<InstanceRange> ranges : register(t0, space0);
RWStructuredBuffer<uint> instance_words : register(u0, space1);

[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    const uint instance_index = dispatch_id.x;
    if (instance_index >= instance_count)
    {
        return;
    }

    uint low = 0;
    uint high = range_count;
    while (low < high)
    {
        const uint middle = low + (high - low) / 2;
        if (ranges[middle].first_instance <= instance_index)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }

    if (low == 0)
    {
        return;
    }

    const InstanceRange range = ranges[low - 1];
    if (instance_index >= range.first_instance + range.instance_count || range.bypass != 0)
    {
        return;
    }

    const uint instance_word = instance_index * 13;
    const float2 transform_x = asfloat(uint2(instance_words[instance_word], instance_words[instance_word + 1]));
    const float2 transform_y = asfloat(uint2(instance_words[instance_word + 2], instance_words[instance_word + 3]));
    const float2 transform_translation =
        asfloat(uint2(instance_words[instance_word + 4], instance_words[instance_word + 5]));
    const float2 corners[4] =
    {
        range.bounds_min,
        float2(range.bounds_max.x, range.bounds_min.y),
        range.bounds_max,
        float2(range.bounds_min.x, range.bounds_max.y)
    };

    bool outside_left = true;
    bool outside_right = true;
    bool outside_bottom = true;
    bool outside_top = true;
    bool uncertain = false;

    [unroll]
    for (uint corner_index = 0; corner_index < 4; ++corner_index)
    {
        const float2 local = corners[corner_index];
        const float2 world = transform_x * local.x + transform_y * local.y + transform_translation;
        const float4 clip = mul(view_projection, float4(world, 0.0f, 1.0f));
        if (clip.w <= 0.0f)
        {
            uncertain = true;
            break;
        }

        outside_left = outside_left && clip.x < -clip.w;
        outside_right = outside_right && clip.x > clip.w;
        outside_bottom = outside_bottom && clip.y < -clip.w;
        outside_top = outside_top && clip.y > clip.w;
    }

    const bool visible = uncertain || !(outside_left || outside_right || outside_bottom || outside_top);
    if (!visible)
    {
        instance_words[instance_word + 7] = 0x7FC00000;
    }
}
