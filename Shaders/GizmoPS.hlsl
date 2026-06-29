// ============================================================
// GizmoPS.hlsl
// Pixel shader for debug gizmo (outputs flat color)
// Author: Chan WingChung
// ============================================================
cbuffer CB_GIZMO : register(b0)
{
    float4x4 wvp;
    float4 color;
};

float4 main() : SV_TARGET
{
    return color;
}