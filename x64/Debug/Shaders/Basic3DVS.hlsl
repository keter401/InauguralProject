// ============================================================
// Basic3DVS.hlsl
// Vertex shader for unlit / basic 3D rendering
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

VS_OUTPUT_SIMPLE main(VS_INPUT input)
{
    VS_OUTPUT_SIMPLE lOut;
    lOut.svPosition = TransformToClip(input.pos);
    lOut.uv = input.uv;
    return lOut;
}