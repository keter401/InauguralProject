// ============================================================
// Cube.cpp
// PBR マテリアル対応の単位キューブ描画クラスの実装
// アルベド・法線・メタリックラフネスマップと IBL テクスチャに対応する
// 制作者：Chan WingChung
// ============================================================

#include "Cube.h"
#include "Utility/Imgui/imgui.h"
#include <DirectXMath.h>

// ------------------------------------------------------------
// Initialize
// ジオメトリと定数バッファを生成して PBR デフォルト値を設定する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool CUBE::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;

    if (!CreateGeometry())     return false;
    if (!InitConstantBuffer()) return false;

    // ─── PBR パラメーターのデフォルト値を設定する
    m_cbPbr.metallic = 0.0f;
    m_cbPbr.roughness = 0.5f;
    m_cbPbr.useMetallicRoughnessMap = 0;
    m_cbPbr.iblIntensity = 1.0f;

    return true;
}

// ------------------------------------------------------------
// Update
// 毎フレームの更新処理（現在は処理なし）
// 引数：なし
// ------------------------------------------------------------
void CUBE::Update(float deltaSeconds)
{

}

// ------------------------------------------------------------
// CreateGeometry
// 6 面分の頂点データとインデックスデータを生成して GPU バッファを作成する
// 引数：なし
// ------------------------------------------------------------
bool CUBE::CreateGeometry()
{
    // ─── 各面を 4 頂点で定義する（位置・法線・UV・接線・スムーズ法線）
    std::vector<VERTEX_3D> lVertices =
    {
        { -1, 1, 1,  0, 0, 1,  0,0,  1,0,0,  0,0,1 },
        {  1, 1, 1,  0, 0, 1,  1,0,  1,0,0,  0,0,1 },
        {  1,-1, 1,  0, 0, 1,  1,1,  1,0,0,  0,0,1 },
        { -1,-1, 1,  0, 0, 1,  0,1,  1,0,0,  0,0,1 },

        {  1, 1,-1,  0, 0,-1,  0,0, -1,0,0,  0,0,-1 },
        { -1, 1,-1,  0, 0,-1,  1,0, -1,0,0,  0,0,-1 },
        { -1,-1,-1,  0, 0,-1,  1,1, -1,0,0,  0,0,-1 },
        {  1,-1,-1,  0, 0,-1,  0,1, -1,0,0,  0,0,-1 },

        {  1, 1, 1,  1, 0, 0,  0,0,  0,0,-1,  1,0,0 },
        {  1, 1,-1,  1, 0, 0,  1,0,  0,0,-1,  1,0,0 },
        {  1,-1,-1,  1, 0, 0,  1,1,  0,0,-1,  1,0,0 },
        {  1,-1, 1,  1, 0, 0,  0,1,  0,0,-1,  1,0,0 },

        { -1, 1,-1, -1, 0, 0,  0,0,  0,0, 1, -1,0,0 },
        { -1, 1, 1, -1, 0, 0,  1,0,  0,0, 1, -1,0,0 },
        { -1,-1, 1, -1, 0, 0,  1,1,  0,0, 1, -1,0,0 },
        { -1,-1,-1, -1, 0, 0,  0,1,  0,0, 1, -1,0,0 },

        { -1, 1,-1,  0, 1, 0,  0,0,  1,0,0,  0,1,0 },
        {  1, 1,-1,  0, 1, 0,  1,0,  1,0,0,  0,1,0 },
        {  1, 1, 1,  0, 1, 0,  1,1,  1,0,0,  0,1,0 },
        { -1, 1, 1,  0, 1, 0,  0,1,  1,0,0,  0,1,0 },

        { -1,-1, 1,  0,-1, 0,  0,0,  1,0,0,  0,-1,0 },
        {  1,-1, 1,  0,-1, 0,  1,0,  1,0,0,  0,-1,0 },
        {  1,-1,-1,  0,-1, 0,  1,1,  1,0,0,  0,-1,0 },
        { -1,-1,-1,  0,-1, 0,  0,1,  1,0,0,  0,-1,0 },
    };

    // ─── 6 面 × 2 三角形のインデックスを生成する
    std::vector<UINT> lIndices;
    for (UINT lF = 0; lF < 6; lF++)
    {
        UINT lBase = lF * 4;
        lIndices.push_back(lBase + 0);
        lIndices.push_back(lBase + 1);
        lIndices.push_back(lBase + 2);
        lIndices.push_back(lBase + 0);
        lIndices.push_back(lBase + 2);
        lIndices.push_back(lBase + 3);
    }
    m_indexCount = static_cast<UINT>(lIndices.size());

    // ─── 頂点バッファを生成する
    D3D11_BUFFER_DESC lVbDesc = {};
    lVbDesc.ByteWidth = static_cast<UINT>(sizeof(VERTEX_3D) * lVertices.size());
    lVbDesc.Usage = D3D11_USAGE_DEFAULT;
    lVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA lVbData = {};
    lVbData.pSysMem = lVertices.data();

    if (FAILED(m_pDevice->CreateBuffer(&lVbDesc, &lVbData, &m_pVertexBuffer)))
        return false;

    // ─── インデックスバッファを生成する
    D3D11_BUFFER_DESC lIbDesc = {};
    lIbDesc.ByteWidth = static_cast<UINT>(sizeof(UINT) * lIndices.size());
    lIbDesc.Usage = D3D11_USAGE_DEFAULT;
    lIbDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA lIbData = {};
    lIbData.pSysMem = lIndices.data();

    if (FAILED(m_pDevice->CreateBuffer(&lIbDesc, &lIbData, &m_pIndexBuffer)))
        return false;

    return true;
}

