// ============================================================
// Model.cpp
// Assimp でロードした 3D モデルを描画するクラスの実装
// マルチシェーダー・スムーズ法線・各種エフェクト CB に対応する
// 制作者：Chan WingChung
// ============================================================

#include "Model.h"
#include "Imgui/imgui.h"
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <unordered_map>
#include <cmath>
#include <cstdio>

#pragma comment(lib, "d3dcompiler.lib")

// ------------------------------------------------------------
// AddBoneInfluence
// 1頂点のボーンデータの空きスロットに (番号, 重み) を書き込む
// ------------------------------------------------------------
static void AddBoneInfluence(VERTEX_BONE_DATA& data, int boneIndex, float weight)
{
    // ─── 重み0の空きへ入れる（LimitBoneWeights で4本以内が保証されている）
    for (int lI = 0; lI < MAX_BONE_INFLUENCE; lI++)
    {
        if (data.boneWeights[lI] == 0.0f)
        {
            data.boneIndices[lI] = static_cast<uint32_t>(boneIndex);
            data.boneWeights[lI] = weight;
            return;
        }
    }
}

// ------------------------------------------------------------
// Initialize
// デバイスを保持して定数バッファとメッシュを初期化する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool MODEL::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;
    if (!InitConstantBuffer()) return false;
    if (!LoadMesh())           return false;
    return true;
}

// ------------------------------------------------------------
// Update
// 毎フレームの更新処理（現在は処理なし・将来のアニメーション用）
// 引数：なし
// ------------------------------------------------------------
void MODEL::Update(float deltaSeconds)
{
    // ─── アニメーションを進める
    m_animator.Update(deltaSeconds);
    m_animator.ComputePalette(m_bonePalette);
}

// ------------------------------------------------------------
// Draw
// 通常描画パスで DrawInternal を呼び出す
// 引数：なし
// ------------------------------------------------------------
void MODEL::Draw()
{
    DrawInternal(nullptr);
}

// ------------------------------------------------------------
// DrawWithViewProj
// 反射パス用に外部の viewProj と Eye でモデルを描画する
// 引数：viewProj      - 反射後のビュー × プロジェクション行列
//        reflectionEye - 反射後のカメラ位置
// ------------------------------------------------------------
void MODEL::DrawWithViewProj(const MATRIX4X4& viewProj, const VECTOR3& reflectionEye)
{
    // ─── 反射 Eye を保存してから内部描画関数を呼び出す
    m_overrideEye = reflectionEye;
    DrawInternal(&viewProj);
}

