// ============================================================
// GameObject.h
// 全ゲームオブジェクトの抽象基底クラスの宣言
// Initialize / Update / Draw / Finalize の純粋仮想関数を定義する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"

using Microsoft::WRL::ComPtr;

class LIGHT;

class GAME_OBJECT
{
protected:
    // ─── DirectX リソース
    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    // ─── 状態
    bool m_bActive = true;
    bool m_bVisible = true;

public:
    virtual ~GAME_OBJECT() = default;

    // ─── ライフサイクル
    virtual bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) = 0;
    virtual void Update(float deltaSeconds = 0.0f) {}
    virtual void Draw() = 0;
    virtual void Finalize() = 0;

    // ─── ImGui
    virtual void DrawImGui() {}

    // ─── ライト注入
    virtual void SetLight(LIGHT* pLight) {}

    // ─── 描画バリアント（反射パス用）
    virtual void DrawWithViewProj(const MATRIX4X4& viewProj, const VECTOR3& eye)
    {
        Draw();
    }

    // ─── セッター
    void SetActive(bool bActive) { m_bActive = bActive; }
    void SetVisible(bool bVisible) { m_bVisible = bVisible; }

    // ─── ゲッター
    bool IsActive()  const { return m_bActive; }
    bool IsVisible() const { return m_bVisible; }
};