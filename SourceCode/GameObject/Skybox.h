// ============================================================
// Skybox.h
// HDR 環境マップをキューブボックスとして描画するクラスの宣言
// 深度書き込みを無効にして常に最奥面に描画し背景として使用する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <memory>

#include "GameObject.h"
#include "Camera.h"
#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"
#include "Math/Vertex.h"
#include "Math/ConstantBuffers.h"
#include "Shader/Shader.h"

using Microsoft::WRL::ComPtr;

class SKY_BOX : public GAME_OBJECT
{
private:
    // ─── GPU リソース
    ComPtr<ID3D11Buffer> m_pVertexBuffer;
    ComPtr<ID3D11Buffer> m_pIndexBuffer;
    UINT                 m_indexCount = 0;

    ComPtr<ID3D11Buffer>            m_pConstantBuffer;
    ComPtr<ID3D11DepthStencilState> m_pDepthState;
    CB_WVP                          m_cbData = {};

    // ─── 依存オブジェクト
    CAMERA* m_pCamera = nullptr;
    SHADER* m_pShader = nullptr;
    ID3D11ShaderResourceView* m_pHdrSrv = nullptr;

    // ─── スカイボックスサイズ
    float m_size = 500.0f;

    // ─── 内部ヘルパー
    bool CreateGeometry();
    bool InitConstantBuffer();

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) override;
    void Update(float deltaSeconds = 0) override {}
    void Draw()      override;
    void Finalize()  override {}
    void DrawImGui() override {}
    void DrawWithViewProj(const MATRIX4X4& viewProj, const VECTOR3& eye) override;

    // ─── セッター
    void SetCamera(CAMERA* pCamera) { m_pCamera = pCamera; }
    void SetShader(SHADER* pShader) { m_pShader = pShader; }
    void SetHdrSrv(ID3D11ShaderResourceView* pSrv) { m_pHdrSrv = pSrv; }
};