// ------------------------------------------------------------
// DrawInternal
// 全シェーダーで全メッシュをマルチパス描画する内部関数
// 引数：pOverrideViewProj - 反射パス用 viewProj（nullptr なら通常カメラを使用）
// ------------------------------------------------------------
void MODEL::DrawInternal(const MATRIX4X4* pOverrideViewProj)
{
    if (!m_pCamera) return;

    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    MATRIX4X4 lWorld = CalcWorldMatrix();

    // ─── シェーダーループ：1 シェーダー = 1 パス、全メッシュを描画する
    for (SHADER* lShader : m_shaders)
    {
        if (!lShader) continue;
        lShader->Bind(m_pContext);

        // ─── アウトライン CB が必要な場合は b3 にバインドする
        bool lNeedsOutlineCb = lShader->RequiresExtraCb(SHADER::EXTRA_CB_OUTLINE);
        if (lNeedsOutlineCb)
        {
            bool lUseOutlineNormalMap = m_bUseOutlineNormalMap
                && m_outlineNormalMapTexture && m_outlineNormalMapTexture->IsLoaded();
            bool lUseOutlineWidthMap = m_bUseOutlineWidthMap
                && m_outlineWidthMapTexture && m_outlineWidthMapTexture->IsLoaded();

            MATRIX4X4 lViewProj = pOverrideViewProj
                ? *pOverrideViewProj
                : (m_pCamera->GetViewMatrix() * m_pCamera->GetProjectionMatrix());

            m_outlineCbData.viewProj = lViewProj.Transpose().m_mat;
            m_outlineCbData.outlineWidth = m_outlineWidth;
            m_outlineCbData.outlineColor = { m_outlineColor[0], m_outlineColor[1], m_outlineColor[2], m_outlineColor[3] };
            m_outlineCbData.useOutlineNormalMap = lUseOutlineNormalMap ? 1 : 0;
            m_outlineCbData.useOutlineWidthMap = lUseOutlineWidthMap ? 1 : 0;
            m_outlineCbData.outlineWidthScale = m_outlineWidthScale;
            m_pContext->UpdateSubresource(m_outlineConstantBuffer.Get(), 0, nullptr, &m_outlineCbData, 0, 0);

            ID3D11Buffer* lOutlineCb = m_outlineConstantBuffer.Get();
            m_pContext->VSSetConstantBuffers(3, 1, &lOutlineCb);
            m_pContext->PSSetConstantBuffers(3, 1, &lOutlineCb);

            if (m_outlineNormalMapTexture && m_outlineNormalMapTexture->IsLoaded())
                m_outlineNormalMapTexture->BindVS(4);
            if (m_outlineWidthMapTexture && m_outlineWidthMapTexture->IsLoaded())
                m_outlineWidthMapTexture->BindVS(5);
        }

        // ─── ディゾルブ CB が必要な場合は b5 にバインドする
        if (m_bDissolveEnabled)
        {
            m_dissolveCbData.dissolveThreshold = m_dissolveThreshold;
            m_dissolveCbData.edgeWidth = m_dissolveEdgeWidth;
            m_dissolveCbData.dissolveEnabled = 1;
            m_dissolveCbData.edgeColor = { m_dissolveEdgeColor[0], m_dissolveEdgeColor[1],
                                                    m_dissolveEdgeColor[2], m_dissolveEdgeColor[3] };
            m_pContext->UpdateSubresource(m_dissolveConstantBuffer.Get(), 0, nullptr, &m_dissolveCbData, 0, 0);

            ID3D11Buffer* lDissolveCb = m_dissolveConstantBuffer.Get();
            m_pContext->PSSetConstantBuffers(5, 1, &lDissolveCb);
        }

        // ─── PBR CB を b6 にバインドする
        m_pContext->UpdateSubresource(m_pCbPbr.Get(), 0, nullptr, &m_cbPbr, 0, 0);
        ID3D11Buffer* lPbrCb = m_pCbPbr.Get();
        m_pContext->PSSetConstantBuffers(6, 1, &lPbrCb);

        // ─── トゥーンテクスチャを t6 / t7 にバインドする
        if (m_toonLightTexture && m_toonLightTexture->IsLoaded()) m_toonLightTexture->Bind(6);
        if (m_toonDarkTexture && m_toonDarkTexture->IsLoaded())  m_toonDarkTexture->Bind(7);

        // ─── メッシュループ：このシェーダーで全メッシュを 1 パス描画する
        for (auto& lMesh : m_meshes)
        {
            if (lMesh.texture)          lMesh.texture->Bind(0);
            if (lMesh.normalMapTexture) lMesh.normalMapTexture->Bind(3);


            // ─── スキンメッシュ：今のパスに対応するスキン版シェーダーで描く
            SHADER* lSkinnedForPass = nullptr;
            {
                auto lIt = m_skinnedShaderMap.find(lShader->GetName());
                if (lIt != m_skinnedShaderMap.end())
                    lSkinnedForPass = lIt->second;
            }

            if (lMesh.bSkinned && lSkinnedForPass)
            {
                // ─── (B) world = モデル配置のみ（nodeTransform 抜き）
                MATRIX4X4 lWorldOnly = CalcWorldMatrix();
                m_cbData.world = lWorldOnly.Transpose().m_mat;
                m_cbData.wvp = pOverrideViewProj
                    ? (lWorldOnly * (*pOverrideViewProj)).Transpose().m_mat
                    : m_pCamera->GetWVP(lWorldOnly).m_mat;

                // ─── マテリアル/トゥーン/リム等のパラメータも設定（PSが参照するため）
                m_cbData.materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                m_cbData.useTexture = lMesh.texture ? 1 : 0;
                m_cbData.useNormalMap = lMesh.normalMapTexture ? 1 : 0;
                if (m_pCamera)
                {
                    const VECTOR3& lEye = pOverrideViewProj ? m_overrideEye : m_pCamera->GetPosition();
                    m_cbData.cameraPos = { lEye.x, lEye.y, lEye.z };
                }
                m_cbData.toonLightThreshold = m_toonLightThreshold;
                m_cbData.toonDarkThreshold = m_toonDarkThreshold;
                m_cbData.useToonLightTexture = (m_bUseToonLightTexture && m_toonLightTexture && m_toonLightTexture->IsLoaded()) ? 1 : 0;
                m_cbData.useToonDarkTexture = (m_bUseToonDarkTexture && m_toonDarkTexture && m_toonDarkTexture->IsLoaded()) ? 1 : 0;
                m_cbData.rimColor = { m_rimColor[0], m_rimColor[1], m_rimColor[2] };
                m_cbData.rimPower = m_rimPower;
                m_cbData.rimIntensity = m_rimIntensity;
                m_cbData.useRimLight = m_bUseRimLight ? 1 : 0;

                // ─── ボーンパレットを CB_BONES に転送（b8）
                CB_BONES lBoneCb = {};
                int lCount = static_cast<int>(std::min(m_bonePalette.size(), (size_t)MAX_BONES_CB));
                for (int lB = 0; lB < lCount; lB++)
                    lBoneCb.bones[lB] = m_bonePalette[lB].Transpose().m_mat;
                m_pContext->UpdateSubresource(m_pCbBones.Get(), 0, nullptr, &lBoneCb, 0, 0);
                ID3D11Buffer* lBcb = m_pCbBones.Get();
                m_pContext->VSSetConstantBuffers(8, 1, &lBcb);

                m_pContext->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);
                ID3D11Buffer* lCb = m_constantBuffer.Get();
                m_pContext->VSSetConstantBuffers(0, 1, &lCb);
                m_pContext->PSSetConstantBuffers(0, 1, &lCb);

                // ─── スキン版シェーダーに差し替えて描画（入力レイアウトも Bind が設定）
                lSkinnedForPass->Bind(m_pContext);

                UINT lStride = sizeof(VERTEX_SKINNED);
                UINT lOffset = 0;
                ID3D11Buffer* lVb = lMesh.skinnedVertexBuffer.Get();
                m_pContext->IASetVertexBuffers(0, 1, &lVb, &lStride, &lOffset);
                m_pContext->IASetIndexBuffer(lMesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
                m_pContext->DrawIndexed(lMesh.indexCount, 0, 0);

                // ─── 次のメッシュへ進む前に、パスのシェーダーを bind し直す
                //     （次が非スキンメッシュのとき、元のシェーダーに戻すため）
                lShader->Bind(m_pContext);
                continue;
            }

            // ─── 追従ノードの現在ポーズがあれば、それを使って頭などに追従させる
            MATRIX4X4 lNodeCurrent;
            MATRIX4X4 lEffectiveWorld;
            if (m_animator.GetNodeGlobal(lMesh.nodeName, lNodeCurrent))
                lEffectiveWorld = lNodeCurrent * lWorld;   // アニメ追従
            else
                lEffectiveWorld = lMesh.nodeTransform * lWorld;   // 静止

            m_cbData.world = lEffectiveWorld.Transpose().m_mat;
            m_cbData.wvp = pOverrideViewProj
                ? (lEffectiveWorld * (*pOverrideViewProj)).Transpose().m_mat
                : m_pCamera->GetWVP(lEffectiveWorld).m_mat;

            m_cbData.materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            m_cbData.useTexture = lMesh.texture ? 1 : 0;
            m_cbData.useNormalMap = lMesh.normalMapTexture ? 1 : 0;

            // ─── 反射パスでは反射 Eye を、通常描画では実カメラ位置を使用する
            if (pOverrideViewProj)
            {
                m_cbData.cameraPos = { m_overrideEye.x, m_overrideEye.y, m_overrideEye.z };
            }
            else
            {
                const VECTOR3& lEye = m_pCamera->GetPosition();
                m_cbData.cameraPos = { lEye.x, lEye.y, lEye.z };
            }

            m_cbData.toonLightThreshold = m_toonLightThreshold;
            m_cbData.toonDarkThreshold = m_toonDarkThreshold;
            m_cbData.useToonLightTexture = (m_bUseToonLightTexture && m_toonLightTexture && m_toonLightTexture->IsLoaded()) ? 1 : 0;
            m_cbData.useToonDarkTexture = (m_bUseToonDarkTexture && m_toonDarkTexture && m_toonDarkTexture->IsLoaded()) ? 1 : 0;

            m_cbData.rimColor = { m_rimColor[0], m_rimColor[1], m_rimColor[2] };
            m_cbData.rimPower = m_rimPower;
            m_cbData.rimIntensity = m_rimIntensity;
            m_cbData.useRimLight = m_bUseRimLight ? 1 : 0;

            m_pContext->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);
            ID3D11Buffer* lCb = m_constantBuffer.Get();
            m_pContext->VSSetConstantBuffers(0, 1, &lCb);
            m_pContext->PSSetConstantBuffers(0, 1, &lCb);

            UINT lStride = sizeof(VERTEX_3D);
            UINT lOffset = 0;
            ID3D11Buffer* lVb = lMesh.vertexBuffer.Get();
            m_pContext->IASetVertexBuffers(0, 1, &lVb, &lStride, &lOffset);
            m_pContext->IASetIndexBuffer(lMesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            m_pContext->DrawIndexed(lMesh.indexCount, 0, 0);
        }
    }
}

