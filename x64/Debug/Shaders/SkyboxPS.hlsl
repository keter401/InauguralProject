// ============================================================
// SkyboxPS.hlsl
// Pixel shader for skybox (equirectangular HDR sampling)
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

float4 main(PS_INPUT_NORMAL_MAP input) : SV_TARGET
{
    float3 lDir = normalize(input.worldPos - cameraPos);

    // --- Direction -> equirectangular UV
    float lTheta = atan2(lDir.z, lDir.x);
    float lPhi = asin(clamp(lDir.y, -1.0f, 1.0f));
    float lU = (lTheta / (2.0f * PI)) + 0.5f;
    float lV = (lPhi / PI) + 0.5f;

    float3 lColor = g_texture.Sample(g_sampler, float2(lU, lV)).rgb;
    lColor = GammaCorrect(ToneMapAces(lColor));

    return float4(lColor, 1.0f);
}