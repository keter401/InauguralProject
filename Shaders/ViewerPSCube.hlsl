// ============================================================
// ViewerPSCube.hlsl
// Buffer viewer pixel shader for cube maps.
// Unwraps the cube to an equirectangular view (uv -> direction),
// with a light Reinhard tonemap since IBL maps are HDR.
// Author: Chan WingChung
// ============================================================

TextureCube g_cube : register(t0);
SamplerState g_sampler : register(s0);

struct VS_OUTPUT_VIEW
{
    float4 svPosition : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VS_OUTPUT_VIEW input) : SV_TARGET
{
    // --- uv -> spherical sampling direction (equirectangular unwrap)
    float lPhi = input.uv.x * 6.2831853f - 3.14159265f; // longitude
    float lTheta = input.uv.y * 3.14159265f; // latitude 0..PI
    float3 lDir = float3(sin(lTheta) * cos(lPhi),
                           cos(lTheta),
                           sin(lTheta) * sin(lPhi));

    float3 lColor = g_cube.SampleLevel(g_sampler, lDir, 0).rgb;

    // --- HDR なので簡易トーンマップして見やすく
    lColor = lColor / (lColor + 1.0f);
    return float4(lColor, 1.0f);
}