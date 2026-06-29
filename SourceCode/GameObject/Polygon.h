// ============================================================
// Polygon.h
// 汎用ポリゴン描画クラスの宣言
// 頂点・インデックスバッファを持ち FIELD / MIRROR の基底クラスになる
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

#include "GameObject.h"
#include "Camera.h"
#include "Light.h"
#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"
#include "Math/Vertex.h"
#include "Math/ConstantBuffers.h"
#include "Shader/Shader.h"

using Microsoft::WRL::ComPtr;

class POLYGON : public GAME_OBJECT
{
private:
    // ─── メッシュデータ
    std::vector<VERTEX_3D> m_vertices;
    std::vector<UINT>      m_indices;

    // ─── GPU リソース
    ComPtr<ID3D11Buffer> m_pVertexBuffer;
    ComPtr<ID3D11Buffer> m_pIndexBuffer;
    UINT                 m_indexCount = 0;

    ComPtr<ID3D11Buffer> m_pConstantBuffer;
    CB_WVP               m_cbData = {};

    // ─── 依存オブジェクト
    SHADER* m_pShader = nullptr;
    CAMERA* m_pCamera = nullptr;
    LIGHT*  m_pLight  = nullptr;

protected:
    // ─── 変換・状態
    VECTOR3 m_position    = { 0.0f, 0.0f, 0.0f };
    VECTOR3 m_scale       = { 1.0f, 1.0f, 1.0f };
    VECTOR3 m_overrideEye = { 0.0f, 0.0f, 0.0f };

    // ─── 内部描画
    void DrawInternal(const MATRIX4X4* pOverrideViewProj);

    // ─── 仮想フック（サブクラスでオーバーライド）
    virtual MATRIX4X4 CalcWorldMatrix()              const;
    virtual void      SetupMaterialCb(CB_WVP& cbData) const {}
    virtual void      BindExtraCb()                   const {}

    // ─── バッファ管理
    bool InitConstantBuffer();
    bool BuildBuffers();

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) override;
    void Update(float deltaSeconds = 0) override {}
    void Draw()     override;
    void Finalize() override {}

    // ─── 描画バリアント
    void DrawWithViewProj(const MATRIX4X4& viewProj, const VECTOR3& eye) override;
    void DrawShadow(const MATRIX4X4& cubeViewProj, SHADER* pShadowCubeShader);

    // ─── メッシュ設定
    void SetMeshData(const std::vector<VERTEX_3D>& vertices, const std::vector<UINT>& indices);

    // ─── セッター
    void SetCamera(CAMERA* pCamera) { m_pCamera = pCamera; }
    void SetLight(LIGHT* pLight) override { m_pLight = pLight; }
    void SetShader(SHADER* pShader) { m_pShader = pShader; }
    void SetPosition(const VECTOR3& position) { m_position = position; }
    void SetScale(const VECTOR3& scale) { m_scale = scale; }

    // ─── ゲッター
    const VECTOR3& GetPosition() const { return m_position; }
    const VECTOR3& GetScale()    const { return m_scale; }
    CAMERA*        GetCamera()   const { return m_pCamera; }
    LIGHT*         GetLight()    const { return m_pLight; }
};