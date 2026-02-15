struct VSOut
{
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

[shader("vertex")]
VSOut vs_main(uint vertexID : SV_VertexID)
{
    float2 pos[3] =
    {
        float2( 0.0, -0.5),
        float2( 0.5,  0.5),
        float2(-0.5,  0.5)
    };

    float3 col[3] =
    {
        float3(1,0,0),
        float3(0,1,0),
        float3(0,0,1)
    };

    VSOut o;
    o.position = float4(pos[vertexID], 0, 1);
    o.color = col[vertexID];
    return o;
}

[shader("fragment")]
float4 fs_main(VSOut input) : SV_Target0
{
    return float4(input.color, 1);
}
