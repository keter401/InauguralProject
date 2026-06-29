// ============================================================
// Matrix4x4.h
// 4x4 行列クラスの宣言
// DirectXMath をラップし、変換行列の生成・合成・転置を提供する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <DirectXMath.h>
#include "Vector3.h"

class MATRIX4X4
{
public:
    // ─── 行列データ
    DirectX::XMFLOAT4X4 m_mat;

    // ─── コンストラクター
    MATRIX4X4();
    MATRIX4X4(const DirectX::XMMATRIX& xmMat);
    MATRIX4X4(const DirectX::XMFLOAT4X4& xmFloat4x4);

    // ─── 変換・演算
    DirectX::XMMATRIX ToXMMatrix() const;
    MATRIX4X4 operator*(const MATRIX4X4& other) const;
    MATRIX4X4 Transpose() const;

    // ─── 基本行列ファクトリ
    static MATRIX4X4 Identity();
    static MATRIX4X4 Translation(float x, float y, float z);
    static MATRIX4X4 RotationX(float angle);
    static MATRIX4X4 RotationY(float angle);
    static MATRIX4X4 RotationZ(float angle);
    static MATRIX4X4 Scale(float x, float y, float z);

    // ─── ビュー・プロジェクション行列ファクトリ
    static MATRIX4X4 LookAtLH(
        const VECTOR3& eye,
        const VECTOR3& at,
        const VECTOR3& up);

    static MATRIX4X4 PerspectiveFovLH(
        float fovY,
        float aspect,
        float nearZ,
        float farZ);
};