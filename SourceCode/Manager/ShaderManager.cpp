// ============================================================
// ShaderManager.cpp
// プロジェクトで使用する全シェーダーを管理するクラスの実装
// HLSL ファイルの登録・検索と ImGui によるモデルへの割り当てを担う
// 制作者：Chan WingChung
// ============================================================

#include "ShaderManager.h"
#include "GameObject/Model.h"
#include "Utility/Imgui/imgui.h"

// ------------------------------------------------------------
// Initialize
// デバイスを保持してシェーダーを一括登録する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool SHADER_MANAGER::Initialize(
    ID3D11Device* pDevice,
    ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;

    // ─── 全シェーダーをコンパイルして登録する
    return RegisterShaders();
}

// ------------------------------------------------------------
// RegisterShaders
// プロジェクトで使用する全シェーダーをコンパイルして登録する
// 引数：なし
// ------------------------------------------------------------
bool SHADER_MANAGER::RegisterShaders()
{
    // ─── Basic（3D 基本描画・位置・法線・UV）
    auto lModelShader = std::make_unique<SHADER>();
    if (!lModelShader->Initialize(
        m_pDevice,
        L"Shaders/Basic3DVS.hlsl",
        L"Shaders/Basic3DPS.hlsl",
        "Basic"))
        return false;
    m_shaders.push_back(std::move(lModelShader));

    // ─── Phong（フォン反射モデルによるライティング）
    auto lPhongShader = std::make_unique<SHADER>();
    if (!lPhongShader->Initialize(
        m_pDevice,
        L"Shaders/PhongVS.hlsl",
        L"Shaders/PhongPS.hlsl",
        "Phong"))
        return false;
    m_shaders.push_back(std::move(lPhongShader));

    // ─── Gizmo（ライトギズモ描画・位置＋カラーのみのレイアウト）
    auto lGizmoShader = std::make_unique<SHADER>();
    if (!lGizmoShader->Initialize(
        m_pDevice,
        L"Shaders/GizmoVS.hlsl",
        L"Shaders/GizmoPS.hlsl",
        "Gizmo",
        SHADER::INPUT_LAYOUT_TYPE::GIZMO))
        return false;
    m_shaders.push_back(std::move(lGizmoShader));

    // ─── Shadow（シャドウマップ生成パス用）
    auto lShadowShader = std::make_unique<SHADER>();
    if (!lShadowShader->Initialize(
        m_pDevice,
        L"Shaders/ShadowVS.hlsl",
        L"Shaders/ShadowPS.hlsl",
        "Shadow"))
        return false;
    m_shaders.push_back(std::move(lShadowShader));

    // ─── NormalMap（接線空間法線マッピング・接線ベクトル付きレイアウト）
    auto lNormalMapShader = std::make_unique<SHADER>();
    if (!lNormalMapShader->Initialize(
        m_pDevice,
        L"Shaders/NormalMapVS.hlsl",
        L"Shaders/NormalMapPS.hlsl",
        "NormalMap",
        SHADER::INPUT_LAYOUT_TYPE::MODEL_3D_TANGENT))
        return false;
    m_shaders.push_back(std::move(lNormalMapShader));

    // ─── Outline（背面押し出しアウトライン・前面カリング・アウトライン CB 必要）
    auto lOutlineShader = std::make_unique<SHADER>();
    if (!lOutlineShader->Initialize(
        m_pDevice,
        L"Shaders/OutlineVS.hlsl",
        L"Shaders/OutlinePS.hlsl",
        "Outline",
        SHADER::INPUT_LAYOUT_TYPE::OUTLINE,
        SHADER::CULL_MODE::FRONT,
        SHADER::EXTRA_CB_OUTLINE))
        return false;
    m_shaders.push_back(std::move(lOutlineShader));

    // ─── Toon（3 トーンセルシェーディング）
    auto lToonShader = std::make_unique<SHADER>();
    if (!lToonShader->Initialize(
        m_pDevice,
        L"Shaders/ToonVS.hlsl",
        L"Shaders/ToonPS.hlsl",
        "Toon"))
        return false;
    m_shaders.push_back(std::move(lToonShader));

    // ─── Mirror（平面鏡反射・ミラー CB 必要）
    auto lMirrorShader = std::make_unique<SHADER>();
    if (!lMirrorShader->Initialize(
        m_pDevice,
        L"Shaders/MirrorVS.hlsl",
        L"Shaders/MirrorPS.hlsl",
        "Mirror",
        SHADER::INPUT_LAYOUT_TYPE::MODEL_3D,
        SHADER::CULL_MODE::BACK,
        SHADER::EXTRA_CB_MIRROR))
        return false;
    m_shaders.push_back(std::move(lMirrorShader));

    // ─── PBR（物理ベースレンダリング・IBL 対応・PBR CB 必要）
    auto lPbrShader = std::make_unique<SHADER>();
    if (!lPbrShader->Initialize(
        m_pDevice,
        L"Shaders/PbrVS.hlsl",
        L"Shaders/PbrPS.hlsl",
        "PBR",
        SHADER::INPUT_LAYOUT_TYPE::MODEL_3D_TANGENT,
        SHADER::CULL_MODE::BACK,
        SHADER::EXTRA_CB_PBR))
        return false;
    m_shaders.push_back(std::move(lPbrShader));

    // ─── Skybox（正距円筒 HDR を背景キューブとして描画・カリングなし）
    auto lSkyShader = std::make_unique<SHADER>();
    if (!lSkyShader->Initialize(
        m_pDevice,
        L"Shaders/SkyboxVS.hlsl",
        L"Shaders/SkyboxPS.hlsl",
        "Skybox",
        SHADER::INPUT_LAYOUT_TYPE::MODEL_3D_TANGENT,
        SHADER::CULL_MODE::NONE))
        return false;
    m_shaders.push_back(std::move(lSkyShader));

    // スキニング（ Phong シェーダー対応のスケルトンアニメーション ）
    auto lPhongSkinned = std::make_unique<SHADER>();
    if(!lPhongSkinned->Initialize(
        m_pDevice,
        L"Shaders/PhongSkinnedVS.hlsl",
        L"Shaders/PhongPS.hlsl",
        "PhongSkinned",
        SHADER::INPUT_LAYOUT_TYPE::SKINNED,
        SHADER::CULL_MODE::BACK,
        SHADER::EXTRA_CB_NONE))
        return false;
    m_shaders.push_back(std::move(lPhongSkinned));

    // スキニング（ Toon シェーダー対応のスケルトンアニメーション ）
    auto lToonSkinned = std::make_unique<SHADER>();
    if (!lToonSkinned->Initialize(
        m_pDevice,
        L"Shaders/ToonSkinnedVS.hlsl",
        L"Shaders/ToonPS.hlsl",
        "ToonSkinned",
        SHADER::INPUT_LAYOUT_TYPE::SKINNED,
        SHADER::CULL_MODE::BACK,
        SHADER::EXTRA_CB_NONE))
        return false;
    m_shaders.push_back(std::move(lToonSkinned));

    // スキニング（ Basic シェーダー対応のスケルトンアニメーション ）
    auto lBasicSkinned = std::make_unique<SHADER>();
    if (!lBasicSkinned->Initialize(
        m_pDevice,
        L"Shaders/BasicSkinnedVS.hlsl",
        L"Shaders/Basic3DPS.hlsl",
        "BasicSkinned",
        SHADER::INPUT_LAYOUT_TYPE::SKINNED,
        SHADER::CULL_MODE::BACK,
        SHADER::EXTRA_CB_NONE))
        return false;
    m_shaders.push_back(std::move(lBasicSkinned));

    // スキニング（ 影をスケルトンアニメーションを適用する ）
    auto lShadowSkinned = std::make_unique<SHADER>();
    if (!lShadowSkinned->Initialize(
        m_pDevice,
        L"Shaders/ShadowSkinnedVS.hlsl",
        L"Shaders/ShadowPS.hlsl",
        "ShadowSkinned",
        SHADER::INPUT_LAYOUT_TYPE::SKINNED,
        SHADER::CULL_MODE::BACK,
        SHADER::EXTRA_CB_NONE))
        return false;
    m_shaders.push_back(std::move(lShadowSkinned));

    return true;
}

