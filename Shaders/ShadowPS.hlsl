// ============================================================
// ShadowPS.hlsl
// Pixel shader for shadow-map generation
// (writes normalized world distance, supports dissolve clip)
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

// --- Shadow-gen constants (only this shader uses them)
cbuffer CB_SHADOW_GEN : register(b7)
{
    float3 genLightPos;
    float genFarZ;
};

float main(VS_OUTPUT_SHADOW input) : SV_TARGET
{
    // --- Dissolve clip so dissolving objects also drop matching shadows
    float lNoise = SmoothNoise(input.uv);
    clip(lNoise - dissolveThreshold);

    float lDist = length(input.worldPos - genLightPos);
    return saturate(lDist / genFarZ);
}