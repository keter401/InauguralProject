#include "Outline.hlsli"

VS_OUTPUT_OUTLINE main(VS_INPUT_OUTLINE input)
{
    VS_OUTPUT_OUTLINE lOut;
    
    float3 lWorldSmoothNormal = normalize(mul(float4(input.smoothNormal, 0.0f), world).xyz);
    
    float3 lPushDir = lWorldSmoothNormal;

    if (useOutlineNormalMap)
    {
        float3 lWorldNormal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
        float3 lWorldTangent = normalize(mul(float4(input.tangent, 0.0f), world).xyz);
        
        lWorldTangent = normalize(lWorldTangent - lWorldNormal * dot(lWorldNormal, lWorldTangent));
        float3 lWorldBitangent = cross(lWorldNormal, lWorldTangent);

        float3x3 lTbn = float3x3(lWorldTangent, lWorldBitangent, lWorldNormal);
        
        float3 lMapSample = g_outlineNormalMap.SampleLevel(g_outlineSampler, input.uv, 0).rgb;
        
        float3 lTangentSpaceNormal = lMapSample * 2.0f - 1.0f;
        
        lPushDir = normalize(mul(lTangentSpaceNormal, lTbn));
    }
    
    float lWidth = outlineWidth;

    if (useOutlineWidthMap)
    {
        float lWidthFactor = g_outlineWidthMap.SampleLevel(g_outlineSampler, input.uv, 0).r;
        lWidth = outlineWidth * lWidthFactor * outlineWidthScale;
    }
    
    float4 lWorldPos = mul(float4(input.pos, 1.0f), world);
    
    lWorldPos.xyz += lPushDir * lWidth;
    
    lOut.svPosition = mul(lWorldPos, viewProj);
    lOut.uv = input.uv;

    return lOut;
}