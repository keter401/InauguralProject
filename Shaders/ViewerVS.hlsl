// ============================================================
// ViewerVS.hlsl
// Fullscreen-triangle vertex shader for the buffer viewer.
// Emits a screen-covering triangle from SV_VertexID and passes a
// 0..1 uv (Y flipped so textures display upright).
// Author: Chan WingChung
// ============================================================

struct VS_OUTPUT_VIEW
{
    float4 svPosition : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUTPUT_VIEW main(uint vertexId : SV_VertexID)
{
    VS_OUTPUT_VIEW lOut;

    // --- (0,0) (2,0) (0,2) -> screen-covering triangle
    float2 lUv = float2((vertexId << 1) & 2, vertexId & 2);

    lOut.svPosition = float4(lUv * 2.0f - 1.0f, 0.0f, 1.0f);
    lOut.uv = float2(lUv.x, 1.0f - lUv.y); // flip Y = upright
    return lOut;
}