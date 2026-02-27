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

    // Debug Printf: limit output to avoid flooding
    if (vertexID == 0)
    {
        printf("Vertex %u pos=(%f, %f) color=(%f, %f, %f)\n",
               vertexID, v.pos.x, v.pos.y, v.color.r, v.color.g, v.color.b);
    }

    return o;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target0
{
    return float4(input.color, 1);
}