// ------------------------------------------------------------
// DrawShadow
// キューブシャドウマップ生成パスでモデルを描画する
// スキンメッシュはボーンパレットで、ボーンを持たないメッシュ（顔など）は
// 追従ノードの現在ポーズで変形し、影もアニメーションに追従させる
// 引数：cubeViewProj         - キューブ1フェイス分のビュー×プロジェクション行列
//        pShadowShader        - 静的／ノード追従メッシュ用シャドウ生成シェーダー
//        pShadowSkinnedShader - スキンメッシュ用シャドウ生成シェーダー
// ------------------------------------------------------------
void MODEL::DrawShadow(
    const MATRIX4X4& cubeViewProj,
    SHADER* pShadowShader,
    SHADER* pShadowSkinnedShader)
{
    if (!pShadowShader) return;

    // ─── ディゾルブ CB をセットしてシャドウ形状に反映する（b5）
    m_dissolveCbData.dissolveThreshold = m_dissolveThreshold;
    m_dissolveCbData.edgeWidth  = m_dissolveEdgeWidth;
    m_dissolveCbData.dissolveEnabled = m_bDissolveEnabled ? 1 : 0;
    m_dissolveCbData.edgeColor = { m_dissolveEdgeColor[0], m_dissolveEdgeColor[1],
                                            m_dissolveEdgeColor[2], m_dissolveEdgeColor[3] };
    m_pContext->UpdateSubresource(m_dissolveConstantBuffer.Get(), 0, nullptr, &m_dissolveCbData, 0, 0);
    ID3D11Buffer* lDissolveCb = m_dissolveConstantBuffer.Get();
    m_pContext->PSSetConstantBuffers(5, 1, &lDissolveCb);

    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    MATRIX4X4 lWorld = CalcWorldMatrix();

    // ─── 全メッシュをこのフェイスの視点から描画する
    for (auto& lMesh : m_meshes)
    {
        // ─── スキンメッシュ：ボーンパレットで変形してから影を焼く
        if (lMesh.bSkinned && pShadowSkinnedShader)
        {
            // ─── world はモデル配置のみ（nodeTransform 抜き、本体描画と同じ規約）
            m_cbData.world = lWorld.Transpose().m_mat;
            m_cbData.wvp = (lWorld * cubeViewProj).Transpose().m_mat;
            m_pContext->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);
            ID3D11Buffer* lCb = m_constantBuffer.Get();
            m_pContext->VSSetConstantBuffers(0, 1, &lCb);
            m_pContext->PSSetConstantBuffers(0, 1, &lCb);

            // ─── ボーンパレットを CB_BONES に転送する（b8）
            CB_BONES lBoneCb = {};
            int lCount = static_cast<int>(std::min(m_bonePalette.size(), (size_t)MAX_BONES_CB));
            for (int lB = 0; lB < lCount; lB++)
                lBoneCb.bones[lB] = m_bonePalette[lB].Transpose().m_mat;
            m_pContext->UpdateSubresource(m_pCbBones.Get(), 0, nullptr, &lBoneCb, 0, 0);
            ID3D11Buffer* lBcb = m_pCbBones.Get();
            m_pContext->VSSetConstantBuffers(8, 1, &lBcb);

            // ─── スキン版シャドウシェーダーを Bind（入力レイアウトも設定される）
            pShadowSkinnedShader->Bind(m_pContext);

            UINT lStride = sizeof(VERTEX_SKINNED);
            UINT lOffset = 0;
            ID3D11Buffer* lVb = lMesh.skinnedVertexBuffer.Get();
            m_pContext->IASetVertexBuffers(0, 1, &lVb, &lStride, &lOffset);
            m_pContext->IASetIndexBuffer(lMesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            m_pContext->DrawIndexed(lMesh.indexCount, 0, 0);
            continue;
        }

        // ─── 非スキンメッシュ：追従ノードがあれば現在ポーズ（顔など）、無ければ静止
        MATRIX4X4 lNodeCurrent;
        MATRIX4X4 lEffectiveWorld;
        if (m_animator.GetNodeGlobal(lMesh.nodeName, lNodeCurrent))
            lEffectiveWorld = lNodeCurrent * lWorld;          // アニメ追従
        else
            lEffectiveWorld = lMesh.nodeTransform * lWorld;   // 静止

        m_cbData.world = lEffectiveWorld.Transpose().m_mat;
        m_cbData.wvp = (lEffectiveWorld * cubeViewProj).Transpose().m_mat;
        m_pContext->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);
        ID3D11Buffer* lCb = m_constantBuffer.Get();
        m_pContext->VSSetConstantBuffers(0, 1, &lCb);
        m_pContext->PSSetConstantBuffers(0, 1, &lCb);

        pShadowShader->Bind(m_pContext);

        UINT lStride = sizeof(VERTEX_3D);
        UINT lOffset = 0;
        ID3D11Buffer* lVb = lMesh.vertexBuffer.Get();
        m_pContext->IASetVertexBuffers(0, 1, &lVb, &lStride, &lOffset);
        m_pContext->IASetIndexBuffer(lMesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        m_pContext->DrawIndexed(lMesh.indexCount, 0, 0);
    }
}

