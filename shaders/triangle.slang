#ifndef DEBUG_PRINTF
#define DEBUG_PRINTF 0
#endif

struct Vertex
{
  float3 pos;
  float2 uv;
};

struct VSOutput
{
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

struct PushConstants
{
    Vertex* vertex_ptr;
    float aspect;
    float4x4 viewProj;

    uint texture_id;
    uint sampler_id;
};

[[vk::binding(0,0)]]
Texture2D textures[];

[[vk::binding(1,0)]]
SamplerState samplers[];

[[vk::push_constant]]
PushConstants push;

[shader("vertex")]
VSOutput vs_main(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID)
{
    VSOutput o;

    Vertex v = push.vertex_ptr[vertexID];

#if DEBUG_PRINTF
    printf("vertexID=%u pos=(%f,%f,%f)\n",
        vertexID,
        v.pos.x, v.pos.y, v.pos.z);
#endif

    o.position = mul(push.viewProj, float4(v.pos,1));


o.uv = v.uv;

#if DEBUG_PRINTF
    printf("clip=(%f,%f,%f,%f)\n",
        o.position.x,
        o.position.y,
        o.position.z,
        o.position.w);
#endif

    return o;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target0
{
// #if DEBUG_PRINTF
//     printf("uv=(%f,%f) tex=%u samp=%u\n",
//         input.uv.x,
//         input.uv.y,
//         push.texture_id,
//         push.sampler_id);
// #endif

    Texture2D tex = textures[push.texture_id];
    SamplerState samp = samplers[push.sampler_id];

    return tex.Sample(samp, input.uv);
}




