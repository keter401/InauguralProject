// ============================================================
// NormalMapPS.hlsl
// Pixel shader for normal mapping (multi-light + shadow)
// Author: Chan WingChung
// ============================================================
#include "LightCommon.hlsli"

float4 main(PS_INPUT_NORMAL_MAP input) : SV_TARGET
{
    float4 lBaseColor = g_texture.Sample(g_sampler, input.uv);

    if (!useNormalMap)
        return lBaseColor;

    float3 lN = UnpackNormalMap(input.worldNormal, input.worldTangent, input.uv);
    float3 lV = normalize(cameraPos - input.worldPos);

    float3 lAmbient = lBaseColor.rgb * ambientStrength;
    float3 lLighting = float3(0, 0, 0);

    [loop]
    for (int lI = 0; lI < lightCount; lI++)
        lLighting += CalcPhongContribution(lights[lI], lN, lV, input.worldPos,
                    lBaseColor.rgb, CalcShadowForLight(lI, input.worldPos), shininess);

    float3 lLinearColor = lAmbient + lLighting;
    float3 lFinal = GammaCorrect(ToneMapAces(lLinearColor));

    return float4(lFinal, lBaseColor.a);
}