// ------------------------------------------------------------
// AddShader
// シェーダーを重複なしでリストに追加する
// 引数：pShader - 追加するシェーダーポインタ
// ------------------------------------------------------------
bool MODEL::AddShader(SHADER* pShader)
{
    if (!pShader) return false;

    // ─── 重複チェックをしてから追加する
    for (SHADER* lExisting : m_shaders)
    {
        if (lExisting == pShader)
            return false;
    }

    m_shaders.push_back(pShader);
    return true;
}

// ------------------------------------------------------------
// RemoveShader
// 指定インデックスのシェーダーをリストから削除する
// 引数：index - 削除するシェーダーのインデックス
// ------------------------------------------------------------
void MODEL::RemoveShader(size_t index)
{
    if (index >= m_shaders.size()) return;
    m_shaders.erase(m_shaders.begin() + index);
}

// ------------------------------------------------------------
// DrawImGui
// モデルの変換・各エフェクトパラメーターを編集する ImGui ウィンドウを描画する
// 引数：なし
// ------------------------------------------------------------
void MODEL::DrawImGui()
{
    if (ImGui::Begin("Model"))
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

        if (ImGui::CollapsingHeader("Outline"))
        {
            ImGui::DragFloat("Width", &m_outlineWidth, 0.001f, 0.0f, 1.0f);
            ImGui::ColorEdit4("Color", m_outlineColor);

            ImGui::Spacing();
            ImGui::Checkbox("Use Outline Normal Map", &m_bUseOutlineNormalMap);
            ImGui::SameLine();
            if (m_outlineNormalMapTexture && m_outlineNormalMapTexture->IsLoaded())
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "(Loaded)");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "(Not Found - Fallback)");

            ImGui::Checkbox("Use Outline Width Map", &m_bUseOutlineWidthMap);
            ImGui::SameLine();
            if (m_outlineWidthMapTexture && m_outlineWidthMapTexture->IsLoaded())
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "(Loaded)");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "(Not Found - Fallback)");

            ImGui::DragFloat("Width Scale", &m_outlineWidthScale, 0.01f, 0.0f, 5.0f);
        }

        if (ImGui::CollapsingHeader("Toon Shader"))
        {
            ImGui::SliderFloat("Light Threshold", &m_toonLightThreshold, 0.0f, 1.0f);
            ImGui::SliderFloat("Dark Threshold", &m_toonDarkThreshold, 0.0f, 1.0f);

            // ─── Light Threshold は Dark Threshold より常に大きく保つ
            if (m_toonLightThreshold < m_toonDarkThreshold)
                m_toonLightThreshold = m_toonDarkThreshold;

                ImGui::Spacing();
                ImGui::Checkbox("Use Toon Light Texture", &m_bUseToonLightTexture);
                ImGui::SameLine();
                if (m_toonLightTexture && m_toonLightTexture->IsLoaded())
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "(Loaded)");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "(Not Found - Fallback)");

                ImGui::Checkbox("Use Toon Dark Texture", &m_bUseToonDarkTexture);
                ImGui::SameLine();
                if (m_toonDarkTexture && m_toonDarkTexture->IsLoaded())
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "(Loaded)");
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "(Not Found - Fallback)");
        }

        if (ImGui::CollapsingHeader("Rim Light"))
        {
            ImGui::Checkbox("Enable Rim Light", &m_bUseRimLight);
            ImGui::ColorEdit3("Rim Color", m_rimColor);
            ImGui::DragFloat("Rim Power", &m_rimPower, 0.1f, 0.1f, 20.0f);
            ImGui::DragFloat("Rim Intensity", &m_rimIntensity, 0.01f, 0.0f, 5.0f);
        }

        if (ImGui::CollapsingHeader("Dissolve"))
        {
            ImGui::Checkbox("Enable Dissolve", &m_bDissolveEnabled);
            ImGui::SliderFloat("Threshold", &m_dissolveThreshold, 0.0f, 1.0f);
            ImGui::DragFloat("Edge Width", &m_dissolveEdgeWidth, 0.001f, 0.0f, 1.0f);
            ImGui::ColorEdit4("Edge Color", m_dissolveEdgeColor);
        }

        if (ImGui::CollapsingHeader("PBR"))
        {
            ImGui::SliderFloat("Metallic", &m_cbPbr.metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &m_cbPbr.roughness, 0.0f, 1.0f);
        }

        // ─── アニメーション再生コントロール
        m_animator.DrawImGui();
    }
    ImGui::End();
}

// ------------------------------------------------------------
// BuildSkinnedBuffers
// スキンメッシュごとに VERTEX_SKINNED のVBを生成する
// （CalcSmoothNormals 後に呼ぶ：smoothNormal が確定済み）
// 引数：なし
// ------------------------------------------------------------
bool MODEL::BuildSkinnedBuffers()
{
    for (auto& lMesh : m_meshes)
    {
        if (!lMesh.bSkinned) continue;

        // ─── VERTEX_3D + boneData を VERTEX_SKINNED に詰め直す
        std::vector<VERTEX_SKINNED> lSkinned(lMesh.cpuVertices.size());
        for (size_t lV = 0; lV < lMesh.cpuVertices.size(); lV++)
        {
            const VERTEX_3D& lSrc = lMesh.cpuVertices[lV];
            const VERTEX_BONE_DATA& lBd = lMesh.boneData[lV];
            VERTEX_SKINNED& lDst = lSkinned[lV];

            lDst.x = lSrc.x; lDst.y = lSrc.y; lDst.z = lSrc.z;
            lDst.normalX = lSrc.normalX; lDst.normalY = lSrc.normalY; lDst.normalZ = lSrc.normalZ;
            lDst.u = lSrc.u; lDst.v = lSrc.v;
            lDst.tangentX = lSrc.tangentX; lDst.tangentY = lSrc.tangentY; lDst.tangentZ = lSrc.tangentZ;
            lDst.smoothNormalX = lSrc.smoothNormalX; lDst.smoothNormalY = lSrc.smoothNormalY; lDst.smoothNormalZ = lSrc.smoothNormalZ;
            for (int lI = 0; lI < MAX_BONE_INFLUENCE; lI++)
            {
                lDst.boneIndices[lI] = lBd.boneIndices[lI];
                lDst.boneWeights[lI] = lBd.boneWeights[lI];
            }
        }

        // ─── スキン用VBを生成する
        D3D11_BUFFER_DESC lVbDesc = {};
        lVbDesc.ByteWidth = sizeof(VERTEX_SKINNED) * static_cast<UINT>(lSkinned.size());
        lVbDesc.Usage = D3D11_USAGE_DEFAULT;
        lVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA lVbData = { lSkinned.data() };
        if (FAILED(m_pDevice->CreateBuffer(&lVbDesc, &lVbData, &lMesh.skinnedVertexBuffer)))
            return false;
    }
    return true;
}

