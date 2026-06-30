// ============================================================
// OutlineSkinnedVS.hlsl
// Vertex shader for inverted-hull outline (skinned mesh)
// Same extrusion as OutlineVS, but applies the bone palette
// first so the outline hull follows skeletal animation.
// Author: Chan WingChung
// ============================================================
#include "Outline.hlsli"

// ------------------------------------------------------------
// main : skin the vertex, then extrude along the skinned push
//        direction and project with the outline view-proj
// input : skinned vertex (adds boneIndices / boneWeights)
// ------------------------------------------------------------
VS_OUTPUT_OUTLINE main(VS_INPUT_SKINNED input)
{
    VS_OUTPUT_OUTLINE lOut;

    // --- Blend bones, then skin position & smooth normal into
    //     animated model space. The normal MUST be skinned too,
    //     or the push direction stays stuck in bind pose.
    float4x4 lSkin = ComputeSkinMatrix(input.boneIndices, input.boneWeights);
    float3 lSkinnedPos = mul(float4(input.pos, 1.0f), lSkin).xyz;
    float3 lSkinnedSmoothNormal = mul(float4(input.smoothNormal, 0.0f), lSkin).xyz;

    // --- Default push direction: skinned smooth normal in world
    float3 lWorldSmoothNormal = normalize(mul(float4(lSkinnedSmoothNormal, 0.0f), world).xyz);
    float3 lPushDir = lWorldSmoothNormal;

    if (useOutlineNormalMap)
    {
        // --- Skin normal & tangent too so the TBN matches the pose
        float3 lSkinnedNormal = mul(float4(input.normal, 0.0f), lSkin).xyz;
        float3 lSkinnedTangent = mul(float4(input.tangent, 0.0f), lSkin).xyz;

        float3 lWorldNormal = normalize(mul(float4(lSkinnedNormal, 0.0f), world).xyz);
        float3 lWorldTangent = normalize(mul(float4(lSkinnedTangent, 0.0f), world).xyz);

        // --- Gram-Schmidt: re-orthogonalize tangent against normal
        lWorldTangent = normalize(lWorldTangent - lWorldNormal * dot(lWorldNormal, lWorldTangent));
        float3 lWorldBitangent = cross(lWorldNormal, lWorldTangent);

        float3x3 lTbn = float3x3(lWorldTangent, lWorldBitangent, lWorldNormal);

        // --- Sample the outline normal map and unpack to [-1, 1]
        float3 lMapSample = g_outlineNormalMap.SampleLevel(g_outlineSampler, input.uv, 0).rgb;
        float3 lTangentSpaceNormal = lMapSample * 2.0f - 1.0f;

        lPushDir = normalize(mul(lTangentSpaceNormal, lTbn));
    }

    // --- Outline thickness (uniform width by default)
    float lWidth = outlineWidth;

    if (useOutlineWidthMap)
    {
        // --- Modulate width per-texel so the line can taper
        float lWidthFactor = g_outlineWidthMap.SampleLevel(g_outlineSampler, input.uv, 0).r;
        lWidth = outlineWidth * lWidthFactor * outlineWidthScale;
    }

    // --- Skinned world pos -> push outward -> clip space
    float4 lWorldPos = mul(float4(lSkinnedPos, 1.0f), world);
    lWorldPos.xyz += lPushDir * lWidth;

    lOut.svPosition = mul(lWorldPos, viewProj);
    lOut.uv = input.uv;

    return lOut;
}