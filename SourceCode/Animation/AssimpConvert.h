// ============================================================
// AssimpConvert.h
// Assimp 行列 → MATRIX4X4 の変換（プロジェクト唯一の転置規約）
// MODEL と ANIMATOR が共有し、変換の食い違いによる歪みを防ぐ
// 制作者：Chan WingChung
// ============================================================
#pragma once
#include <assimp/matrix4x4.h>
#include "Math/Matrix4x4.h"

// ------------------------------------------------------------
// ConvertAiMatrix : Assimp 行列を MATRIX4X4 に転置変換する
// 引数：aiMat - Assimp の行列（行優先・Row-major）
// ------------------------------------------------------------
inline MATRIX4X4 ConvertAiMatrix(const aiMatrix4x4& aiMat)
{
    // ─── Assimp は行優先。XMFLOAT4X4 へ転置して格納する
    DirectX::XMFLOAT4X4 lDx;
    lDx._11 = aiMat.a1; lDx._12 = aiMat.b1; lDx._13 = aiMat.c1; lDx._14 = aiMat.d1;
    lDx._21 = aiMat.a2; lDx._22 = aiMat.b2; lDx._23 = aiMat.c2; lDx._24 = aiMat.d2;
    lDx._31 = aiMat.a3; lDx._32 = aiMat.b3; lDx._33 = aiMat.c3; lDx._34 = aiMat.d3;
    lDx._41 = aiMat.a4; lDx._42 = aiMat.b4; lDx._43 = aiMat.c4; lDx._44 = aiMat.d4;
    return MATRIX4X4(lDx);
}