// ------------------------------------------------------------
// LoadMesh
// Assimp でメッシュを読み込みスムーズ法線とテクスチャを生成する
// 引数：なし
// ------------------------------------------------------------
bool MODEL::LoadMesh()
{
    Assimp::Importer lImporter;

    // ─── 左手系変換・法線生成・接線計算を含む読み込みフラグを設定する
    const aiScene* lScene = lImporter.ReadFile(
        m_filePath,
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_LimitBoneWeights);

    if (!lScene || !lScene->mRootNode)
    {
        OutputDebugStringA("[Model] Assimpのファイル読み込みに失敗\n");
        OutputDebugStringA(lImporter.GetErrorString());
        return false;
    }

    // ─── 骨格を ANIMATOR に構築させる（階層は ANIMATOR が所有）
    m_animator.BuildSkeleton(lScene->mRootNode);

    // ─── ノードを再帰的に走査してメッシュを構築する
    ProcessNode(lScene->mRootNode, aiMatrix4x4(), lScene);

    // ─── ボーンを骨格ノードへ名前でひも付ける
    m_animator.LinkBones();

    // ─── モーションFBXを読み込む（パスが設定されていれば）
    if (!m_animPath.empty())
        m_animator.LoadClip(m_animPath);

    // ─── メッシュをまたいでスムーズ法線を計算してGPUに転送する
    CalcSmoothNormals();
    BuildSkinnedBuffers();

    // ─── アウトライン・トゥーン・ディゾルブ用テクスチャを読み込む
    if (!m_meshes.empty() && !m_meshes[0].texturePath.empty())
    {
        const std::string& lDiffusePath = m_meshes[0].texturePath;
        size_t lDotPos = lDiffusePath.find_last_of('.');
        std::string lBasePath = (lDotPos != std::string::npos)
            ? lDiffusePath.substr(0, lDotPos)
            : lDiffusePath;

        LoadOutlineTextures(lBasePath);
        LoadToonTextures(lBasePath);
    }

    {
        char lDbg[128];
        sprintf_s(lDbg, "[Skeletal] skeleton built / bones registered via ANIMATOR\n");
        OutputDebugStringA(lDbg);
    }

    return true;
}

