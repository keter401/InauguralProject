// ============================================================
// PhongPS.hlsl
// Pixel shader for Phong lighting (multi-light + shadow + rim)
// Author: Chan WingChung
// ============================================================
#include "LightCommon.hlsli"

float4 main(PS_INPUT_PHONG input) : SV_TARGET
{
    float3 lN = normalize(input.worldNormal);
    float3 lV = normalize(cameraPos - input.worldPos);

    float4 lBaseColor = useTexture
        ? g_texture.Sample(g_sampler, input.uv)
        : materialColor;

    float3 lAmbient = lBaseColor.rgb * ambientStrength;
    float3 lLighting = float3(0, 0, 0);

    [loop]
    for (int lI = 0; lI < lightCount; lI++)
        lLighting += CalcPhongContribution(lights[lI], lN, lV, input.worldPos,
                    lBaseColor.rgb, CalcShadowForLight(lI, input.worldPos), shininess);

    float3 lLinearColor = lAmbient + lLighting + CalcRimLight(lN, lV);
    float3 lFinal = GammaCorrect(ToneMapAces(lLinearColor));

    return float4(lFinal, lBaseColor.a);
}