struct VertexOutput
{
    float2 uv : TEXCOORD0;
    float4 position : SV_Position;
};

VertexOutput main(uint vertex : SV_VertexID)
{
    const float2 uv = float2((vertex & 1) * 2, (vertex >> 1) * 2);
    VertexOutput output;
    output.position = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
    output.uv = uv;
    return output;
}
