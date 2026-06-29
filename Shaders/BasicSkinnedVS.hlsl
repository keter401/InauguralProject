// ============================================================
// BasicSkinnedVS.hlsl
// Skinned vertex shader for unlit / basic rendering
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

VS_OUTPUT_SIMPLE main(VS_INPUT_SKINNED input)
{
    VS_OUTPUT_SIMPLE lOut;

    float4x4 lSkin = ComputeSkinMatrix(input.boneIndices, input.boneWeights);
    float4 lSkinnedPos = mul(float4(input.pos, 1.0f), lSkin);

    lOut.svPosition = mul(lSkinnedPos, wvp);
    lOut.uv = input.uv;
    return lOut;
}