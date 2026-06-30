// ============================================================
// DissolvePS.hlsl
// Pixel shader for dissolve effect (noise clip + edge glow)
// Author: Chan WingChung
// ============================================================
#include "LightCommon.hlsli"

float4 main(PS_INPUT_PHONG input) : SV_TARGET
{
    // --- Dissolve clip by procedural noise
    float lNoise = SmoothNoise(input.uv);
    clip(lNoise - dissolveThreshold);

    float lEdgeFactor = 1.0f - saturate((lNoise - dissolveThreshold) / max(edgeWidth, 0.0001f));
    float3 lEdge = edgeColor.rgb * lEdgeFactor;

    float3 lN = normalize(input.worldNormal);
    float3 lV = normalize(cameraPos - input.worldPos);

    float4 lBaseColor = useTexture
        ? g_texture.Sample(g_sampler, input.uv)
        : materialColor;

    float3 lAmbient = lBaseColor.rgb * ambientStrength;
    float3 lLighting = float3(0, 0, 0);

    [loop]
    for (int lI = 0; lI < lightCount; lI++)
        lLighting += CalcPhongContribution(lights[lI], lN, lV, input.worldPos,
                    lBaseColor.rgb, CalcShadowForLight(lI, input.worldPos), shininess);

    float3 lLinearColor = lAmbient + lLighting + lEdge;
    float3 lFinal = GammaCorrect(ToneMapAces(lLinearColor));

    return float4(lFinal, lBaseColor.a);
}