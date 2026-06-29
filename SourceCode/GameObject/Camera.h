// ============================================================
// Camera.h
// オービットカメラクラスの宣言
// Yaw / Pitch / Distance で位置を制御し View・Projection 行列を提供する
// 制作者：Chan WingChung
// ============================================================

#pragma once
#include "GameObject.h"
#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"

class CAMERA : public GAME_OBJECT
{
private:
    // ─── カメラパラメータ
    VECTOR3 m_position = { 0.0f, 1.0f, -5.0f };
    VECTOR3 m_target = { 0.0f, 1.0f,  0.0f };
    VECTOR3 m_up = { 0.0f, 1.0f,  0.0f };

    // ─── オービット制御
    float m_distance = 5.0f;
    float m_yaw = -90.0f;
    float m_pitch = 0.0f;

    // ─── 投影パラメータ
    float m_fovY = DirectX::XM_PIDIV4;
    float m_aspect = 1280.0f / 720.0f;
    float m_nearZ = 0.1f;
    float m_farZ = 1000.0f;

    // ─── 行列キャッシュ
    MATRIX4X4 m_view;
    MATRIX4X4 m_projection;

    // ─── 内部更新
    void UpdateMatrix();
    void UpdateEyeFromOrbit();

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) override;
    void Update(float deltaSeconds = 0) override;
    void Draw()      override {}
    void Finalize()  override {}
    void DrawImGui() override;

    // ─── セッター
    void SetPosition(const VECTOR3& position) { m_position = position; }
    void SetTarget(const VECTOR3& target) { m_target = target; }
    void SetUp(const VECTOR3& up) { m_up = up; }
    void SetFovY(float fovY) { m_fovY = fovY; }
    void SetAspect(float aspect) { m_aspect = aspect; }
    void SetNearZ(float nearZ) { m_nearZ = nearZ; }
    void SetFarZ(float farZ) { m_farZ = farZ; }

    // ─── ゲッター
    const VECTOR3& GetPosition()         const { return m_position; }
    const VECTOR3& GetTarget()           const { return m_target; }
    const MATRIX4X4& GetViewMatrix()       const { return m_view; }
    const MATRIX4X4& GetProjectionMatrix() const { return m_projection; }

    // ─── 行列演算
    MATRIX4X4 GetWVP(const MATRIX4X4& world) const;
};