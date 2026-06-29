// ============================================================
// Camera.cpp
// オービットカメラクラスの実装
// Yaw / Pitch / Distance でカメラ位置を制御し View・Projection 行列を提供する
// 制作者：Chan WingChung
// ============================================================

#include "Camera.h"
#include "Imgui/imgui.h"
#include <cmath>

// ------------------------------------------------------------
// Initialize
// カメラのデバイスを保持して初期行列を計算する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool CAMERA::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;
    // ─── 初回の位置と行列を計算する
    UpdateEyeFromOrbit();
    UpdateMatrix();
    return true;
}

// ------------------------------------------------------------
// Update
// 毎フレームのオービット位置と行列を更新する
// 引数：なし
// ------------------------------------------------------------
void CAMERA::Update(float deltaSeconds)
{
    // ─── Yaw / Pitch / Distance からカメラ位置を再計算して行列を更新する
    UpdateEyeFromOrbit();
    UpdateMatrix();
}

// ------------------------------------------------------------
// DrawImGui
// カメラパラメーターを操作する ImGui ウィンドウを描画する
// 引数：なし
// ------------------------------------------------------------
void CAMERA::DrawImGui()
{
    if (ImGui::Begin("Camera"))
    {
        // ─── 注視点・距離・Yaw・Pitch のオービットパラメーターを編集する
        ImGui::DragFloat3("Target", &m_target.x, 0.1f);
        ImGui::DragFloat("Distance", &m_distance, 0.1f, 0.5f, 100.0f);
        ImGui::DragFloat("Yaw (deg)", &m_yaw, 1.0f, -360.0f, 360.0f);
        ImGui::DragFloat("Pitch (deg)", &m_pitch, 1.0f, -89.0f, 89.0f);

        ImGui::Separator();

        // ─── 現在の Eye 座標を読み取り専用で表示する
        ImGui::Text("Eye: (%.2f, %.2f, %.2f)", m_position.x, m_position.y, m_position.z);

        ImGui::Separator();

        // ─── 視野角・クリップ距離を編集する
        float lFovDeg = DirectX::XMConvertToDegrees(m_fovY);
        if (ImGui::DragFloat("FovY (deg)", &lFovDeg, 0.5f, 10.0f, 170.0f))
        {
            m_fovY = DirectX::XMConvertToRadians(lFovDeg);
        }

        ImGui::DragFloat("NearZ", &m_nearZ, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("FarZ", &m_farZ, 1.0f, 10.0f, 10000.0f);
    }
    ImGui::End();
}

// ------------------------------------------------------------
// UpdateEyeFromOrbit
// Yaw / Pitch / Distance から球面座標でカメラ位置を計算する
// 引数：なし
// ------------------------------------------------------------
void CAMERA::UpdateEyeFromOrbit()
{
    // ─── 球面座標変換：Yaw・Pitch をラジアンに変換して XYZ 成分を計算する
    float lYawRad = DirectX::XMConvertToRadians(m_yaw);
    float lPitchRad = DirectX::XMConvertToRadians(m_pitch);

    float lX = m_distance * cosf(lPitchRad) * cosf(lYawRad);
    float lY = m_distance * sinf(lPitchRad);
    float lZ = m_distance * cosf(lPitchRad) * sinf(lYawRad);

    // ─── 注視点を中心としたオービット位置に設定する
    m_position.x = m_target.x + lX;
    m_position.y = m_target.y + lY;
    m_position.z = m_target.z + lZ;
}

// ------------------------------------------------------------
// GetWVP
// ワールド行列に View・Projection を乗算して転置した WVP 行列を返す
// 引数：world - モデルのワールド行列
// ------------------------------------------------------------
MATRIX4X4 CAMERA::GetWVP(const MATRIX4X4& world) const
{
    // ─── WVP を計算して HLSL 用に転置する
    MATRIX4X4 lWVP = world * m_view * m_projection;
    return lWVP.Transpose();
}

// ------------------------------------------------------------
// UpdateMatrix
// View 行列と Projection 行列を現在のパラメーターで再構築する
// 引数：なし
// ------------------------------------------------------------
void CAMERA::UpdateMatrix()
{
    // ─── LookAt から View 行列を生成する
    m_view = MATRIX4X4::LookAtLH(m_position, m_target, m_up);
    // ─── 視野角・アスペクト比・クリップ距離から Projection 行列を生成する
    m_projection = MATRIX4X4::PerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ);
}