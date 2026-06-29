// ============================================================
// GizmoVS.hlsl
// Vertex shader for debug gizmo (self-contained, no Common3D)
// Author: Chan WingChung
// ============================================================
cbuffer CB_GIZMO : register(b0)
{
    float4x4 wvp;
    float4 color;
};

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 color : COLOR;
};

struct VS_OUTPUT
{
    float4 svPosition : SV_POSITION;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT lOut;
    lOut.svPosition = mul(float4(input.pos, 1.0f), wvp);
    return lOut;
}