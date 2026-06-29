// ============================================================
// Vertex.h
// 3D モデルとギズモで使用する頂点構造体の定義
// 制作者：Chan WingChung
// ============================================================

#pragma once
#include <cstdint> 

static const int MAX_BONE_INFLUENCE = 4;

// ─── 3D モデル頂点
struct VERTEX_3D
{
    float x, y, z;
    float normalX, normalY, normalZ;
    float u, v;
    float tangentX, tangentY, tangentZ;
    float smoothNormalX, smoothNormalY, smoothNormalZ;
};

// ─── 1頂点ぶんのボーン影響
struct VERTEX_BONE_DATA
{   
    uint32_t boneIndices[MAX_BONE_INFLUENCE] = { 0, 0, 0, 0 };
    float    boneWeights[MAX_BONE_INFLUENCE] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

// ─── ギズモ頂点
struct VERTEX_GIZMO
{
    float x, y, z;
    float r, g, b;
};

// ─── スキンメッシュ用頂点
struct VERTEX_SKINNED
{
    float    x, y, z;
    float    normalX, normalY, normalZ;
    float    u, v;
    float    tangentX, tangentY, tangentZ;
    float    smoothNormalX, smoothNormalY, smoothNormalZ;
    uint32_t boneIndices[MAX_BONE_INFLUENCE];
    float    boneWeights[MAX_BONE_INFLUENCE];
};