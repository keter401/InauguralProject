// ============================================================
// Model.h
// Assimp でロードした 3D モデルを描画するクラスの宣言
// マルチシェーダー・アウトライン・トゥーン・リムライト・ディゾルブに対応する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <memory>
#include <DirectXMath.h>
#include <assimp/scene.h>

#include "GameObject.h"
#include "Camera.h"
#include "Light.h"
#include "Animation/Animator.h"
#include "Animation/AssimpConvert.h"
#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"
#include "Math/Vertex.h"
#include "Math/ConstantBuffers.h"
#include "Resource/Texture.h"
#include "Shader/Shader.h"
#include "Manager/Config.h"

using Microsoft::WRL::ComPtr;

struct MESH
{
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> skinnedVertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
    UINT                 vertexCount = 0;
    UINT                 indexCount = 0;
    MATRIX4X4            nodeTransform;
    std::string          nodeName;

    std::vector<VERTEX_3D> cpuVertices;

    std::vector<VERTEX_BONE_DATA> boneData;
    bool                          bSkinned = false;

    std::unique_ptr<TEXTURE> texture;
    std::string              texturePath;

    std::unique_ptr<TEXTURE> normalMapTexture;
    std::string              normalMapPath;
};

class MODEL : public GAME_OBJECT
{
private:
    // ─── メッシュデータ
    std::string       m_filePath;
    std::vector<MESH> m_meshes;

    // ─── 定数バッファ
    ComPtr<ID3D11Buffer> m_constantBuffer;
    CB_WVP               m_cbData = {};

    ComPtr<ID3D11Buffer> m_outlineConstantBuffer;
    CB_OUTLINE           m_outlineCbData = {};

    ComPtr<ID3D11Buffer> m_dissolveConstantBuffer;
    CB_DISSOLVE          m_dissolveCbData = {};

    ComPtr<ID3D11Buffer> m_pCbPbr;
    CB_PBR               m_cbPbr = {};

    // ─── アニメーション関連
    ANIMATOR m_animator;
    std::string m_animPath;

    // ─── スキニング GPU リソース
    ComPtr<ID3D11Buffer>      m_pCbBones;            // b8
    std::unordered_map<std::string, SHADER*> m_skinnedShaderMap;
    std::vector<MATRIX4X4>    m_bonePalette;         // 毎フレームのパレット

    // ─── シェーダーリスト
    std::vector<SHADER*> m_shaders;

    // ─── 依存オブジェクト
    CAMERA* m_pCamera = nullptr;
    LIGHT* m_pLight = nullptr;

    // ─── 変換
    VECTOR3 m_position = { 0.0f,  0.0f,  0.0f };
    VECTOR3 m_rotation = { 0.0f,  0.0f,  0.0f };
    VECTOR3 m_scale = { 0.02f, 0.02f, 0.02f };

    // ─── 識別子
    std::string m_name = "SD_UnityChan";
    std::string m_textureDir = CONFIG::MODEL_TEXTURE_DIR;

    // ─── アウトライン
    float m_outlineWidth = 0.02f;
    float m_outlineColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float m_outlineWidthScale = 1.0f;
    bool  m_bUseOutlineNormalMap = false;
    bool  m_bUseOutlineWidthMap = false;

    std::unique_ptr<TEXTURE> m_outlineNormalMapTexture;
    std::unique_ptr<TEXTURE> m_outlineWidthMapTexture;

    // ─── トゥーン
    float m_toonLightThreshold = 0.7f;
    float m_toonDarkThreshold = 0.3f;
    bool  m_bUseToonLightTexture = false;
    bool  m_bUseToonDarkTexture = false;

    std::unique_ptr<TEXTURE> m_toonLightTexture;
    std::unique_ptr<TEXTURE> m_toonDarkTexture;

    // ─── リムライト
    bool  m_bUseRimLight = false;
    float m_rimColor[3] = { 1.0f, 1.0f, 1.0f };
    float m_rimPower = 2.0f;
    float m_rimIntensity = 1.0f;

    // ─── ディゾルブ
    bool  m_bDissolveEnabled = false;
    float m_dissolveThreshold = 0.0f;
    float m_dissolveEdgeWidth = 0.05f;
    float m_dissolveEdgeColor[4] = { 1.0f, 0.4f, 0.0f, 1.0f };

    // ─── 反射パス用
    VECTOR3 m_overrideEye = { 0.0f, 0.0f, 0.0f };

    // ─── 内部ヘルパー
    bool      LoadMesh();
    bool      InitConstantBuffer();
    MATRIX4X4 CalcWorldMatrix() const;

    void ProcessNode(
        const aiNode* pNode,
        const aiMatrix4x4& parentTransform,
        const aiScene* pScene);

    void CalcSmoothNormals();