// ------------------------------------------------------------
// ProcessNode
// Assimp のノードを再帰的に走査してメッシュを構築する
// 引数：pNode           - 処理するノード
//        parentTransform - 親ノードの累積変換行列
//        pScene          - Assimp シーンデータ
// ------------------------------------------------------------
void MODEL::ProcessNode(
    const aiNode* pNode,
    const aiMatrix4x4& parentTransform,
    const aiScene* pScene)
{
    // ─── 親の変換行列と自分の変換行列を合成してグローバル変換を求める
    aiMatrix4x4 lGlobalTransform = parentTransform * pNode->mTransformation;

    for (unsigned int lMeshIdx = 0; lMeshIdx < pNode->mNumMeshes; lMeshIdx++)
    {
        const aiMesh* lAiMesh = pScene->mMeshes[pNode->mMeshes[lMeshIdx]];
        MESH lMesh;

        lMesh.nodeTransform = ConvertAiMatrix(lGlobalTransform);
        lMesh.nodeName = pNode->mName.C_Str();

        // ─── 頂点データ（位置・法線・UV・接線）を VERTEX_3D に変換する
        std::vector<VERTEX_3D> lVertices;
        lVertices.reserve(lAiMesh->mNumVertices);
        for (unsigned int lVtxIdx = 0; lVtxIdx < lAiMesh->mNumVertices; lVtxIdx++)
        {
            VERTEX_3D lVtx = {};
            lVtx.x = lAiMesh->mVertices[lVtxIdx].x;
            lVtx.y = lAiMesh->mVertices[lVtxIdx].y;
            lVtx.z = lAiMesh->mVertices[lVtxIdx].z;

            if (lAiMesh->HasNormals())
            {
                lVtx.normalX = lAiMesh->mNormals[lVtxIdx].x;
                lVtx.normalY = lAiMesh->mNormals[lVtxIdx].y;
                lVtx.normalZ = lAiMesh->mNormals[lVtxIdx].z;
            }
            if (lAiMesh->HasTextureCoords(0))
            {
                lVtx.u = lAiMesh->mTextureCoords[0][lVtxIdx].x;
                lVtx.v = lAiMesh->mTextureCoords[0][lVtxIdx].y;
            }

            if (lAiMesh->HasTangentsAndBitangents())
            {
                lVtx.tangentX = lAiMesh->mTangents[lVtxIdx].x;
                lVtx.tangentY = lAiMesh->mTangents[lVtxIdx].y;
                lVtx.tangentZ = lAiMesh->mTangents[lVtxIdx].z;
            }
            else
            {
                // ─── 接線がない場合は +X をデフォルト接線とする
                lVtx.tangentX = 1.0f;
                lVtx.tangentY = 0.0f;
                lVtx.tangentZ = 0.0f;
            }

            lVertices.push_back(lVtx);
        }

        // ─── インデックスデータを面から展開する
        std::vector<UINT> lIndices;
        lIndices.reserve(lAiMesh->mNumFaces * 3);
        for (unsigned int lFaceIdx = 0; lFaceIdx < lAiMesh->mNumFaces; lFaceIdx++)
        {
            const aiFace& lFace = lAiMesh->mFaces[lFaceIdx];
            for (unsigned int lIdxIdx = 0; lIdxIdx < lFace.mNumIndices; lIdxIdx++)
                lIndices.push_back(lFace.mIndices[lIdxIdx]);
        }

        // ─── 頂点バッファを生成する
        D3D11_BUFFER_DESC lVbDesc = {};
        lVbDesc.ByteWidth = sizeof(VERTEX_3D) * static_cast<UINT>(lVertices.size());
        lVbDesc.Usage = D3D11_USAGE_DEFAULT;
        lVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA lVbData = { lVertices.data() };
        if (FAILED(m_pDevice->CreateBuffer(&lVbDesc, &lVbData, &lMesh.vertexBuffer))) return;

        // ─── インデックスバッファを生成する
        D3D11_BUFFER_DESC lIbDesc = {};
        lIbDesc.ByteWidth = sizeof(UINT) * static_cast<UINT>(lIndices.size());
        lIbDesc.Usage = D3D11_USAGE_DEFAULT;
        lIbDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA lIbData = { lIndices.data() };
        if (FAILED(m_pDevice->CreateBuffer(&lIbDesc, &lIbData, &lMesh.indexBuffer))) return;

        // ─── スキンウェイトを読み込む（ボーンを持つメッシュのみ）
        std::vector<VERTEX_BONE_DATA> lBoneData;
        if (lAiMesh->HasBones())
        {
            lBoneData.resize(lAiMesh->mNumVertices);
            for (unsigned int lB = 0; lB < lAiMesh->mNumBones; lB++)
            {
                const aiBone* lAiBone = lAiMesh->mBones[lB];

                // ─── ボーン番号は ANIMATOR が一元採番（重複名は同じ番号を返す）
                int lBoneIndex = m_animator.RegisterBone(
                    lAiBone->mName.C_Str(),
                    ConvertAiMatrix(lAiBone->mOffsetMatrix));

                for (unsigned int lW = 0; lW < lAiBone->mNumWeights; lW++)
                {
                    unsigned int lVtx = lAiBone->mWeights[lW].mVertexId;
                    float        lWeight = lAiBone->mWeights[lW].mWeight;
                    AddBoneInfluence(lBoneData[lVtx], lBoneIndex, lWeight);
                }
            }
            lMesh.bSkinned = true;
        }

        lMesh.vertexCount = static_cast<UINT>(lVertices.size());
        lMesh.indexCount = static_cast<UINT>(lIndices.size());
        lMesh.cpuVertices = std::move(lVertices);
        lMesh.boneData = std::move(lBoneData);

        // lMesh.bSkinned を決めた直後あたりに
        char lDbg[160];
        sprintf_s(lDbg, "[MeshDiag] mesh verts=%u hasBones=%d numBones=%u\n",
            lAiMesh->mNumVertices, lAiMesh->HasBones() ? 1 : 0, lAiMesh->mNumBones);
        OutputDebugStringA(lDbg);

        // ─── マテリアルからディフューズ・法線マップのパスを取得する
        if (lAiMesh->mMaterialIndex < pScene->mNumMaterials)
        {
            const aiMaterial* lMat = pScene->mMaterials[lAiMesh->mMaterialIndex];

            aiString lAiTexPath;
            if (lMat->GetTexture(aiTextureType_DIFFUSE, 0, &lAiTexPath) == AI_SUCCESS)
            {
                std::string lRawPath = lAiTexPath.C_Str();
                size_t lSlash = lRawPath.find_last_of("/\\");
                std::string lFileName = (lSlash != std::string::npos) ? lRawPath.substr(lSlash + 1) : lRawPath;
                lMesh.texturePath = m_textureDir + lFileName;
            }

            aiString lAiNormalPath;
            if (lMat->GetTexture(aiTextureType_NORMALS, 0, &lAiNormalPath) == AI_SUCCESS)
            {
                std::string lRawNormalPath = lAiNormalPath.C_Str();
                size_t lNormalSlash = lRawNormalPath.find_last_of("/\\");
                std::string lNormalFileName = (lNormalSlash != std::string::npos) ? lRawNormalPath.substr(lNormalSlash + 1) : lRawNormalPath;
                lMesh.normalMapPath = m_textureDir + lNormalFileName;
            }
        }

        // ─── ディフューズテクスチャを読み込む（sRGB フォーマット）
        if (!lMesh.texturePath.empty())
        {
            auto lTex = std::make_unique<TEXTURE>();
            if (lTex->Initialize(m_pDevice, m_pContext, lMesh.texturePath))
            {
                lMesh.texture = std::move(lTex);
                OutputDebugStringA(("[Texture] ロード成功：" + lMesh.texturePath + "\n").c_str());
            }
            else
            {
                OutputDebugStringA(("[Texture] ロード失敗：" + lMesh.texturePath + "\n").c_str());
            }
        }
        else
        {
            OutputDebugStringA("[Texture] texturePathが空です\n");
        }

        // ─── 法線マップを読み込む（線形フォーマット・sRGB なし）
        if (!lMesh.normalMapPath.empty())
        {
            auto lNormalTex = std::make_unique<TEXTURE>();
            if (lNormalTex->Initialize(m_pDevice, m_pContext, lMesh.normalMapPath, false))
            {
                lMesh.normalMapTexture = std::move(lNormalTex);
                OutputDebugStringA(("[Texture] 法線マップロード成功：" + lMesh.normalMapPath + "\n").c_str());
            }
            else
            {
                OutputDebugStringA(("[Texture] 法線マップロード失敗：" + lMesh.normalMapPath + "\n").c_str());
            }
        }

        m_meshes.push_back(std::move(lMesh));
    }

    // ─── 子ノードを再帰的に処理する
    for (unsigned int lChildIdx = 0; lChildIdx < pNode->mNumChildren; lChildIdx++)
        ProcessNode(pNode->mChildren[lChildIdx], lGlobalTransform, pScene);
}

