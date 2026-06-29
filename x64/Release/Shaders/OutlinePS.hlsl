#include "Outline.hlsli"

float4 main(VS_OUTPUT_OUTLINE input) : SV_TARGET
{
    return outlineColor;
}