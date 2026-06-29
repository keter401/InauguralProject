// ============================================================
// Cube.h
// PBR マテリアル対応の単位キューブ描画クラスの宣言
// アルベド・法線・メタリックラフネスマップと IBL テクスチャに対応する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <memory>
#include <DirectXMath.h>

#include "GameObject.h"
#include "Camera.h"
#include "Light.h"
#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"
#include "Math/Vertex.h"
#include "Math/ConstantBuffers.h"
#include "Resource/Texture.h"
#include "Shader/Shader.h"

using Microsoft::WRL::ComPtr;

class CUBE : public GAME_OBJECT
{
private:
    // ─── GPU リソース
    ComPtr<ID3D11Buffer> m_pVertexBuffer;
    ComPtr<ID3D11Buffer> m_pIndexBuffer;
    UINT                 m_indexCount = 0;

    ComPtr<ID3D11Buffer> m_pConstantBuffer;
    ComPtr<ID3D11Buffer> m_pCbPbr;
    CB_WVP               m_cbData = {};
    CB_PBR               m_cbPbr = {};

    // ─── シェーダーリスト
    std::vector<SHADER*> m_shaders;

    // ─── 依存オブジェクト
    CAMERA* m_pCamera = nullptr;
    LIGHT* m_pLight = nullptr;

    // ─── 変換
    VECTOR3     m_position = { 0.0f, 0.0f, 0.0f };
    VECTOR3     m_rotation = { 0.0f, 0.0f, 0.0f };
    VECTOR3     m_scale = { 1.0f, 1.0f, 1.0f };
    std::string m_name = "PBR_Cube";

    // ─── テクスチャ
    std::unique_ptr<TEXTURE> m_albedoTexture;
    std::unique_ptr<TEXTURE> m_normalMapTexture;
    std::unique_ptr<TEXTURE> m_metallicRoughnessTexture;

    // ─── IBL SRV（外部から注入）
    ID3D11ShaderResourceView* m_pIblDiffuseSrv = nullptr;
    ID3D11ShaderResourceView* m_pIblSpecularSrv = nullptr;
    ID3D11ShaderResourceView* m_pBrdfLutSrv = nullptr;

    // ─── 内部ヘルパー
    void      DrawInternal(const MATRIX4X4* pOverrideViewProj, const VECTOR3* pOverrideEye);
    bool      CreateGeometry();
    bool      InitConstantBuffer();
    MATRIX4X4 CalcWorldMatrix() const;

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) override;
    void Update(float deltaSeconds = 0) override;
    void Draw()      override;
    void Finalize()  override {}
    void DrawImGui() override;
    void DrawWithViewProj(const MATRIX4X4& viewProj, const VECTOR3& eye) override;
    void DrawShadow(const MATRIX4X4& cubeViewProj, SHADER* pShadowCubeShader);

    // ─── セッター（依存オブジェクト）
    void SetCamera(CAMERA* pCamera) { m_pCamera = pCamera; }
    void SetLight(LIGHT* pLight) override { m_pLight = pLight; }

    // ─── セッター（変換）
    void SetPosition(const VECTOR3& position) { m_position = position; }
    void SetRotation(const VECTOR3& rotation) { m_rotation = rotation; }
    void SetScale(const VECTOR3& scale) { m_scale = scale; }

    // ─── セッター（PBR パラメータ）
    void SetMetallic(float metallic) { m_cbPbr.metallic = metallic; }
    void SetRoughness(float roughness) { m_cbPbr.roughness = roughness; }
    void SetIblIntensity(float intensity) { m_cbPbr.iblIntensity = intensity; }

    // ─── セッター（IBL / テクスチャ SRV）
    void SetIblDiffuseSrv(ID3D11ShaderResourceView* pSrv) { m_pIblDiffuseSrv = pSrv; }
    void SetIblSpecularSrv(ID3D11ShaderResourceView* pSrv) { m_pIblSpecularSrv = pSrv; }
    void SetBrdfLutSrv(ID3D11ShaderResourceView* pSrv) { m_pBrdfLutSrv = pSrv; }

    // ─── ゲッター
    const VECTOR3& GetPosition() const { return m_position; }
    const VECTOR3& GetRotation() const { return m_rotation; }
    const VECTOR3& GetScale()    const { return m_scale; }
    const std::string& GetName()     const { return m_name; }
    const std::vector<SHADER*>& GetShaders()  const { return m_shaders; }

    // ─── シェーダー管理
    bool AddShader(SHADER* pShader);
    void RemoveShader(size_t index);
    void ClearShaders() { m_shaders.clear(); }

    // ─── テクスチャロード
    void LoadPbrTextures(
        const std::string& albedoPath,
        const std::string& normalPath,
        const std::string& metallicRoughnessPath);
};