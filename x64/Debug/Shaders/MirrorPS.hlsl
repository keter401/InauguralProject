// ============================================================
// MirrorPS.hlsl
// Pixel shader for planar mirror (samples reflection RT)
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

float4 main(PS_INPUT_PHONG input) : SV_TARGET
{
    if (useTexture == 1)
    {
        // --- Flip X to convert reflection RT into mirror-space UV
        float2 lScreenUv = input.svPosition.xy / screenSize;
        lScreenUv.x = 1.0f - lScreenUv.x;

        float4 lReflectionColor = g_texture.Sample(g_sampler, lScreenUv);
        return lReflectionColor;
    }
    else
    {
        return materialColor;
    }
}