// ------------------------------------------------------------
// CalcSmoothNormals
// 全メッシュの同一ワールド位置の頂点法線を平均してスムーズ法線を計算する
// 引数：なし
// ------------------------------------------------------------
void MODEL::CalcSmoothNormals()
{
    const int   lPrecision = 4;
    const float lScaleForRound = std::pow(10.0f, static_cast<float>(lPrecision));

    auto lRoundKey = [lScaleForRound](float value) -> long long
        {
            return static_cast<long long>(std::round(value * lScaleForRound));
        };

    auto lMakeKey = [&](float x, float y, float z) -> std::string
        {
            return std::to_string(lRoundKey(x)) + "_" +
                std::to_string(lRoundKey(y)) + "_" +
                std::to_string(lRoundKey(z));
        };

    // ─── 同一ワールド位置の頂点を空間ハッシュでグループ化する
    struct SMOOTH_GROUP
    {
        DirectX::XMFLOAT3 normalSum = { 0.0f, 0.0f, 0.0f };
        std::vector<std::pair<size_t, size_t>> vertexRefs;
    };
    std::unordered_map<std::string, SMOOTH_GROUP> lGroups;

    std::vector<DirectX::XMMATRIX> lNodeXmCache;
    std::vector<DirectX::XMMATRIX> lNodeInverseCache;
    lNodeXmCache.reserve(m_meshes.size());
    lNodeInverseCache.reserve(m_meshes.size());

    // ─── 全メッシュの頂点をワールド空間に変換してグループに積算する
    for (size_t lMeshIdx = 0; lMeshIdx < m_meshes.size(); lMeshIdx++)
    {
        const MESH& lMesh = m_meshes[lMeshIdx];

        DirectX::XMMATRIX lNodeXm = lMesh.nodeTransform.ToXMMatrix();
        DirectX::XMMATRIX lNodeInverse = DirectX::XMMatrixInverse(nullptr, lNodeXm);
        lNodeXmCache.push_back(lNodeXm);
        lNodeInverseCache.push_back(lNodeInverse);

        for (size_t lVtxIdx = 0; lVtxIdx < lMesh.cpuVertices.size(); lVtxIdx++)
        {
            const VERTEX_3D& lVtx = lMesh.cpuVertices[lVtxIdx];

            DirectX::XMVECTOR lLocalPos = DirectX::XMVectorSet(lVtx.x, lVtx.y, lVtx.z, 1.0f);
            DirectX::XMVECTOR lLocalNormal = DirectX::XMVectorSet(lVtx.normalX, lVtx.normalY, lVtx.normalZ, 0.0f);

            DirectX::XMVECTOR lWorldPos = DirectX::XMVector4Transform(lLocalPos, lNodeXm);
            DirectX::XMVECTOR lWorldNormal = DirectX::XMVector3Normalize(DirectX::XMVector4Transform(lLocalNormal, lNodeXm));

            DirectX::XMFLOAT3 lWorldPosF, lWorldNormalF;
            DirectX::XMStoreFloat3(&lWorldPosF, lWorldPos);
            DirectX::XMStoreFloat3(&lWorldNormalF, lWorldNormal);

            std::string   lKey = lMakeKey(lWorldPosF.x, lWorldPosF.y, lWorldPosF.z);
            SMOOTH_GROUP& lGroup = lGroups[lKey];
            lGroup.normalSum.x += lWorldNormalF.x;
            lGroup.normalSum.y += lWorldNormalF.y;
            lGroup.normalSum.z += lWorldNormalF.z;
            lGroup.vertexRefs.push_back({ lMeshIdx, lVtxIdx });
        }
    }

    // ─── グループごとに法線を平均してローカル空間に逆変換する
    for (auto& lPair : lGroups)
    {
        SMOOTH_GROUP& lGroup = lPair.second;

        DirectX::XMVECTOR lSumVec = DirectX::XMVectorSet(lGroup.normalSum.x, lGroup.normalSum.y, lGroup.normalSum.z, 0.0f);
        float             lLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(lSumVec));
        DirectX::XMVECTOR lSmoothWorldNormal = (lLengthSq < 1e-8f)
            ? DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
            : DirectX::XMVector3Normalize(lSumVec);

        for (auto& lRef : lGroup.vertexRefs)
        {
            size_t lMeshIdx = lRef.first;
            size_t lVtxIdx = lRef.second;

            // ─── Transpose 行列で近似的にローカル空間へ逆変換する
            DirectX::XMVECTOR lLocalSmoothNormal = DirectX::XMVector3Normalize(
                DirectX::XMVector4Transform(lSmoothWorldNormal, lNodeInverseCache[lMeshIdx]));

            DirectX::XMFLOAT3 lLocalSmoothNormalF;
            DirectX::XMStoreFloat3(&lLocalSmoothNormalF, lLocalSmoothNormal);

            VERTEX_3D& lVtx = m_meshes[lMeshIdx].cpuVertices[lVtxIdx];
            lVtx.smoothNormalX = lLocalSmoothNormalF.x;
            lVtx.smoothNormalY = lLocalSmoothNormalF.y;
            lVtx.smoothNormalZ = lLocalSmoothNormalF.z;
        }
    }

    // ─── スムーズ法線を含む頂点データを GPU バッファに転送する
    for (auto& lMesh : m_meshes)
    {
        m_pContext->UpdateSubresource(
            lMesh.vertexBuffer.Get(), 0, nullptr,
            lMesh.cpuVertices.data(), 0, 0);
    }

    // ─── ノード変換にスケールが含まれる場合は精度低下を警告する
    for (size_t lMeshIdx = 0; lMeshIdx < m_meshes.size(); lMeshIdx++)
    {
        DirectX::XMVECTOR lScaleVec, lRotQuat, lTransVec;
        if (DirectX::XMMatrixDecompose(&lScaleVec, &lRotQuat, &lTransVec, lNodeXmCache[lMeshIdx]))
        {
            DirectX::XMFLOAT3 lScaleF;
            DirectX::XMStoreFloat3(&lScaleF, lScaleVec);
            const float lTolerance = 0.01f;
            bool lHasScale =
                std::abs(lScaleF.x - 1.0f) > lTolerance ||
                std::abs(lScaleF.y - 1.0f) > lTolerance ||
                std::abs(lScaleF.z - 1.0f) > lTolerance;
            if (lHasScale)
            {
                char lMsg[256];
                sprintf_s(lMsg,
                    "[Model] 警告：メッシュ[%zu]のnodeTransformにスケール(%.3f, %.3f, %.3f)が含まれています。"
                    "SmoothNormalの逆変換近似（Transpose）が不正確になる可能性があります。\n",
                    lMeshIdx, lScaleF.x, lScaleF.y, lScaleF.z);
                OutputDebugStringA(lMsg);
            }
        }
    }
}

