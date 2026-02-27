struct Vertex
{
    float2 pos;
    float3 color;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

struct PushConstants
{
    Vertex* vertexBuffer;
};

[[vk::push_constant]]
PushConstants push;

[shader("vertex")]
VSOutput vs_main(uint vertexID : SV_VertexID)
{
    VSOutput o;

    Vertex v = push.vertexBuffer[vertexID];

    o.position = float4(v.pos, 0, 1);
    o.color = v.color;

    return o;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target0
{
    return float4(input.color, 1);
}
