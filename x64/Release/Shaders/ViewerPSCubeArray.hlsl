// ============================================================
// ViewerPSCubeArray.hlsl
// Buffer viewer pixel shader for a TextureCubeArray (shadow maps).
// Picks one cube by index, unwraps it (uv -> direction), and shows
// the stored distance as grayscale.
// Author: Chan WingChung
// ============================================================

TextureCubeArray g_cubeArray : register(t0);
SamplerState g_sampler : register(s0);

cbuffer CB_VIEWER : register(b0)
{
    int cubeIndex;
    float _pad0, _pad1, _pad2;
};

struct VS_OUTPUT_VIEW
{
    float4 svPosition : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VS_OUTPUT_VIEW input) : SV_TARGET
{
    // --- uv -> spherical direction (equirectangular unwrap)
    float lPhi = input.uv.x * 6.2831853f - 3.14159265f;
    float lTheta = input.uv.y * 3.14159265f;
    float3 lDir = float3(sin(lTheta) * cos(lPhi),
                           cos(lTheta),
                           sin(lTheta) * sin(lPhi));

    // --- 4th component = which cube in the array
    float lDist = g_cubeArray.SampleLevel(g_sampler, float4(lDir, (float) cubeIndex), 0).r;

    // --- stored distance is already 0..1 -> grayscale
    return float4(lDist, lDist, lDist, 1.0f);
}