// ------------------------------------------------------------
// LoadOutlineTextures
// アウトライン法線マップと太さマップを basePath を基に読み込む
// 引数：basePath - テクスチャファイルの拡張子前のパス
// ------------------------------------------------------------
void MODEL::LoadOutlineTextures(const std::string& basePath)
{
    // ─── アウトライン法線マップを読み込む（ガンマ補正なし）
    const std::string lNormalMapPath = basePath + "_outline_normal.png";
    {
        auto lTex = std::make_unique<TEXTURE>();
        if (lTex->Initialize(m_pDevice, m_pContext, lNormalMapPath, false))
        {
            m_outlineNormalMapTexture = std::move(lTex);
            OutputDebugStringA(("[Texture] アウトライン法線マップロード成功：" + lNormalMapPath + "\n").c_str());
        }
        else
        {
            OutputDebugStringA(("[Texture] アウトライン法線マップが見つかりません（フォールバックします）：" + lNormalMapPath + "\n").c_str());
        }
    }

    // ─── アウトライン太さマップを読み込む（ガンマ補正なし）
    const std::string lWidthMapPath = basePath + "_outline_width_map.png";
    {
        auto lTex = std::make_unique<TEXTURE>();
        if (lTex->Initialize(m_pDevice, m_pContext, lWidthMapPath, false))
        {
            m_outlineWidthMapTexture = std::move(lTex);
            OutputDebugStringA(("[Texture] アウトライン太さマップロード成功：" + lWidthMapPath + "\n").c_str());
        }
        else
        {
            OutputDebugStringA(("[Texture] アウトライン太さマップが見つかりません（フォールバックします）：" + lWidthMapPath + "\n").c_str());
        }
    }
}

// ------------------------------------------------------------
// LoadToonTextures
// トゥーン用のライトテクスチャとダークテクスチャを basePath を基に読み込む
// 引数：basePath - テクスチャファイルの拡張子前のパス
// ------------------------------------------------------------
void MODEL::LoadToonTextures(const std::string& basePath)
{
    // ─── ライト面テクスチャを読み込む（sRGB フォーマット）
    const std::string lLightPath = basePath + "_light.png";
    {
        auto lTex = std::make_unique<TEXTURE>();
        if (lTex->Initialize(m_pDevice, m_pContext, lLightPath))
        {
            m_toonLightTexture = std::move(lTex);
            OutputDebugStringA(("[Texture] トゥーンlightテクスチャロード成功：" + lLightPath + "\n").c_str());
        }
        else
        {
            OutputDebugStringA(("[Texture] トゥーンlightテクスチャが見つかりません（中間トーンのみになります）：" + lLightPath + "\n").c_str());
        }
    }

    // ─── ダーク面テクスチャを読み込む（sRGB フォーマット）
    const std::string lDarkPath = basePath + "_dark.png";
    {
        auto lTex = std::make_unique<TEXTURE>();
        if (lTex->Initialize(m_pDevice, m_pContext, lDarkPath))
        {
            m_toonDarkTexture = std::move(lTex);
            OutputDebugStringA(("[Texture] トゥーンdarkテクスチャロード成功：" + lDarkPath + "\n").c_str());
        }
        else
        {
            OutputDebugStringA(("[Texture] トゥーンdarkテクスチャが見つかりません（中間トーンのみになります）：" + lDarkPath + "\n").c_str());
        }
    }
}

// ------------------------------------------------------------
// InitConstantBuffer
// WVP・アウトライン・ディゾルブ・PBR の各定数バッファを生成する
// 引数：なし
// ------------------------------------------------------------
bool MODEL::InitConstantBuffer()
{
    // ─── WVP + マテリアル CB を生成する（b0）
    D3D11_BUFFER_DESC lCbDesc = {};
    lCbDesc.ByteWidth = sizeof(CB_WVP);
    lCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lCbDesc, nullptr, &m_constantBuffer)))
        return false;

    // ─── アウトライン CB を生成する（b3）
    D3D11_BUFFER_DESC lOutlineCbDesc = {};
    lOutlineCbDesc.ByteWidth = sizeof(CB_OUTLINE);
    lOutlineCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lOutlineCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lOutlineCbDesc, nullptr, &m_outlineConstantBuffer)))
        return false;

    // ─── ディゾルブ CB を生成する（b5）
    D3D11_BUFFER_DESC lDissolveCbDesc = {};
    lDissolveCbDesc.ByteWidth = sizeof(CB_DISSOLVE);
    lDissolveCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lDissolveCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lDissolveCbDesc, nullptr, &m_dissolveConstantBuffer)))
        return false;

    // ─── PBR CB を生成する（b6）
    D3D11_BUFFER_DESC lPbrCbDesc = {};
    lPbrCbDesc.ByteWidth = sizeof(CB_PBR);
    lPbrCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lPbrCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lPbrCbDesc, nullptr, &m_pCbPbr)))
        return false;

    // ─── ボーンパレット CB を生成する（b8）
    D3D11_BUFFER_DESC lBoneCbDesc = {};
    lBoneCbDesc.ByteWidth = sizeof(CB_BONES);
    lBoneCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lBoneCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lBoneCbDesc, nullptr, &m_pCbBones)))
        return false;

    return true;
}

// ------------------------------------------------------------
// CalcWorldMatrix
// スケール・XYZ 回転・平行移動の合成ワールド行列を返す
// 引数：なし
// ------------------------------------------------------------
MATRIX4X4 MODEL::CalcWorldMatrix() const
{
    // ─── Scale * Rot * Trans の順で合成する
    MATRIX4X4 lScale = MATRIX4X4::Scale(m_scale.x, m_scale.y, m_scale.z);
    MATRIX4X4 lRot = MATRIX4X4::RotationX(m_rotation.x)
        * MATRIX4X4::RotationY(m_rotation.y)
        * MATRIX4X4::RotationZ(m_rotation.z);
    MATRIX4X4 lTrans = MATRIX4X4::Translation(m_position.x, m_position.y, m_position.z);
    return lScale * lRot * lTrans;
}