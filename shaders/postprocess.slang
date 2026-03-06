struct PostPush
{
  uint src_texture_id;
  uint output_image_id;

  uint width;
  uint height;

  float exposure;
  float gamma;

  uint frame;
};

[[vk::push_constant]]
PostPush push;


[[vk::binding(0,0)]]
Texture2D textures[];


[[vk::binding(2,0)]]
RWTexture2D<float4> storage_images[];

[numthreads(16, 16, 1)]
void cs_main(uint3 id: SV_DispatchThreadID)
{
  if(id.x >= push.width || id.y >= push.height)
    return;

  int2 pix = int2(id.xy);

  float2 uv    = (float2(pix) + 0.5) / float2(push.width, push.height);
  float2 texel = 1.0 / float2(push.width, push.height);

  Texture2D           src = textures[push.src_texture_id];
  RWTexture2D<float4> dst = storage_images[push.output_image_id];

  float3 color = src.Load(int3(pix, 0)).rgb;

  // exposure
  color *= push.exposure;

  // ACES tonemap
  float a = 2.51;
  float b = 0.03;
  float c = 2.43;
  float d = 0.59;
  float e = 0.14;
  color   = saturate((color * (a * color + b)) / (color * (c * color + d) + e));

  // vignette
  float2 p    = uv - 0.5;
  float  vign = smoothstep(0.8, 0.2, length(p));
  color *= vign;

  // grain
  float noise = frac(sin(dot(float2(id.xy), float2(12.9898, 78.233))) * 43758.5453);
  color += (noise - 0.5) * 0.02;

  // gamma
  color = pow(color, 1.0 / push.gamma);

  dst[pix] = float4(color, 1);
}
