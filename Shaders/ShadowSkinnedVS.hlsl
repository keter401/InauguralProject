// ============================================================
// ShadowSkinnedVS.hlsl
// Skinned vertex shader for shadow-map generation
// Deforms vertices by the bone palette so shadows follow the pose
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

VS_OUTPUT_SHADOW main(VS_INPUT_SKINNED input)
{
    VS_OUTPUT_SHADOW lOut;

    // --- Blend bones first, then project with the light's wvp
    float4x4 lSkin = ComputeSkinMatrix(input.boneIndices, input.boneWeights);
    float4 lSkinnedPos = mul(float4(input.pos, 1.0f), lSkin);

    lOut.svPosition = mul(lSkinnedPos, wvp);
    lOut.worldPos = mul(lSkinnedPos, world).xyz;
    lOut.uv = input.uv;

    return lOut;
}