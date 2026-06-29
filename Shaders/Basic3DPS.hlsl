// ============================================================
// Basic3DPS.hlsl
// Pixel shader for unlit / basic 3D rendering
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

float4 main(VS_OUTPUT_SIMPLE input) : SV_TARGET
{
    if (useTexture)
        return g_texture.Sample(g_sampler, input.uv);
    else
        return materialColor;
}