// ============================================================
// ConstantBuffers.h
// HLSL 定数バッファと対応する C++ 構造体の定義
// 全シェーダーで共有する CB の型をここで一元管理する
// 制作者：Chan WingChung
// ============================================================

#pragma once
#include <DirectXMath.h>

// ─── 汎用 WVP + マテリアル（b0）
struct CB_WVP
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 wvp;
    DirectX::XMFLOAT4   materialColor;
    int                 useTexture;
    int                 useNormalMap;
    DirectX::XMFLOAT2   padWvp;
    DirectX::XMFLOAT3   cameraPos;
    float               padWvp2;

    float               toonLightThreshold = 0.7f;
    float               toonDarkThreshold = 0.3f;
    int                 useToonLightTexture = 0;
    int                 useToonDarkTexture = 0;

    DirectX::XMFLOAT3   rimColor = { 1.0f, 1.0f, 1.0f };
    float               rimPower = 2.0f;
    float               rimIntensity = 1.0f;
    int                 useRimLight = 0;
    float               shininess = 32.0f;
    float               ambientStrength = 0.15f;
};

// ─── アウトライン（b3）
struct CB_OUTLINE
{
    DirectX::XMFLOAT4X4 viewProj;
    float               outlineWidth;
    DirectX::XMFLOAT3   padOutline;
    DirectX::XMFLOAT4   outlineColor;
    int                 useOutlineNormalMap;
    int                 useOutlineWidthMap;
    float               outlineWidthScale;
    float               padOutline2;
};

// ─── ミラー（b4）
struct CB_MIRROR
{
    DirectX::XMFLOAT2 screenSize;
    DirectX::XMFLOAT2 padMirror;
};

// ─── ディゾルブ（b5）
struct CB_DISSOLVE
{
    float             dissolveThreshold;
    float             edgeWidth;
    int               dissolveEnabled;
    float             pad0;
    DirectX::XMFLOAT4 edgeColor;
};

// ─── PBR（b6）
struct CB_PBR
{
    float metallic;
    float roughness;
    int   useMetallicRoughnessMap;
    float iblIntensity;
};

// ─── ギズモ
struct CB_GIZMO
{
    DirectX::XMFLOAT4X4 wvp;
    DirectX::XMFLOAT4   color;
};

// ─── ライトデータ（1 灯分）
static const int MAX_LIGHTS = 8;

struct CB_LIGHT_DATA
{
    int               lightType;  float padType[3];
    DirectX::XMFLOAT3 lightPos;   float padL0;
    DirectX::XMFLOAT3 lightDir;   float padL1;
    DirectX::XMFLOAT3 lightColor; float padL2;
    float intensity;
    float range;
    float spotAngle;
    float spotFalloff;
    float areaWidth;
    float areaHeight;
    float padArea[2];
};

// ─── ライト一覧（最大 MAX_LIGHTS 灯、b1）
struct CB_LIGHTS
{
    CB_LIGHT_DATA     lights[MAX_LIGHTS];
    int               lightCount;
    DirectX::XMFLOAT3 padCount;
};

// ─── シャドウデータ（1 灯分）
struct SHADOW_DATA
{
    DirectX::XMFLOAT3 lightPos;
    float             farZ;
};

// ─── シャドウ一覧（最大 MAX_LIGHTS 灯、b2）
struct CB_SHADOW
{
    SHADOW_DATA shadows[MAX_LIGHTS];
    int         shadowCount;
    float       padShadow[3];
};

// ─── ボーンパレット（b8）
static const int MAX_BONES_CB = 128;
struct CB_BONES
{
    DirectX::XMFLOAT4X4 bones[MAX_BONES_CB];
};