    void DrawInternal(const MATRIX4X4* pOverrideViewProj);

    void LoadOutlineTextures(const std::string& basePath);
    void LoadToonTextures(const std::string& basePath);

    bool BuildSkinnedBuffers();   // スキンVBとパレットCBを生成

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) override;
    void Update(float deltaSeconds = 0) override;
    void Draw()      override;
    void Finalize()  override {}
    void DrawImGui() override;

    // ─── 描画バリアント
    void DrawWithViewProj(const MATRIX4X4& viewProj, const VECTOR3& reflectionEye) override;
    void DrawShadow(const MATRIX4X4& cubeViewProj, SHADER* pShadowShader, SHADER* pShadowSkinnedShader);

    // ─── セッター（依存オブジェクト）
    void SetCamera(CAMERA* pCamera) { m_pCamera = pCamera; }
    void SetLight(LIGHT* pLight) override { m_pLight = pLight; }
    void SetFilePath(const std::string& path) { m_filePath = path; }
    void SetTextureDir(const std::string& dir) { m_textureDir = dir; }
    void SetName(const std::string& name) { m_name = name; }

    // ─── セッター（変換）
    void SetPosition(const VECTOR3& position) { m_position = position; }
    void SetRotation(const VECTOR3& rotation) { m_rotation = rotation; }
    void SetScale(const VECTOR3& scale) { m_scale = scale; }

    // ─── セッター（アウトライン）
    void SetOutlineWidth(float width) { m_outlineWidth = width; }
    void SetUseOutlineNormalMap(bool bUse) { m_bUseOutlineNormalMap = bUse; }
    void SetUseOutlineWidthMap(bool bUse) { m_bUseOutlineWidthMap = bUse; }
    void SetOutlineWidthScale(float scale) { m_outlineWidthScale = scale; }

    // ─── セッター（トゥーン）
    void SetToonLightThreshold(float threshold) { m_toonLightThreshold = threshold; }
    void SetToonDarkThreshold(float threshold) { m_toonDarkThreshold = threshold; }
    void SetUseToonLightTexture(bool bUse) { m_bUseToonLightTexture = bUse; }
    void SetUseToonDarkTexture(bool bUse) { m_bUseToonDarkTexture = bUse; }

    // ─── セッター（リムライト）
    void SetUseRimLight(bool bUse) { m_bUseRimLight = bUse; }
    void SetRimColor(float r, float g, float b) { m_rimColor[0] = r; m_rimColor[1] = g; m_rimColor[2] = b; }
    void SetRimPower(float power) { m_rimPower = power; }
    void SetRimIntensity(float intensity) { m_rimIntensity = intensity; }

    // ─── セッター（ディゾルブ）
    void SetDissolveThreshold(float threshold) { m_dissolveThreshold = threshold; }
    void SetDissolveEdgeWidth(float width) { m_dissolveEdgeWidth = width; }
    void SetDissolveEdgeColor(float r, float g, float b, float a)
    {
        m_dissolveEdgeColor[0] = r; m_dissolveEdgeColor[1] = g;
        m_dissolveEdgeColor[2] = b; m_dissolveEdgeColor[3] = a;
    }

    // ─── セッター（アニメーション）
    void SetAnimationPath(const std::string& path) { m_animPath = path; }
    void SetSkinnedShaderFor(const std::string& baseName, SHADER* pSkinned) { m_skinnedShaderMap[baseName] = pSkinned; }

    // ─── ゲッター
    const VECTOR3& GetPosition()                        const { return m_position; }
    const VECTOR3& GetRotation()                        const { return m_rotation; }
    const VECTOR3& GetScale()                           const { return m_scale; }
    const std::string& GetName()                        const { return m_name; }
    const std::vector<SHADER*>& GetShaders()            const { return m_shaders; }
    float                       GetOutlineWidth()       const { return m_outlineWidth; }
    float*                      GetOutlineColorPtr()          { return m_outlineColor; }
    float                       GetOutlineWidthScale()  const { return m_outlineWidthScale; }
    bool                        IsUseOutlineNormalMap() const { return m_bUseOutlineNormalMap; }
    bool                        IsUseOutlineWidthMap()  const { return m_bUseOutlineWidthMap; }
    float                       GetToonLightThreshold() const { return m_toonLightThreshold; }
    float                       GetToonDarkThreshold()  const { return m_toonDarkThreshold; }
    bool                        IsUseToonLightTexture() const { return m_bUseToonLightTexture; }
    bool                        IsUseToonDarkTexture()  const { return m_bUseToonDarkTexture; }
    bool                        IsUseRimLight()         const { return m_bUseRimLight; }

    // ─── シェーダー管理
    bool AddShader(SHADER* pShader);
    void RemoveShader(size_t index);
    void ClearShaders() { m_shaders.clear(); }
};