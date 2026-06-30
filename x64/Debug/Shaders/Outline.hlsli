// ============================================================
// Outline.hlsli
// Outline-only constants and structs
// Author: Chan WingChung
// ============================================================
#ifndef OUTLINE_HLSLI
#define OUTLINE_HLSLI

#include "Common3D.hlsli"

cbuffer CB_OUTLINE : register(b3)
{
    float4x4 viewProj;
    float outlineWidth;
    float3 padOutline;
    float4 outlineColor;
    int useOutlineNormalMap;
    int useOutlineWidthMap;
    float outlineWidthScale;
    float padOutline2;
};

Texture2D g_outlineNormalMap : register(t4);
Texture2D g_outlineWidthMap : register(t5);
SamplerState g_outlineSampler : register(s2);

struct VS_INPUT_OUTLINE
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 smoothNormal : SMOOTHNORMAL;
    float3 tangent : TANGENT;
};
struct VS_OUTPUT_OUTLINE
{
    float4 svPosition : SV_POSITION;
    float2 uv : TEXCOORD0;
};
#define PS_INPUT_OUTLINE VS_OUTPUT_OUTLINE

#endif