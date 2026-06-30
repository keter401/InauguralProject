// ============================================================
// OutlinePS.hlsl
// Pixel shader for the inverted-hull outline pass
// The hull is unlit and filled with a single flat color.
// Author: Chan WingChung
// ============================================================
#include "Outline.hlsli"

// ------------------------------------------------------------
// main : output the solid outline color (from CB_OUTLINE)
// input : interpolated outline vertex (only uv is carried)
// ------------------------------------------------------------
float4 main(VS_OUTPUT_OUTLINE input) : SV_TARGET
{
    ApplyDissolve(input.uv);
    return outlineColor;
}