// ------------------------------------------------------------
// InitConstantBuffer
// WVP 用と PBR 用の定数バッファを生成する
// 引数：なし
// ------------------------------------------------------------
bool CUBE::InitConstantBuffer()
{
    // ─── CB_WVP バッファを生成する（b0 スロット用）
    D3D11_BUFFER_DESC lCbDesc = {};
    lCbDesc.ByteWidth = sizeof(CB_WVP);
    lCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lCbDesc, nullptr, &m_pConstantBuffer)))
        return false;

    // ─── CB_PBR バッファを生成する（b6 スロット用）
    D3D11_BUFFER_DESC lPbrDesc = {};
    lPbrDesc.ByteWidth = sizeof(CB_PBR);
    lPbrDesc.Usage = D3D11_USAGE_DEFAULT;
    lPbrDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lPbrDesc, nullptr, &m_pCbPbr)))
        return false;

    return true;
}

// ------------------------------------------------------------
// CalcWorldMatrix
// スケール・回転・平行移動の合成ワールド行列を返す
// 引数：なし
// ------------------------------------------------------------
MATRIX4X4 CUBE::CalcWorldMatrix() const
{
    // ─── Scale * Rot * Trans の順で合成する
    MATRIX4X4 lScale = MATRIX4X4::Scale(m_scale.x, m_scale.y, m_scale.z);
    MATRIX4X4 lRot = MATRIX4X4::RotationX(m_rotation.x)
        * MATRIX4X4::RotationY(m_rotation.y)
        * MATRIX4X4::RotationZ(m_rotation.z);
    MATRIX4X4 lTrans = MATRIX4X4::Translation(m_position.x, m_position.y, m_position.z);
    return lScale * lRot * lTrans;
}

// ------------------------------------------------------------
// Draw
// 通常描画パスでキューブを描画する
// 引数：なし
// ------------------------------------------------------------
void CUBE::Draw()
{
    DrawInternal(nullptr, nullptr);
}

// ------------------------------------------------------------
// DrawWithViewProj
// 反射パス用に外部の viewProj と eye でキューブを描画する
// 引数：viewProj - 反射後のビュー × プロジェクション行列
//        eye      - 反射後のカメラ位置
// ------------------------------------------------------------
void CUBE::DrawWithViewProj(const MATRIX4X4& viewProj, const VECTOR3& eye)
{
    DrawInternal(&viewProj, &eye);
}

// ------------------------------------------------------------
// DrawShadow
// キューブをシャドウパスのキューブフェイス視点で深度のみ描画する
// 引数：cubeViewProj      - そのフェイスのビュー×プロジェクション行列
//        pShadowCubeShader - シャドウ生成シェーダー
// ------------------------------------------------------------
void CUBE::DrawShadow(const MATRIX4X4& cubeViewProj, SHADER* pShadowCubeShader)
{
    if (!pShadowCubeShader) return;

    pShadowCubeShader->Bind(m_pContext);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ─── world と WVP をキューブフェイスの視点で計算して CB に書き込む
    MATRIX4X4 lWorld = CalcWorldMatrix();
    m_cbData.world = lWorld.Transpose().m_mat;
    m_cbData.wvp = (lWorld * cubeViewProj).Transpose().m_mat;
    m_pContext->UpdateSubresource(m_pConstantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);

    // ─── VS にのみバインドする
    ID3D11Buffer* lCb = m_pConstantBuffer.Get();
    m_pContext->VSSetConstantBuffers(0, 1, &lCb);

    // ─── IA バッファをセットして描画する
    UINT lStride = sizeof(VERTEX_3D);
    UINT lOffset = 0;
    ID3D11Buffer* lVb = m_pVertexBuffer.Get();
    m_pContext->IASetVertexBuffers(0, 1, &lVb, &lStride, &lOffset);
    m_pContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_pContext->DrawIndexed(m_indexCount, 0, 0);
}

