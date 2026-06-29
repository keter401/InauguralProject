// ============================================================
// LightCommon.hlsli
// Light & shadow: data definitions, shadow sampling,
// and all lighting calculations
// Author: Chan WingChung
// ============================================================
#ifndef LIGHT_COMMON_HLSLI
#define LIGHT_COMMON_HLSLI

#include "Common3D.hlsli"

// ============================================================
// Light data (moved from Common3D)
// ============================================================
struct LIGHT_DATA
{
    int lightType; // 0=Point, 1=Directional, 2=Spot, 3=Area
    float3 padType;
    float3 lightPos;
    float padL0;
    float3 lightDir;
    float padL1;
    float3 lightColor;
    float padL2;
    float intensity;
    float range;
    float spotAngle;
    float spotFalloff;
    float areaWidth;
    float areaHeight;
    float2 padArea;
};
cbuffer CB_LIGHTS : register(b1)
{
    LIGHT_DATA lights[MAX_LIGHTS];
    int lightCount;
    float3 padCount;
};

// ============================================================
// Shadow data (absorbed from ShadowCommon)
// ============================================================
struct SHADOW_DATA
{
    float3 lightPos;
    float farZ;
};
cbuffer CB_SHADOW : register(b2)
{
    SHADOW_DATA shadows[MAX_LIGHTS];
    int shadowCount;
    float3 padShadow;
};

TextureCubeArray g_shadowCubeArray : register(t2);
SamplerState g_shadowSampler : register(s1);

// ------------------------------------------------------------
// CalcShadowForLight : 3x3 PCF on a cube-array shadow map
// Returns 1.0 = fully lit, 0.0 = fully shadowed
// ------------------------------------------------------------
float CalcShadowForLight(int lightIndex, float3 worldPos)
{
    if (lightIndex >= shadowCount)
        return 1.0f;

    float3 lToPixel = worldPos - shadows[lightIndex].lightPos;
    float lCurrentDist = length(lToPixel);
    if (lCurrentDist < 0.01f)
        return 1.0f;

    float lFarZ = shadows[lightIndex].farZ;
    float lBias = max(0.05f, lCurrentDist * 0.02f);

    // --- Fixed world-axis PCF offsets (stable near the directly-below region)
    float lOffsetScale = 0.008f * lCurrentDist;
    float3 lAxisX = float3(lOffsetScale, 0.0f, 0.0f);
    float3 lAxisZ = float3(0.0f, 0.0f, lOffsetScale);

    float lShadowSum = 0.0f;
    [unroll]
    for (int lY = -1; lY <= 1; lY++)
    {
        [unroll]
        for (int lX = -1; lX <= 1; lX++)
        {
            float3 lSampleDir = lToPixel + lAxisX * lX + lAxisZ * lY;
            float lClosest01 = g_shadowCubeArray.Sample(
                g_shadowSampler, float4(lSampleDir, lightIndex)).r;
            float lClosestDist = lClosest01 * lFarZ;
            lShadowSum += (lCurrentDist - lBias > lClosestDist) ? 0.0f : 1.0f;
        }
    }
    return lShadowSum / 9.0f;
}

// ============================================================
// Lighting calculations
// ============================================================
struct LIGHT_RESULT
{
    float3 L;
    float attenuation;
};

// ------------------------------------------------------------
// CalcLightDir : compute light direction L and attenuation
// lightType  0=Point, 1=Directional, 2=Spot, 3=Area
// ------------------------------------------------------------
LIGHT_RESULT CalcLightDir(LIGHT_DATA light, float3 worldPos)
{
    LIGHT_RESULT lOut;
    lOut.L = float3(0, 1, 0);
    lOut.attenuation = 0.0f;

    if (light.lightType == 0)
    {
        float3 lToLight = light.lightPos - worldPos;
        float lDist = length(lToLight);
        if (lDist >= 0.05f)
        {
            lOut.L = lToLight / lDist;
            float lA = saturate(1.0f - lDist / light.range);
            lOut.attenuation = lA * lA;
        }
    }
    else if (light.lightType == 1)
    {
        lOut.L = normalize(-light.lightDir);
        lOut.attenuation = 1.0f;
    }
    else if (light.lightType == 2)
    {
        float3 lToLight = light.lightPos - worldPos;
        float lDist = length(lToLight);
        if (lDist >= 0.05f)
        {
            lOut.L = lToLight / lDist;
            float lRangeAtten = saturate(1.0f - lDist / light.range);
            float lCosAngle = dot(normalize(light.lightDir), -lOut.L);
            float lSpotAtten = saturate((lCosAngle - cos(light.spotAngle)) / max(light.spotFalloff, 0.001f));
            lOut.attenuation = lRangeAtten * lSpotAtten;
        }
    }
    else if (light.lightType == 3)
    {
        float3 lToLight = light.lightPos - worldPos;
        float lDist = length(lToLight);
        if (lDist >= 0.05f)
        {
            lOut.L = lToLight / lDist;
            float lRangeAtten = saturate(1.0f - lDist / light.range);
            float lFacing = saturate(dot(normalize(light.lightDir), -lOut.L));
            lOut.attenuation = lRangeAtten * lFacing;
        }
    }

    return lOut;
}

// ------------------------------------------------------------
// CalcPhongContribution : diffuse + specular for one light
// ------------------------------------------------------------
float3 CalcPhongContribution(LIGHT_DATA light, float3 N, float3 V, float3 worldPos, float3 baseColor, float shadowFactor, float shininess)
{
    LIGHT_RESULT lLR = CalcLightDir(light, worldPos);

    float lDiff = saturate(dot(N, lLR.L));
    float3 lDiffuse = baseColor * light.lightColor * lDiff * light.intensity * lLR.attenuation * shadowFactor;

    float3 lR = reflect(-lLR.L, N);
    float lSpec = pow(saturate(dot(lR, V)), shininess);
    float3 lSpecular = light.lightColor * lSpec * light.intensity * lLR.attenuation * shadowFactor;

    return lDiffuse + lSpecular;
}

// ------------------------------------------------------------
// CalcToonContribution : single scalar lighting term for cel shading
// ------------------------------------------------------------
float CalcToonContribution(LIGHT_DATA light, float3 N, float3 worldPos, float shadowFactor)
{
    LIGHT_RESULT lLR = CalcLightDir(light, worldPos);
    return saturate(dot(N, lLR.L)) * lLR.attenuation * shadowFactor * light.intensity;
}

// ------------------------------------------------------------
// CalcRimLight : fresnel-like edge glow (from PhongPS / ToonPS)
// ------------------------------------------------------------
float3 CalcRimLight(float3 N, float3 V)
{
    if (!useRimLight)
        return float3(0, 0, 0);
    float lRimFactor = pow(1.0f - saturate(dot(N, V)), rimPower);
    return rimColor * lRimFactor * rimIntensity;
}

#endif