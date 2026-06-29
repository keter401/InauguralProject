// ============================================================
// ToonPS.hlsl
// Pixel shader for toon / cel shading
// (3-tone + tone shift + rim)
// Author: Chan WingChung
// ============================================================
#include "LightCommon.hlsli"

// ------------------------------------------------------------
// ApplyToneShift : brighten / darken base color by toon band
// ------------------------------------------------------------
float4 ApplyToneShift(float4 baseColor, float toonFactor)
{
    if (toonFactor >= toonLightThreshold)
        return float4(saturate(baseColor.rgb * 1.2f), baseColor.a);
    else if (toonFactor < toonDarkThreshold)
        return float4(baseColor.rgb * 0.6f, baseColor.a);
    else
        return baseColor;
}

// ------------------------------------------------------------
// SampleToonColor : pick light/dark texture or tone-shift the base
// ------------------------------------------------------------
float4 SampleToonColor(float2 uv, float toonFactor)
{
    float4 lBase = g_texture.Sample(g_sampler, uv);

    if (toonFactor >= toonLightThreshold)
        return useToonLightTexture ? g_toonLightTexture.Sample(g_sampler, uv) : ApplyToneShift(lBase, toonFactor);
    else if (toonFactor < toonDarkThreshold)
        return useToonDarkTexture ? g_toonDarkTexture.Sample(g_sampler, uv) : ApplyToneShift(lBase, toonFactor);
    else
        return lBase;
}

float4 main(VS_OUTPUT_PHONG input) : SV_TARGET
{
    float3 lN = normalize(input.worldNormal);
    float3 lV = normalize(cameraPos - input.worldPos);

    float lToonFactor = 0.0f;
    float3 lWeightedLightColor = float3(0, 0, 0);
    float lTotalWeight = 0.0f;

    [loop]
    for (int lI = 0; lI < lightCount; lI++)
    {
        float lContrib = CalcToonContribution(
            lights[lI], lN, input.worldPos,
            CalcShadowForLight(lI, input.worldPos));

        lToonFactor = max(lToonFactor, lContrib);

        lWeightedLightColor += lights[lI].lightColor * lContrib;
        lTotalWeight += lContrib;
    }

    float3 lLightColor = (lTotalWeight > 0.001f)
        ? saturate(lWeightedLightColor / lTotalWeight)
        : float3(0, 0, 0);

    float4 lBaseColor = useTexture
        ? SampleToonColor(input.uv, lToonFactor)
        : ApplyToneShift(materialColor, lToonFactor);

    static const float LIGHT_COLOR_STRENGTH = 0.5f;

    float lLightInfluence = saturate(lToonFactor) * LIGHT_COLOR_STRENGTH;
    float3 lLitColor = saturate(lBaseColor.rgb + lLightColor * lLightInfluence);

    float3 lAmbient = lBaseColor.rgb * 0.15f;
    float3 lFinalColor = max(lLitColor, lAmbient) + CalcRimLight(lN, lV);

    return float4(saturate(lFinalColor), lBaseColor.a);
}