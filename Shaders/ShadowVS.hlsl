// ============================================================
// ShadowVS.hlsl
// Vertex shader for shadow-map generation (writes world distance)
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

VS_OUTPUT_SHADOW main(VS_INPUT input)
{
    VS_OUTPUT_SHADOW lOut;
    lOut.svPosition = TransformToClip(input.pos);
    lOut.worldPos = TransformToWorld(input.pos);
    lOut.uv = input.uv;
    return lOut;
}