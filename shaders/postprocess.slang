import tonemapping;

//===========================================================
// Push constant layout — must match C side exactly
//===========================================================
struct PostPush
{
  // resource IDs
  uint src_texture_id;
  uint output_image_id;
  uint sampler_id;

  // resolution
  uint width;
  uint height;

  // effect enable flags (bitfield)
  uint flags;

  // tonemap selection: 0=neutral 1=aces 2=reinhard 3=reinhard2 4=filmic 5=uncharted2 6=unreal
  uint tonemap_mode;

  // core
  float exposure;
  float gamma_value;

  // color grading
  float contrast;
  float saturation;
  float temperature;  // white-balance: negative=cool, positive=warm
  float tint;         // white-balance: green-magenta axis

  // spatial
  float vignette_intensity;
  float vignette_smoothness;
  float sharpen_strength;
  float ca_strength;  // chromatic-aberration

  // noise / output
  float grain_intensity;
  float sepia_intensity;
  float dither_strength;
  uint  frame;

  // lift / gain (shadows / highlights color wheels)
  float lift_r;
  float lift_g;
  float lift_b;
  float gain_r;
  float gain_g;
  float gain_b;
};

//===========================================================
// Flag bits
//===========================================================
static const uint FLAG_EXPOSURE      = (1u << 0);
static const uint FLAG_TONEMAP       = (1u << 1);
static const uint FLAG_GAMMA         = (1u << 2);
static const uint FLAG_CONTRAST      = (1u << 3);
static const uint FLAG_SATURATION    = (1u << 4);
static const uint FLAG_WHITE_BALANCE = (1u << 5);
static const uint FLAG_VIGNETTE      = (1u << 6);
static const uint FLAG_GRAIN         = (1u << 7);
static const uint FLAG_SHARPEN       = (1u << 8);
static const uint FLAG_CA            = (1u << 9);
static const uint FLAG_DITHER        = (1u << 10);
static const uint FLAG_SEPIA         = (1u << 11);
static const uint FLAG_LIFT_GAIN     = (1u << 12);

bool has_flag(uint flags, uint bit)
{
  return (flags & bit) != 0;
}

//===========================================================
// Bindless resources
//===========================================================
[[vk::push_constant]]
PostPush push;

[[vk::binding(0, 0)]]
Texture2D textures[];

[[vk::binding(1, 0)]]
SamplerState samplers[];

[[vk::binding(2, 0)]]
RWTexture2D<float4> storage_images[];

//===========================================================
// Helper: white balance via simplified LMS transform
//===========================================================
float3 white_balance(float3 color, float temp, float tnt)
{
  float t1 = temp * 0.0553;
  float t2 = tnt * 0.0553;
  color.r *= 1.0 + t1;
  color.b *= 1.0 - t1;
  color.g *= 1.0 + t2;
  return max(color, 0.0);
}

//===========================================================
// Contrast (around midpoint 0.5 for LDR)
//===========================================================
float3 apply_contrast(float3 color, float contrast)
{
  const float mid = 0.5;
  return (color - mid) * contrast + mid;
}

//===========================================================
// Saturation
//===========================================================
float3 apply_saturation(float3 color, float saturation)
{
  float luma = dot(color, float3(0.2126, 0.7152, 0.0722));
  return lerp(float3(luma), color, saturation);
}

//===========================================================
// Lift / Gain  (shadows / highlights color adjustment)
//===========================================================
float3 apply_lift_gain(float3 color, float3 lift, float3 gain)
{
  return max(color * gain + lift, 0.0);
}

//===========================================================
// Sepia
//===========================================================
float3 apply_sepia(float3 color, float intensity)
{
  float3 sepia;
  sepia.r = dot(color, float3(0.393, 0.769, 0.189));
  sepia.g = dot(color, float3(0.349, 0.686, 0.168));
  sepia.b = dot(color, float3(0.272, 0.534, 0.131));
  return lerp(color, sepia, intensity);
}

//===========================================================
// Vignette
//===========================================================
float3 apply_vignette(float3 color, float2 uv, float intensity, float smoothness)
{
  float2 d        = uv - 0.5;
  float  r2       = dot(d, d);  // 0 at center, ~0.5 at corners
  float  vignette = 1.0 - r2 * intensity;
  vignette        = clamp(vignette, 0.0, 1.0);
  vignette        = pow(vignette, smoothness);
  return color * vignette;
}

//===========================================================
// Film grain (hash-based, temporal)
//===========================================================
float hash_grain(float2 p, float t)
{
  float3 p3 = frac(float3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33 + t);
  return frac((p3.x + p3.y) * p3.z);
}

float3 apply_film_grain(float3 color, float2 uv, float time, float intensity)
{
  float noise        = hash_grain(uv * 1000.0, time * 0.01);
  float luma         = dot(color, float3(0.2126, 0.7152, 0.0722));
  float grain_weight = 1.0 - abs(luma * 2.0 - 1.0);
  color += (noise - 0.5) * intensity * grain_weight;
  return max(color, 0.0);
}

//===========================================================
// 3x3 Unsharp-mask sharpen (reads from unmodified source)
//===========================================================
float3 apply_sharpen(Texture2D src, SamplerState samp, float2 uv, float2 texel, float strength, float3 center)
{
  float3 neighbors = float3(0);
  neighbors += src.SampleLevel(samp, uv + float2(-texel.x, 0), 0).rgb;
  neighbors += src.SampleLevel(samp, uv + float2(texel.x, 0), 0).rgb;
  neighbors += src.SampleLevel(samp, uv + float2(0, -texel.y), 0).rgb;
  neighbors += src.SampleLevel(samp, uv + float2(0, texel.y), 0).rgb;
  neighbors *= 0.25;
  return max(center + (center - neighbors) * strength, 0.0);
}

//===========================================================
// Chromatic aberration (radial UV offset per channel)
//===========================================================
float3 apply_chromatic_aberration(Texture2D src, SamplerState samp, float2 uv, float2 resolution, float strength)
{
  float2 center_offset = uv - 0.5;
  float2 offset        = center_offset * strength / resolution;

  float r = src.SampleLevel(samp, uv + offset, 0).r;
  float g = src.SampleLevel(samp, uv, 0).g;
  float b = src.SampleLevel(samp, uv - offset, 0).b;
  return float3(r, g, b);
}

//===========================================================
// Interleaved gradient noise dithering (absolute last)
//===========================================================
float interleaved_gradient_noise(float2 p)
{
  float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
  return frac(magic.z * frac(dot(p, magic.xy)));
}

float3 apply_dithering(float3 color, float2 pixel_coord, float strength)
{
  float dither = interleaved_gradient_noise(pixel_coord);
  return color + (dither - 0.5) * strength / 255.0;
}



[numthreads(16, 16, 1)]
void cs_main(uint3 id: SV_DispatchThreadID)
{
  if(id.x >= push.width || id.y >= push.height)
    return;

  int2 pix = int2(id.xy);

  Texture2D           src = textures[push.src_texture_id];
  RWTexture2D<float4> dst = storage_images[push.output_image_id];

  // read HDR color
  float3 color = src.Load(int3(pix, 0)).rgb;

  // exposure
  ///////color *= push.exposure;

  // ACES tonemap
  //color = aces(color);
//color = pow(max(color, 0.0), 1.0 / 2.2);
  // write
  dst[pix] = float4(color, 1.0);
}

