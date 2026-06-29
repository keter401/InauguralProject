// ============================================================
// ToonSkinnedVS.hlsl
// Skinned vertex shader for toon shading
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

VS_OUTPUT_PHONG main(VS_INPUT_SKINNED input)
{
    VS_OUTPUT_PHONG lOut;

    // --- Blend bones, then transform skinned pos/normal into world/clip
    float4x4 lSkin = ComputeSkinMatrix(input.boneIndices, input.boneWeights);
    float4 lSkinnedPos = mul(float4(input.pos, 1.0f), lSkin);
    float3 lSkinnedNormal = mul(float4(input.normal, 0.0f), lSkin).xyz;

    lOut.worldPos = mul(lSkinnedPos, world).xyz;
    lOut.svPosition = mul(lSkinnedPos, wvp);
    lOut.worldNormal = normalize(mul(float4(lSkinnedNormal, 0.0f), world).xyz);
    lOut.uv = input.uv;

    return lOut;
}