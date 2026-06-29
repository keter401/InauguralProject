// ============================================================
// Light.h
// ポイント・ディレクショナル・スポット・エリアを管理するライトクラスの宣言
// ギズモ描画とキューブシャドウマップに対応する 
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <string>

#include "GameObject.h"
#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"
#include "Math/ConstantBuffers.h"
#include "Shader/Shader.h"

using Microsoft::WRL::ComPtr;

enum class LIGHT_TYPE : int
{
    POINT = 0,
    DIRECTIONAL = 1,
    SPOT = 2,
    AREA = 3,
};

class LIGHT : public GAME_OBJECT
{
private:
    // ─── ギズモ生成
    void CreateSphereWire(float radius, int segments);
    void CreateCrossMarker(float size);
    void CreateArrow(float length);
    void CreateConeWire(float length, float angleDeg, int segments);
    void CreateAreaRect(float width, float height);

    // ─── ギズモ描画
    void DrawPointGizmo();
    void DrawDirectionalGizmo();
    void DrawSpotGizmo();
    void DrawAreaGizmo();
    void DrawGizmoMesh(
        ID3D11Buffer* pVb,
        UINT                      vertexCount,
        const MATRIX4X4& world,
        const DirectX::XMFLOAT4& color,
        D3D11_PRIMITIVE_TOPOLOGY  topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    // ─── キューブシャドウ行列更新
    void UpdateCubeShadowMatrices();

    // ─── ライトプロパティ
    std::string        m_name           = "Light";
    LIGHT_TYPE         m_type           = LIGHT_TYPE::POINT;
    VECTOR3            m_position       = { 0.0f,  3.0f, 0.0f };
    VECTOR3            m_direction      = { 0.0f, -1.0f, 0.0f };
    DirectX::XMFLOAT3  m_color          = { 1.0f,  1.0f, 1.0f };
    float              m_intensity      = 1.0f;
    float              m_range          = 10.0f;
    float              m_spotAngleDeg   = 30.0f;
    float              m_spotFalloff    = 0.2f;
    float              m_areaWidth      = 1.0f;
    float              m_areaHeight     = 1.0f;
    bool               m_bShowGizmo     = true;

    // ─── 定数バッファ
    ComPtr<ID3D11Buffer> m_pCb;
    CB_LIGHT_DATA        m_cbData = {};

    // ─── ギズモ GPU リソース
    ComPtr<ID3D11Buffer> m_pSphereVb; UINT m_sphereVertexCount = 0;
    ComPtr<ID3D11Buffer> m_pCrossVb;  UINT m_crossVertexCount  = 0;
    ComPtr<ID3D11Buffer> m_pArrowVb;  UINT m_arrowVertexCount  = 0;
    ComPtr<ID3D11Buffer> m_pConeVb;   UINT m_coneVertexCount   = 0;
    ComPtr<ID3D11Buffer> m_pAreaVb;   UINT m_areaVertexCount   = 0;
    ComPtr<ID3D11Buffer> m_pGizmoCb;

    // ─── 依存オブジェクト
    SHADER* m_pGizmoShader = nullptr;
    class CAMERA* m_pCamera = nullptr;

    // ─── キューブシャドウ行列
    MATRIX4X4 m_cubeView[6];
    MATRIX4X4 m_cubeProj;

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) override;
    void Update(float deltaSeconds = 0) override;
    void Draw()     override;
    void Finalize() override {}

    // ─── ImGui
    void DrawImGui()                            override {}
    void DrawImGuiParams(const std::string& label);

    // ─── セッター（依存オブジェクト）
    void SetCamera(class CAMERA* pCamera) { m_pCamera = pCamera; }
    void SetGizmoShader(SHADER* pShader)  { m_pGizmoShader = pShader; }
    void SetName(const std::string& name) { m_name = name; }

    // ─── セッター（ライトプロパティ）
    void SetType(LIGHT_TYPE type)               { m_type = type; }
    void SetPosition(const VECTOR3& position)   { m_position = position; }
    void SetDirection(const VECTOR3& direction) { m_direction = direction; }
    void SetColor(float r, float g, float b)    { m_color = { r, g, b }; }
    void SetIntensity(float intensity)          { m_intensity = intensity; }
    void SetRange(float range)                  { m_range = range; }
    void SetSpotAngle(float angleDeg)           { m_spotAngleDeg = angleDeg; }
    void SetAreaSize(float width, float height) { m_areaWidth = width; m_areaHeight = height; }

    // ─── ゲッター
    const std::string& GetName()    const { return m_name; }
    LIGHT_TYPE         GetType()    const { return m_type; }
    const VECTOR3& GetPosition()    const { return m_position; }
    float              GetRange()   const { return m_range; }
    CB_LIGHT_DATA      GetCbData()  const { return m_cbData; }

    // ─── GPU バインド
    void Bind(UINT slot = 1);

    // ─── キューブシャドウ行列取得
    const MATRIX4X4& GetCubeView(UINT face)     const { return m_cubeView[face]; }
    const MATRIX4X4& GetCubeProjection()        const { return m_cubeProj; }
};