// ------------------------------------------------------------
// DrawInternal
// 全シェーダーで CB を設定してキューブを描画する内部関数
// 引数：pOverrideViewProj - 反射パス用 viewProj（nullptr なら通常カメラを使用）
//        pOverrideEye      - 反射パス用カメラ位置（nullptr なら通常カメラを使用）
// ------------------------------------------------------------
void CUBE::DrawInternal(const MATRIX4X4* pOverrideViewProj, const VECTOR3* pOverrideEye)
{
    if (!m_pCamera || m_shaders.empty()) return;

    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    MATRIX4X4 lWorld = CalcWorldMatrix();

    for (SHADER* lShader : m_shaders)
    {
        if (!lShader) continue;
        lShader->Bind(m_pContext);

        // ─── PBR キューブはカリングなしで描画する（内側からも見えるようにする）
        D3D11_RASTERIZER_DESC lRasterDesc = {};
        lRasterDesc.FillMode = D3D11_FILL_SOLID;
        lRasterDesc.CullMode = D3D11_CULL_NONE;
        lRasterDesc.DepthClipEnable = TRUE;
        ComPtr<ID3D11RasterizerState> lRasterState;
        m_pDevice->CreateRasterizerState(&lRasterDesc, &lRasterState);
        m_pContext->RSSetState(lRasterState.Get());

        // ─── CB_WVP を設定する（反射パスの場合は override の行列を使用）
        m_cbData.world = lWorld.Transpose().m_mat;
        m_cbData.wvp = pOverrideViewProj
            ? (lWorld * (*pOverrideViewProj)).Transpose().m_mat
            : m_pCamera->GetWVP(lWorld).m_mat;
        m_cbData.materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };

        m_cbData.useTexture = (m_albedoTexture && m_albedoTexture->IsLoaded()) ? 1 : 0;
        m_cbData.useNormalMap = (m_normalMapTexture && m_normalMapTexture->IsLoaded()) ? 1 : 0;

        const VECTOR3& lEye = pOverrideEye ? *pOverrideEye : m_pCamera->GetPosition();
        m_cbData.cameraPos = { lEye.x, lEye.y, lEye.z };

        m_cbData.useRimLight = 0;
        m_cbData.useToonLightTexture = 0;
        m_cbData.useToonDarkTexture = 0;

        m_pContext->UpdateSubresource(m_pConstantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);
        ID3D11Buffer* lCb = m_pConstantBuffer.Get();
        m_pContext->VSSetConstantBuffers(0, 1, &lCb);
        m_pContext->PSSetConstantBuffers(0, 1, &lCb);

        // ─── CB_PBR（メタリック・ラフネス・IBL 強度）を b6 にバインドする
        m_cbPbr.useMetallicRoughnessMap =
            (m_metallicRoughnessTexture && m_metallicRoughnessTexture->IsLoaded()) ? 1 : 0;
        m_pContext->UpdateSubresource(m_pCbPbr.Get(), 0, nullptr, &m_cbPbr, 0, 0);
        ID3D11Buffer* lPbrCb = m_pCbPbr.Get();
        m_pContext->PSSetConstantBuffers(6, 1, &lPbrCb);

        // ─── テクスチャを各スロットにバインドする
        if (m_albedoTexture && m_albedoTexture->IsLoaded())
            m_albedoTexture->Bind(0);
        if (m_normalMapTexture && m_normalMapTexture->IsLoaded())
            m_normalMapTexture->Bind(3);
        if (m_metallicRoughnessTexture && m_metallicRoughnessTexture->IsLoaded())
            m_metallicRoughnessTexture->Bind(9);

        // ─── IBL テクスチャ（t10 / t11 / t12）をバインドする
        if (m_pIblDiffuseSrv)  m_pContext->PSSetShaderResources(10, 1, &m_pIblDiffuseSrv);
        if (m_pIblSpecularSrv) m_pContext->PSSetShaderResources(11, 1, &m_pIblSpecularSrv);
        if (m_pBrdfLutSrv)     m_pContext->PSSetShaderResources(12, 1, &m_pBrdfLutSrv);

        // ─── 頂点・インデックスバッファをセットして描画する
        UINT lStride = sizeof(VERTEX_3D);
        UINT lOffset = 0;
        ID3D11Buffer* lVb = m_pVertexBuffer.Get();
        m_pContext->IASetVertexBuffers(0, 1, &lVb, &lStride, &lOffset);
        m_pContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        m_pContext->DrawIndexed(m_indexCount, 0, 0);
    }
}

