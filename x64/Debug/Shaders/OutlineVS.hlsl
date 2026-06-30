// ============================================================
// OutlineVS.hlsl
// Vertex shader for inverted-hull outline (static mesh)
// Pushes each vertex out along its smooth normal so the
// front-culled back hull appears as a silhouette outline.
// Author: Chan WingChung
// ============================================================
#include "Outline.hlsli"

// ------------------------------------------------------------
// main : extrude the vertex along the (optionally mapped) push
//        direction, then project with the outline view-proj
// input : position / normal / uv / smoothNormal / tangent
// ------------------------------------------------------------
VS_OUTPUT_OUTLINE main(VS_INPUT_OUTLINE input)
{
    VS_OUTPUT_OUTLINE lOut;

    // --- Default push direction: smooth normal in world space
    //     (averaged normals close the gaps at hard edges)
    float3 lWorldSmoothNormal = normalize(mul(float4(input.smoothNormal, 0.0f), world).xyz);
    float3 lPushDir = lWorldSmoothNormal;

    if (useOutlineNormalMap)
    {
        // --- Build a world-space TBN to remap the push direction
        float3 lWorldNormal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
        float3 lWorldTangent = normalize(mul(float4(input.tangent, 0.0f), world).xyz);

        // --- Gram-Schmidt: re-orthogonalize tangent against normal
        lWorldTangent = normalize(lWorldTangent - lWorldNormal * dot(lWorldNormal, lWorldTangent));
        float3 lWorldBitangent = cross(lWorldNormal, lWorldTangent);

        float3x3 lTbn = float3x3(lWorldTangent, lWorldBitangent, lWorldNormal);

        // --- Sample the outline normal map and unpack to [-1, 1]
        float3 lMapSample = g_outlineNormalMap.SampleLevel(g_outlineSampler, input.uv, 0).rgb;
        float3 lTangentSpaceNormal = lMapSample * 2.0f - 1.0f;

        // --- Tangent-space normal -> world space = new push dir
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

    // --- To world space, push outward, then to clip space
    float4 lWorldPos = mul(float4(input.pos, 1.0f), world);
    lWorldPos.xyz += lPushDir * lWidth;

    lOut.svPosition = mul(lWorldPos, viewProj);
    lOut.uv = input.uv;

    return lOut;
}