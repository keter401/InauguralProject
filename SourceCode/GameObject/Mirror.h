// ============================================================
// Mirror.h
// 平面鏡反射クラスの宣言
// POLYGON を継承し反射テクスチャへの描画と鏡面へのマッピングを担う
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include "Polygon.h"
#include "Resource/RenderTexture.h"
#include "Camera.h"
#include <memory>
#include <vector>

class GAME_OBJECT;

struct MIRROR_REFLECTION_CONTEXT
{
    CAMERA* pCamera = nullptr;
    std::vector<GAME_OBJECT*> drawables;

    ID3D11RenderTargetView* pMainRtv = nullptr;
    ID3D11DepthStencilView* pMainDsv = nullptr;
    UINT                      mainWidth = 0;
    UINT                      mainHeight = 0;
};

class MIRROR : public POLYGON
{
private:
    // ─── 定数
    static const UINT REFLECTION_WIDTH = 1280;
    static const UINT REFLECTION_HEIGHT = 720;

    // ─── 回転・スケール
    VECTOR3 m_rotationAxis = { 0.0f, 1.0f, 0.0f };
    float   m_rotationAngleDeg = 0.0f;
    VECTOR3 m_scale = { 1.0f, 1.0f, 1.0f };

    // ─── 反射テクスチャ
    std::unique_ptr<RENDER_TEXTURE>  m_renderTexture;
    ID3D11ShaderResourceView* m_pReflectionSrv = nullptr;
    float                            m_reflectionWidth = 0.0f;
    float                            m_reflectionHeight = 0.0f;

    // ─── ミラー定数バッファ
    mutable ComPtr<ID3D11Buffer> m_pMirrorCb;

    // ─── 内部ヘルパー
    void BuildMesh();
    void RestoreMainTarget(const MIRROR_REFLECTION_CONTEXT& context);

protected:
    // ─── 仮想フックのオーバーライド
    MATRIX4X4 CalcWorldMatrix()               const override;
    void      SetupMaterialCb(CB_WVP& cbData) const override;
    void      BindExtraCb()                   const override;

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) override;
    void Update(float deltaSeconds = 0) override {}
    void DrawImGui() override;

    // ─── ミラー反射パス実行
    void ExecuteReflectionPass(const MIRROR_REFLECTION_CONTEXT& context);

    // ─── セッター
    void SetRotationAxis(const VECTOR3& axis) { m_rotationAxis = axis; }
    void SetRotationAngleDeg(float angleDeg) { m_rotationAngleDeg = angleDeg; }
    void SetScale(const VECTOR3& scale) { m_scale = scale; }

    // ─── ゲッター
    const VECTOR3& GetRotationAxis()     const { return m_rotationAxis; }
    float          GetRotationAngleDeg() const { return m_rotationAngleDeg; }
    const VECTOR3& GetScale()            const { return m_scale; }

    // ─── 反射計算
    MATRIX4X4 CalcReflectionView(
        const VECTOR3& realEye,
        const VECTOR3& realTarget,
        const VECTOR3& realUp) const;
    VECTOR3 GetWorldNormal()                            const;
    VECTOR3 CalcReflectionEye(const VECTOR3& cameraPos) const;
};