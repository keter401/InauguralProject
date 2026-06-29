// ============================================================
// Matrix4x4.cpp
// 4x4 行列クラスの実装
// DirectXMath を内部で使用し、変換・回転・スケール行列などを提供する
// 制作者：Chan WingChung
// ============================================================

#include "Matrix4x4.h"

using namespace DirectX;

// ------------------------------------------------------------
// MATRIX4X4（デフォルトコンストラクター）
// 単位行列で初期化する
// 引数：なし
// ------------------------------------------------------------
MATRIX4X4::MATRIX4X4()
{
    // ─── 単位行列として初期化する
    XMStoreFloat4x4(&m_mat, XMMatrixIdentity());
}

// ------------------------------------------------------------
// MATRIX4X4（XMMATRIX コンストラクター）
// DirectXMath の XMMATRIX から変換して初期化する
// 引数：xmMat - DirectXMath の行列
// ------------------------------------------------------------
MATRIX4X4::MATRIX4X4(const XMMATRIX& xmMat)
{
    // ─── XMMATRIX を XMFLOAT4X4 に変換して保持する
    XMStoreFloat4x4(&m_mat, xmMat);
}

// ------------------------------------------------------------
// MATRIX4X4（XMFLOAT4X4 コンストラクター）
// XMFLOAT4X4 をそのまま格納して初期化する
// 引数：xmFloat4x4 - 行列データ
// ------------------------------------------------------------
MATRIX4X4::MATRIX4X4(const XMFLOAT4X4& xmFloat4x4)
    : m_mat(xmFloat4x4)
{
}

// ------------------------------------------------------------
// ToXMMatrix
// 内部データを DirectXMath の XMMATRIX に変換して返す
// 引数：なし
// ------------------------------------------------------------
XMMATRIX MATRIX4X4::ToXMMatrix() const
{
    // ─── XMFLOAT4X4 を SIMD 演算可能な XMMATRIX に変換する
    return XMLoadFloat4x4(&m_mat);
}

// ------------------------------------------------------------
// operator*
// 2つの行列を乗算して結果を返す
// 引数：other - 右辺の行列
// ------------------------------------------------------------
MATRIX4X4 MATRIX4X4::operator*(const MATRIX4X4& other) const
{
    // ─── 両行列を XMMATRIX に変換して乗算する
    XMMATRIX lA = XMLoadFloat4x4(&m_mat);
    XMMATRIX lB = XMLoadFloat4x4(&other.m_mat);
    return MATRIX4X4(lA * lB);
}

// ------------------------------------------------------------
// Transpose
// 行列を転置して返す（行と列を入れ替える）
// 引数：なし
// ------------------------------------------------------------
MATRIX4X4 MATRIX4X4::Transpose() const
{
    // ─── HLSL への転送前に行・列順序を合わせるために使用する
    return MATRIX4X4(XMMatrixTranspose(XMLoadFloat4x4(&m_mat)));
}

// ------------------------------------------------------------
// Identity
// 単位行列を生成して返す
// 引数：なし
// ------------------------------------------------------------
MATRIX4X4 MATRIX4X4::Identity()
{
    return MATRIX4X4(XMMatrixIdentity());
}

// ------------------------------------------------------------
// Translation
// 平行移動行列を生成して返す
// 引数：x, y, z - 各軸の移動量
// ------------------------------------------------------------
MATRIX4X4 MATRIX4X4::Translation(float x, float y, float z)
{
    return MATRIX4X4(XMMatrixTranslation(x, y, z));
}

// ------------------------------------------------------------
// RotationX
// X 軸周りの回転行列を生成して返す
// 引数：angle - 回転角度（ラジアン）
// ------------------------------------------------------------
MATRIX4X4 MATRIX4X4::RotationX(float angle)
{
    return MATRIX4X4(XMMatrixRotationX(angle));
}

// ------------------------------------------------------------
// RotationY
// Y 軸周りの回転行列を生成して返す
// 引数：angle - 回転角度（ラジアン）
// ------------------------------------------------------------
MATRIX4X4 MATRIX4X4::RotationY(float angle)
{
    return MATRIX4X4(XMMatrixRotationY(angle));
}

// ------------------------------------------------------------
// RotationZ
// Z 軸周りの回転行列を生成して返す
// 引数：angle - 回転角度（ラジアン）
// ------------------------------------------------------------
MATRIX4X4 MATRIX4X4::RotationZ(float angle)
{
    return MATRIX4X4(XMMatrixRotationZ(angle));
}

// ------------------------------------------------------------
// Scale
// スケール行列を生成して返す
// 引数：x, y, z - 各軸のスケール倍率
// ------------------------------------------------------------
MATRIX4X4 MATRIX4X4::Scale(float x, float y, float z)
{
    return MATRIX4X4(XMMatrixScaling(x, y, z));
}

// ------------------------------------------------------------
// LookAtLH
// 左手系ビュー行列を生成して返す（カメラ位置・注視点・上方向から計算）
// 引数：eye - カメラ位置
//        at  - 注視点
//        up  - カメラ上方向ベクトル
// ------------------------------------------------------------
MATRIX4X4 MATRIX4X4::LookAtLH(
    const VECTOR3& eye,
    const VECTOR3& at,
    const VECTOR3& up)
{
    // ─── VECTOR3 を XMVECTOR に変換してビュー行列を計算する
    XMVECTOR lEye = XMVectorSet(eye.x, eye.y, eye.z, 0.0f);
    XMVECTOR lAt = XMVectorSet(at.x, at.y, at.z, 0.0f);
    XMVECTOR lUp = XMVectorSet(up.x, up.y, up.z, 0.0f);
    return MATRIX4X4(XMMatrixLookAtLH(lEye, lAt, lUp));
}

// ------------------------------------------------------------
// PerspectiveFovLH
// 左手系透視投影行列を生成して返す
// 引数：fovY   - 垂直視野角（ラジアン）
//        aspect - アスペクト比（幅 / 高さ）
//        nearZ  - ニアクリップ距離
//        farZ   - ファークリップ距離
// ------------------------------------------------------------
MATRIX4X4 MATRIX4X4::PerspectiveFovLH(
    float fovY,
    float aspect,
    float nearZ,
    float farZ)
{
    // ─── 指定された視野角とアスペクト比で透視投影行列を生成する
    return MATRIX4X4(XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ));
}