// ------------------------------------------------------------
// DrawImGui
// PBR パラメーター・テクスチャ状態・IBL 状態を表示する ImGui ウィンドウを描画する
// 引数：なし
// ------------------------------------------------------------
void CUBE::DrawImGui()
{
    if (ImGui::Begin("PBR Cube"))
    {
        // ─── 変換パラメーターを編集する
        ImGui::DragFloat3("Position", &m_position.x, 0.1f);

        float lDeg[3] =
        {
            DirectX::XMConvertToDegrees(m_rotation.x),
            DirectX::XMConvertToDegrees(m_rotation.y),
            DirectX::XMConvertToDegrees(m_rotation.z)
        };
        if (ImGui::DragFloat3("Rotation (deg)", lDeg, 1.0f))
        {
            m_rotation.x = DirectX::XMConvertToRadians(lDeg[0]);
            m_rotation.y = DirectX::XMConvertToRadians(lDeg[1]);
            m_rotation.z = DirectX::XMConvertToRadians(lDeg[2]);
        }
        ImGui::DragFloat3("Scale", &m_scale.x, 0.01f, 0.001f, 100.0f);

        ImGui::Separator();

        // ─── PBR マテリアルパラメーターを編集する
        ImGui::Text("PBR Parameters");
        ImGui::SliderFloat("Metallic", &m_cbPbr.metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &m_cbPbr.roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("IBL Intensity", &m_cbPbr.iblIntensity, 0.0f, 5.0f);

        ImGui::Separator();

        // ─── テクスチャのロード状態を読み取り専用で表示する
        ImGui::Text("Textures");

        auto lShowTex = [](const char* lLabel, const std::unique_ptr<TEXTURE>& lTex)
            {
                ImGui::Text("%s:", lLabel);
                ImGui::SameLine();
                if (lTex && lTex->IsLoaded())
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "(Loaded)");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "(Not Found)");
            };

        lShowTex("Albedo", m_albedoTexture);
        lShowTex("Normal Map", m_normalMapTexture);
        lShowTex("Metallic/Roughness", m_metallicRoughnessTexture);

        ImGui::Separator();

        // ─── IBL SRV のバインド状態を読み取り専用で表示する
        ImGui::Text("IBL");
        ImGui::Text("Diffuse SRV:");  ImGui::SameLine();
        ImGui::TextColored(
            m_pIblDiffuseSrv ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
            m_pIblDiffuseSrv ? "(Ready)" : "(Not Set)");

        ImGui::Text("Specular SRV:"); ImGui::SameLine();
        ImGui::TextColored(
            m_pIblSpecularSrv ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
            m_pIblSpecularSrv ? "(Ready)" : "(Not Set)");

        ImGui::Text("BRDF LUT:");     ImGui::SameLine();
        ImGui::TextColored(
            m_pBrdfLutSrv ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
            m_pBrdfLutSrv ? "(Ready)" : "(Not Set)");
    }
    ImGui::End();
}

// ------------------------------------------------------------
// AddShader
// シェーダーを重複なしでリストに追加する
// 引数：pShader - 追加するシェーダーポインタ
// ------------------------------------------------------------
bool CUBE::AddShader(SHADER* pShader)
{
    if (!pShader) return false;
    // ─── 重複チェックをしてから追加する
    for (SHADER* lExisting : m_shaders)
        if (lExisting == pShader) return false;
    m_shaders.push_back(pShader);
    return true;
}

// ------------------------------------------------------------
// RemoveShader
// 指定インデックスのシェーダーをリストから削除する
// 引数：index - 削除するシェーダーのインデックス
// ------------------------------------------------------------
void CUBE::RemoveShader(size_t index)
{
    if (index >= m_shaders.size()) return;
    m_shaders.erase(m_shaders.begin() + index);
}

// ------------------------------------------------------------
// LoadPbrTextures
// アルベド・法線マップ・メタリックラフネスマップを読み込む
// 引数：albedoPath             - アルベドテクスチャのファイルパス
//        normalPath             - 法線マップのファイルパス
//        metallicRoughnessPath  - メタリックラフネスマップのファイルパス
// ------------------------------------------------------------
void CUBE::LoadPbrTextures(
    const std::string& albedoPath,
    const std::string& normalPath,
    const std::string& metallicRoughnessPath)
{
    // ─── アルベドを sRGB で読み込む（ガンマ補正あり）
    auto lAlbedo = std::make_unique<TEXTURE>();
    if (lAlbedo->Initialize(m_pDevice, m_pContext, albedoPath))
        m_albedoTexture = std::move(lAlbedo);

    // ─── 法線マップは線形で読み込む（ガンマ補正なし）
    auto lNormal = std::make_unique<TEXTURE>();
    if (lNormal->Initialize(m_pDevice, m_pContext, normalPath, false))
        m_normalMapTexture = std::move(lNormal);

    // ─── メタリックラフネスも線形で読み込む（ガンマ補正なし）
    auto lMR = std::make_unique<TEXTURE>();
    if (lMR->Initialize(m_pDevice, m_pContext, metallicRoughnessPath, false))
        m_metallicRoughnessTexture = std::move(lMR);
}