// ============================================================
// PbrPS.hlsl
// Pixel shader for physically based rendering
// (Cook-Torrance BRDF + IBL)
// Author: Chan WingChung
// ============================================================
#include "LightCommon.hlsli"

static const float MAX_REFLECTION_LOD = 4.0f;

// ------------------------------------------------------------
// DistributionGGX : GGX/Trowbridge-Reitz normal distribution
// ------------------------------------------------------------
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float lA = roughness * roughness;
    float lA2 = lA * lA;
    float lNdotH = max(dot(N, H), 0.0f);
    float lDenom = (lNdotH * lNdotH * (lA2 - 1.0f) + 1.0f);
    return lA2 / (PI * lDenom * lDenom);
}

// ------------------------------------------------------------
// FresnelSchlick : Schlick approximation of Fresnel
// ------------------------------------------------------------
float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(max(1.0f - cosTheta, 0.0f), 5.0f);
}

// ------------------------------------------------------------
// FresnelSchlickRoughness : Fresnel with roughness (for IBL)
// ------------------------------------------------------------
float3 FresnelSchlickRoughness(float cosTheta, float3 f0, float roughness)
{
    float3 lOneMinusRough = float3(
        max(1.0f - roughness, f0.x),
        max(1.0f - roughness, f0.y),
        max(1.0f - roughness, f0.z));
    return f0 + (lOneMinusRough - f0) * pow(max(1.0f - cosTheta, 0.0f), 5.0f);
}

// ------------------------------------------------------------
// GeometrySchlickGGX / GeometrySmith : geometry self-shadowing
// ------------------------------------------------------------
float GeometrySchlickGGX(float ndotX, float roughness)
{
    float lR = roughness + 1.0f;
    float lK = (lR * lR) / 8.0f;
    return ndotX / (ndotX * (1.0f - lK) + lK);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return GeometrySchlickGGX(max(dot(N, V), 0.0f), roughness)
         * GeometrySchlickGGX(max(dot(N, L), 0.0f), roughness);
}

// ------------------------------------------------------------
// CalcPbrContribution : Cook-Torrance contribution for one light
// ------------------------------------------------------------
float3 CalcPbrContribution(
    LIGHT_DATA light,
    float3 N,
    float3 V,
    float3 worldPos,
    float3 albedo,
    float metallic,
    float roughness,
    float3 f0,
    float shadowFactor)
{
    LIGHT_RESULT lLR = CalcLightDir(light, worldPos);
    float3 lL = lLR.L;
    float3 lH = normalize(V + lL);

    float lNdotL = max(dot(N, lL), 0.0f);
    float lNdotV = max(dot(N, V), 0.0f);

    float lD = DistributionGGX(N, lH, roughness);
    float3 lF = FresnelSchlick(max(dot(lH, V), 0.0f), f0);
    float lG = GeometrySmith(N, V, lL, roughness);
    float3 lSpecular = (lD * lF * lG) / (4.0f * lNdotV * lNdotL + 0.0001f);
    float3 lKd = (1.0f - lF) * (1.0f - metallic);

    float3 lRadiance = light.lightColor * light.intensity * lLR.attenuation;
    if (light.lightType != 1)
    {
        float lDist = length(light.lightPos - worldPos);
        lRadiance *= 1.0f / max(lDist * lDist, 0.0001f);
    }

    return (lKd * albedo / PI + lSpecular) * lRadiance * lNdotL * shadowFactor;
}

float4 main(PS_INPUT_NORMAL_MAP input) : SV_TARGET
{
    // --- Normal (optionally perturbed by normal map)
    float3 lN = normalize(input.worldNormal);
    if (useNormalMap)
        lN = UnpackNormalMap(input.worldNormal, input.worldTangent, input.uv);

    float3 lV = normalize(cameraPos - input.worldPos);
    float3 lR = reflect(-lV, lN);
    float lNdotV = max(dot(lN, lV), 0.0f);

    // --- Material params
    float3 lAlbedo = materialColor.rgb;
    if (useTexture)
        lAlbedo *= g_texture.Sample(g_sampler, input.uv).rgb;

    float lMetallic = metallic;
    float lRoughness = roughness;
    if (useMetallicRoughnessMap)
    {
        float4 lMR = g_metallicRoughnessMap.Sample(g_sampler, input.uv);
        lMetallic = lMR.r;
        lRoughness = lMR.g;
    }

    float3 lF0 = lerp(float3(0.04f, 0.04f, 0.04f), lAlbedo, lMetallic);

    // --- Direct lighting
    float3 lLo = float3(0, 0, 0);
    [loop]
    for (int lI = 0; lI < lightCount; lI++)
        lLo += CalcPbrContribution(lights[lI], lN, lV, input.worldPos,
                   lAlbedo, lMetallic, lRoughness, lF0,
                   CalcShadowForLight(lI, input.worldPos));

    // --- Image-based lighting (ambient)
    float3 lFIbl = FresnelSchlickRoughness(lNdotV, lF0, lRoughness);
    float3 lKdIbl = (1.0f - lFIbl) * (1.0f - lMetallic);
    float3 lIrradiance = g_iblDiffuse.Sample(g_sampler, lN).rgb;
    float3 lIblDiffuse = lKdIbl * lAlbedo * lIrradiance;

    float lMipBias = log2(lRoughness + 1.0f) * MAX_REFLECTION_LOD;
    float3 lPrefilteredColor = g_iblSpecular.SampleBias(g_sampler, lR, lMipBias).rgb;
    float2 lBrdf = g_brdfLut.Sample(g_sampler, float2(lNdotV, lRoughness)).rg;
    float3 lIblSpecular = lPrefilteredColor * (lF0 * lBrdf.x + lBrdf.y);

    float3 lAmbient = (lIblDiffuse + lIblSpecular) * iblIntensity;
    float3 lColor = lAmbient + lLo;

    // --- Tone map + gamma
    lColor = GammaCorrect(ToneMapAces(lColor));

    return float4(lColor, materialColor.a);
}