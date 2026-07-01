// ============================================================
// ViewerPS2D.hlsl
// Buffer viewer pixel shader for 2D textures.
// Author: Chan WingChung
// ============================================================

Texture2D g_tex : register(t0);
SamplerState g_sampler : register(s0);

struct VS_OUTPUT_VIEW
{
    float4 svPosition : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VS_OUTPUT_VIEW input) : SV_TARGET
{
    float3 lColor = g_tex.Sample(g_sampler, input.uv).rgb;
    return float4(lColor, 1.0f);
}