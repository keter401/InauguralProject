// ============================================================
// Common3D.hlsli
// Shared foundation for all 3D shaders
// (transforms, textures, common math, VS output structs)
// Author: Chan WingChung
// ============================================================
#ifndef COMMON3D_HLSLI
#define COMMON3D_HLSLI

// --- Basic transform & material
cbuffer CB_WVP : register(b0)
{
    float4x4 world;
    float4x4 wvp;
    float4   materialColor;
    int      useTexture;
    int      useNormalMap;
    float2   padWvp;
    float3   cameraPos;
    float    padWvp2;

    float toonLightThreshold;
    float toonDarkThreshold;
    int   useToonLightTexture;
    int   useToonDarkTexture;

    float3 rimColor;
    float  rimPower;
    float  rimIntensity;
    int    useRimLight;
    float  shininess;
    float  ambientStrength;
};

// --- Effect-specific constant buffers
cbuffer CB_MIRROR : register(b4)
{
    float2 screenSize;
    float2 padMirror;
};
cbuffer CB_DISSOLVE : register(b5)
{
    float  dissolveThreshold;
    float  edgeWidth;
    int    dissolveEnabled;
    float  pad0Dissolve;
    float4 edgeColor;
};
cbuffer CB_PBR : register(b6)
{
    float metallic;
    float roughness;
    int   useMetallicRoughnessMap;
    float iblIntensity;
};

// --- Skinning
cbuffer CB_BONES : register(b8)
{
    float4x4 bones[128];
};

#define MAX_LIGHTS 8
static const float PI = 3.14159265f;

// --- Textures / samplers
Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);
Texture2D g_normalMap : register(t3);
Texture2D g_toonLightTexture : register(t6);
Texture2D g_toonDarkTexture : register(t7);
Texture2D g_metallicRoughnessMap : register(t9);
TextureCube g_iblDiffuse : register(t10);
TextureCube g_iblSpecular : register(t11);
Texture2D g_brdfLut : register(t12);

// --- Vertex inputs
struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};
struct VS_INPUT_TANGENT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 tangent : TANGENT;
};
struct VS_INPUT_SKINNED
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 tangent : TANGENT;
    float3 smoothNormal : SMOOTHNORMAL;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

// --- Vertex outputs (unified on svPosition)
struct VS_OUTPUT_SIMPLE
{
    float4 svPosition : SV_POSITION;
    float2 uv : TEXCOORD;
};
struct VS_OUTPUT_PHONG
{
    float4 svPosition : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float2 uv : TEXCOORD2;
};
struct VS_OUTPUT_NORMAL_MAP
{
    float4 svPosition : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 worldTangent : TEXCOORD3;
};
struct VS_OUTPUT_SHADOW
{
    float4 svPosition : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

// VS output == PS input. Keep one alias per effect.
#define PS_INPUT_PHONG      VS_OUTPUT_PHONG
#define PS_INPUT_NORMAL_MAP VS_OUTPUT_NORMAL_MAP

// ------------------------------------------------------------
// ComputeSkinMatrix : blend bone matrices by weight
// boneIndices : indices of influencing bones
// boneWeights : weight of each bone
// ------------------------------------------------------------
float4x4 ComputeSkinMatrix(uint4 boneIndices, float4 boneWeights)
{
    float lWsum = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;
    if (lWsum < 1e-5f)
        return float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

    return bones[boneIndices.x] * boneWeights.x +
           bones[boneIndices.y] * boneWeights.y +
           bones[boneIndices.z] * boneWeights.z +
           bones[boneIndices.w] * boneWeights.w;
}

// ------------------------------------------------------------
// Transform helpers (shared by every VS)
// ------------------------------------------------------------
float4 TransformToClip(float3 localPos)
{
    return mul(float4(localPos, 1.0f), wvp);
}
float3 TransformToWorld(float3 localPos)
{
    return mul(float4(localPos, 1.0f), world).xyz;
}
float3 TransformNormalToWorld(float3 n)
{
    return normalize(mul(float4(n, 0.0f), world).xyz);
}
float3 TransformTangentToWorld(float3 t)
{
    return normalize(mul(float4(t, 0.0f), world).xyz);
}

// ------------------------------------------------------------
// Color helpers (shared by every PS)
// ------------------------------------------------------------
float3 ToneMapAces(float3 color)
{
    const float lA = 2.51f, lB = 0.03f, lC = 2.43f, lD = 0.59f, lE = 0.14f;
    return saturate((color * (lA * color + lB)) / (color * (lC * color + lD) + lE));
}
float3 GammaCorrect(float3 color)
{
    return pow(max(color, 0.0f), 1.0f / 2.2f);
}

// ------------------------------------------------------------
// UnpackNormalMap : sample normal map and return world-space normal
// ------------------------------------------------------------
float3 UnpackNormalMap(float3 worldNormal, float3 worldTangent, float2 uv)
{
    float3 lN = normalize(worldNormal);
    float3 lT = normalize(worldTangent);
    lT = normalize(lT - lN * dot(lN, lT)); // Gram-Schmidt orthogonalization
    float3 lB = cross(lN, lT);
    float3x3 lTbn = float3x3(lT, lB, lN);
    float3 lTangentNormal = g_normalMap.Sample(g_sampler, uv).rgb * 2.0f - 1.0f;
    return normalize(mul(lTangentNormal, lTbn));
}

// ------------------------------------------------------------
// Procedural noise (shared by Dissolve / Shadow)
// ------------------------------------------------------------
float Hash21(float2 uv)
{
    uv = frac(uv * float2(127.1f, 311.7f));
    uv += dot(uv, uv + 19.19f);
    return frac(uv.x * uv.y);
}
float SmoothNoise(float2 uv)
{
    float2 lScaledUv = uv * 8.0f;
    float2 lI = floor(lScaledUv);
    float2 lF = frac(lScaledUv);
    float lA = Hash21(lI + float2(0.0f, 0.0f));
    float lB = Hash21(lI + float2(1.0f, 0.0f));
    float lC = Hash21(lI + float2(0.0f, 1.0f));
    float lD = Hash21(lI + float2(1.0f, 1.0f));
    float2 lU = smoothstep(0.0f, 1.0f, lF);
    return lerp(lerp(lA, lB, lU.x), lerp(lC, lD, lU.x), lU.y);
}

// ------------------------------------------------------------
// ApplyDissolve
// ------------------------------------------------------------
float3 ApplyDissolve(float2 uv)
{
    if (dissolveEnabled == 0)
        return float3(0, 0, 0);

    float lNoise = SmoothNoise(uv);
    clip(lNoise - dissolveThreshold); // Discard pixels below the threshold = dissolve

    float lEdgeFactor = 1.0f - saturate((lNoise - dissolveThreshold) / max(edgeWidth, 0.0001f));
    return edgeColor.rgb * lEdgeFactor; // Glow color at the edge
}

#endif