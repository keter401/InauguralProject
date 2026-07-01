// ============================================================
// CloudVS.hlsl
// Fullscreen-triangle vertex shader for the raymarched cloud pass.
// Emits one screen-covering triangle from SV_VertexID (no vertex
// buffer) and carries clip-space xy so the PS can rebuild the ray.
// Author: Chan WingChung
// ============================================================

struct VS_OUTPUT_CLOUD
{
    float4 svPosition : SV_POSITION;
    float2 clip : TEXCOORD0; // NDC xy (w == 1), for ray rebuild
};

// ------------------------------------------------------------
// main : expand vertex id 0/1/2 into a screen-covering triangle
// input : vertexId - 0, 1, 2 from Draw(3, 0)
// ------------------------------------------------------------
VS_OUTPUT_CLOUD main(uint vertexId : SV_VertexID)
{
    VS_OUTPUT_CLOUD lOut;

    // --- (0,0) (2,0) (0,2) -> triangle that covers the whole screen
    float2 lUv = float2((vertexId << 1) & 2, vertexId & 2);

    // --- z = 1 (far) so hardware depth test lets geometry occlude us
    lOut.svPosition = float4(lUv * 2.0f - 1.0f, 1.0f, 1.0f);
    lOut.clip = lOut.svPosition.xy; // w == 1, already NDC

    return lOut;
}