// ------------------------------------------------------------
// SetModelList
// ImGui での選択対象となるモデルリストを設定する
// 引数：models - シーン内の全 MODEL ポインタリスト
// ------------------------------------------------------------
void SHADER_MANAGER::SetModelList(std::vector<MODEL*> models)
{
    // ─── ムーブで受け取りコピーを避ける
    m_models = std::move(models);
}

// ------------------------------------------------------------
// DrawImGui
// シェーダーのモデルへの割り当てを操作する ImGui ウィンドウを描画する
// 引数：なし
// ------------------------------------------------------------
void SHADER_MANAGER::DrawImGui()
{
    if (ImGui::Begin("Shader Manager"))
    {
        // ─── モデルが未登録の場合は案内を表示して終了する
        if (m_models.empty())
        {
            ImGui::TextDisabled("No models registered.");
            ImGui::TextDisabled("Call SetModelList() after GameObjectManager::Initialize().");
            ImGui::End();
            return;
        }

        // ─── 対象モデルの選択コンボを表示する
        {
            std::vector<const char*> lModelNames;
            for (auto* lModel : m_models)
                lModelNames.push_back(lModel->GetName().c_str());

            ImGui::Text("Target Model");
            ImGui::Combo("##Model", &m_selectedModelIdx,
                lModelNames.data(),
                static_cast<int>(lModelNames.size()));
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ─── 追加するシェーダーの選択と追加ボタンを表示する
        {
            std::vector<const char*> lShaderNames;
            for (auto& lShader : m_shaders)
                lShaderNames.push_back(lShader->GetName().c_str());

            ImGui::Text("Shader to Add");
            ImGui::Combo("##Shader", &m_selectedShaderIdx,
                lShaderNames.data(),
                static_cast<int>(lShaderNames.size()));

            ImGui::SameLine();
            if (ImGui::Button("Add Shader"))
            {
                SHADER* lShader = m_shaders[m_selectedShaderIdx].get();
                MODEL* lTargetModel = m_models[m_selectedModelIdx];
                lTargetModel->AddShader(lShader);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ─── 選択中モデルに割り当てられたシェーダーを一覧表示して削除できるようにする
        {
            MODEL* lTargetModel = m_models[m_selectedModelIdx];
            const std::vector<SHADER*>& lAssigned = lTargetModel->GetShaders();

            ImGui::Text("Assigned Shaders (%s) : draw order = top -> bottom",
                lTargetModel->GetName().c_str());

            if (lAssigned.empty())
            {
                ImGui::TextDisabled("  (No shaders assigned)");
            }
            else
            {
                int lRemoveIdx = -1;

                for (size_t lIdx = 0; lIdx < lAssigned.size(); lIdx++)
                {
                    ImGui::PushID(static_cast<int>(lIdx));

                    ImGui::Text("  [%zu] %s", lIdx, lAssigned[lIdx]->GetName().c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove"))
                    {
                        lRemoveIdx = static_cast<int>(lIdx);
                    }

                    ImGui::PopID();
                }

                // ─── ループ後に削除することでイテレーション中の変更を避ける
                if (lRemoveIdx >= 0)
                {
                    lTargetModel->RemoveShader(static_cast<size_t>(lRemoveIdx));
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ─── 全モデルのシェーダー割り当て状況を一覧表示する
        ImGui::Text("All Models Overview:\n");
        for (auto* lModel : m_models)
        {
            const std::vector<SHADER*>& lShaders = lModel->GetShaders();

            std::string lSummary;
            if (lShaders.empty())
            {
                lSummary = "None";
            }
            else
            {
                for (size_t lIdx = 0; lIdx < lShaders.size(); lIdx++)
                {
                    if (lIdx > 0) lSummary += " + \n";
                    lSummary += lShaders[lIdx]->GetName();
                }
            }

            ImGui::Text("  %s  \n->  %s",
                lModel->GetName().c_str(), lSummary.c_str());
        }
    }
    ImGui::End();
}

// ------------------------------------------------------------
// Finalize
// 全シェーダーとモデルリストを解放する
// 引数：なし
// ------------------------------------------------------------
void SHADER_MANAGER::Finalize()
{
    // ─── シェーダーリストとモデルキャッシュをクリアする
    m_shaders.clear();
    m_models.clear();
}

// ------------------------------------------------------------
// GetByName
// 指定した名前のシェーダーポインタを返す（見つからない場合は nullptr）
// 引数：name - 検索するシェーダー名
// ------------------------------------------------------------
SHADER* SHADER_MANAGER::GetByName(const std::string& name) const
{
    // ─── 名前が一致するシェーダーを線形探索する
    for (auto& lShader : m_shaders)
    {
        if (lShader->GetName() == name)
            return lShader.get();
    }
    return nullptr;
}