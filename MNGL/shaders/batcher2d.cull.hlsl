// Keep these register spaces aligned with SDL's DXIL compute binding convention.
cbuffer CullingUniforms : register(b0, space2)
{
    column_major float4x4 view_projection;
    uint block_count;
    uint3 padding;
};

struct GeometryBlock
{
    float2 bounds_min;
    float2 bounds_max;
    float2 transform_x;
    float2 transform_y;
    float2 transform_translation;
    uint first_vertex;
    uint vertex_count;
};

struct DrawCommand
{
    uint num_vertices;
    uint num_instances;
    uint first_vertex;
    uint first_instance;
};

StructuredBuffer<GeometryBlock> culling_blocks : register(t0, space0);
RWStructuredBuffer<DrawCommand> draw_commands : register(u0, space1);

[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    const uint block_index = dispatch_id.x;
    if (block_index >= block_count)
    {
        return;
    }

    const GeometryBlock block = culling_blocks[block_index];
    const float2 corners[4] =
    {
        block.bounds_min,
        float2(block.bounds_max.x, block.bounds_min.y),
        block.bounds_max,
        float2(block.bounds_min.x, block.bounds_max.y)
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
        const float2 world = block.transform_x * local.x +
                             block.transform_y * local.y +
                             block.transform_translation;
        const float4 clip = mul(view_projection, float4(world, 0.0f, 1.0f));

        // Match the CPU path's conservative handling of points behind the clip plane.
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

    const bool visible = uncertain ||
                         !(outside_left || outside_right || outside_bottom || outside_top);
    DrawCommand command;
    command.num_vertices = visible ? block.vertex_count : 0;
    command.num_instances = visible ? 1 : 0;
    command.first_vertex = block.first_vertex;
    command.first_instance = 0;
    draw_commands[block_index] = command;
}
