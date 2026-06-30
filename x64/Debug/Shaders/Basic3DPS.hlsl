// ============================================================
// Basic3DPS.hlsl
// Pixel shader for unlit / basic 3D rendering
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

float4 main(VS_OUTPUT_SIMPLE input) : SV_TARGET
{
    float3 lEdge = ApplyDissolve(input.uv);
    float4 lColor = useTexture
        ? g_texture.Sample(g_sampler, input.uv)
        : materialColor;
    return float4(lColor.rgb + lEdge, lColor.a);
}