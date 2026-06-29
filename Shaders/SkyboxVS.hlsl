// ============================================================
// SkyboxVS.hlsl
// Vertex shader for skybox (camera-locked, depth pushed to far)
// Author: Chan WingChung
// ============================================================
#include "Common3D.hlsli"

VS_OUTPUT_NORMAL_MAP main(VS_INPUT_TANGENT input)
{
    VS_OUTPUT_NORMAL_MAP lOut;

    // --- Lock the skybox to the camera position (translation only)
    float4x4 lSkyWorld = world;
    lSkyWorld._41 = cameraPos.x;
    lSkyWorld._42 = cameraPos.y;
    lSkyWorld._43 = cameraPos.z;

    lOut.svPosition = TransformToClip(input.pos);

    // --- Force depth to the far plane (z = w) so the sky is always behind everything
    lOut.svPosition.z = lOut.svPosition.w;

    lOut.worldPos = mul(float4(input.pos, 1.0f), lSkyWorld).xyz;
    lOut.worldNormal = normalize(mul(float4(input.normal, 0.0f), lSkyWorld).xyz);
    lOut.worldTangent = normalize(mul(float4(input.tangent, 0.0f), lSkyWorld).xyz);
    lOut.uv = input.uv;

    return lOut;
}