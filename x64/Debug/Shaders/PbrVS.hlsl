// ============================================================
// PbrVS.hlsl
// Vertex shader for physically based rendering
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

VS_OUTPUT_NORMAL_MAP main(VS_INPUT_TANGENT input)
{
    VS_OUTPUT_NORMAL_MAP lOut;
    lOut.svPosition = TransformToClip(input.pos);
    lOut.worldPos = TransformToWorld(input.pos);
    lOut.worldNormal = TransformNormalToWorld(input.normal);
    lOut.worldTangent = TransformTangentToWorld(input.tangent);
    lOut.uv = input.uv;
    return lOut;
}