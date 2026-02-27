// Example shader demonstrating Debug Printf usage in Slang
// To enable Debug Printf:
// 1. Set enable_debug_printf = true in RendererDesc
// 2. Use standard printf() calls in your shader code
// 3. View output in RenderDoc (1.14+) or Validation Layers

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

    // Debug Printf examples:
    // Basic float printing
    printf("Vertex %u: position = (%f, %f)\n", vertexID, v.pos.x, v.pos.y);
    
    // Vector printing (note: Slang/HLSL doesn't support %vNf format, 
    // so print components individually)
    printf("Vertex %u: color = (%f, %f, %f)\n", vertexID, v.color.r, v.color.g, v.color.b);
    
    // Hexadecimal output
    printf("VertexID as hex: 0x%x\n", vertexID);

    return o;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target0
{
    // Fragment shader debug printf example
    // Warning: This will print for EVERY fragment, which can be millions of messages!
    // Uncomment only for specific debugging needs:
    // printf("Fragment color: (%1.2f, %1.2f, %1.2f)\n", input.color.r, input.color.g, input.color.b);
    
    return float4(input.color, 1);
}
