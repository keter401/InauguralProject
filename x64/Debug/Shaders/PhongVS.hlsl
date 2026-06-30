// ============================================================
// PhongVS.hlsl
// Vertex shader for Phong / Dissolve lighting
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

VS_OUTPUT_PHONG main(VS_INPUT input)
{
    VS_OUTPUT_PHONG lOut;
    lOut.svPosition = TransformToClip(input.pos);
    lOut.worldPos = TransformToWorld(input.pos);
    lOut.worldNormal = TransformNormalToWorld(input.normal);
    lOut.uv = input.uv;
    return lOut;
}