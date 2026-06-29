// ============================================================
// LightManager.cpp
// シーン内の複数ライトを管理するクラスの実装
// ライトの追加・削除・更新・GPU バインドと ImGui 操作を担う
// 制作者：Chan WingChung
// ============================================================

#include "LightManager.h"
#include "Math/ConstantBuffers.h"
#include "Utility/Imgui/imgui.h"
#include <string>

// ------------------------------------------------------------
// Initialize
// ライト一覧用の定数バッファを生成する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool LIGHT_MANAGER::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;

    // ─── CB_LIGHTS（MAX_LIGHTS 灯分）の定数バッファを生成する
    D3D11_BUFFER_DESC lCbDesc = {};
    lCbDesc.ByteWidth = sizeof(CB_LIGHTS);
    lCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lCbDesc, nullptr, m_pCbLights.GetAddressOf())))
        return false;

    return true;
}

// ------------------------------------------------------------
// AddLight
// 新しいライトを生成して管理リストに追加する
// 引数：なし
// ------------------------------------------------------------
LIGHT* LIGHT_MANAGER::AddLight()
{
    auto lLight = std::make_unique<LIGHT>();

    // ─── カメラとギズモシェーダーが既に設定済みなら新ライトにも渡す
    if (m_pCamera)
        lLight->SetCamera(m_pCamera);
    if (m_pGizmoShader)
        lLight->SetGizmoShader(m_pGizmoShader);

    // ─── "Light0" "Light1" ... の形式で名前を自動生成する
    std::string lName = "Light" + std::to_string(m_lights.size());
    lLight->SetName(lName);

    LIGHT* lRawPtr = lLight.get();
    m_lights.push_back(std::move(lLight));

    // ─── デバイスが準備済みなら即座に初期化する
    if (m_pDevice)
        lRawPtr->Initialize(m_pDevice, m_pContext);

    return lRawPtr;
}

// ------------------------------------------------------------
// GetLight
// 指定インデックスのライトポインタを返す
// 引数：index - ライトリスト内のインデックス（デフォルト 0）
// ------------------------------------------------------------
LIGHT* LIGHT_MANAGER::GetLight(size_t index) const
{
    if (index >= m_lights.size()) return nullptr;
    return m_lights[index].get();
}

// ------------------------------------------------------------
// SetCamera
// カメラポインタを全ライトに伝播する
// 引数：pCamera - カメラポインタ
// ------------------------------------------------------------
void LIGHT_MANAGER::SetCamera(CAMERA* pCamera)
{
    m_pCamera = pCamera;
    // ─── 既存の全ライトにも同じカメラを設定する
    for (auto& lLight : m_lights)
        lLight->SetCamera(pCamera);
}

// ------------------------------------------------------------
// SetGizmoShader
// ギズモシェーダーを全ライトに伝播する
// 引数：pShader - ギズモ描画に使用するシェーダーポインタ
// ------------------------------------------------------------
void LIGHT_MANAGER::SetGizmoShader(SHADER* pShader)
{
    m_pGizmoShader = pShader;
    // ─── 既存の全ライトにも同じシェーダーを設定する
    for (auto& lLight : m_lights)
        lLight->SetGizmoShader(pShader);
}

// ------------------------------------------------------------
// NotifyLightRemoved
// 登録済みコールバックを通じてライト削除をゲームオブジェクトに通知する
// 引数：pNewPrimary - 削除後に使用する新しいプライマリライトポインタ
// ------------------------------------------------------------
void LIGHT_MANAGER::NotifyLightRemoved(LIGHT* pNewPrimary)
{
    // ─── GAME_OBJECT_MANAGER の RedistributeLight を呼び出す
    if (m_pCallbackFunc && m_pCallbackTarget)
        m_pCallbackFunc(m_pCallbackTarget, pNewPrimary);
}

// ------------------------------------------------------------
// FlushPendingRemove
// 遅延削除フラグが立っている場合にライトを削除する
// 引数：なし
// ------------------------------------------------------------
void LIGHT_MANAGER::FlushPendingRemove()
{
    if (m_pendingRemoveIdx < 0)
        return;

    int lIdx = m_pendingRemoveIdx;
    m_pendingRemoveIdx = -1;

    if (lIdx >= static_cast<int>(m_lights.size()))
        return;

    // ─── ライトを解放してリストから除去する
    m_lights[lIdx]->Finalize();
    m_lights.erase(m_lights.begin() + lIdx);

    // ─── 選択インデックスが範囲外にならないように補正する
    if (m_selectedIdx >= static_cast<int>(m_lights.size()))
        m_selectedIdx = static_cast<int>(m_lights.size()) - 1;

    // ─── 削除完了を全オブジェクトに通知してライトを再配布する
    NotifyLightRemoved(GetLight(0));
}

// ------------------------------------------------------------
// Update
// 遅延削除を処理してから全ライトの状態を更新する
// 引数：なし
// ------------------------------------------------------------
void LIGHT_MANAGER::Update()
{
    // ─── フレームをまたいで安全にライトを削除する（イテレーション中の削除を回避）
    FlushPendingRemove();

    // ─── 全ライトのシャドウ行列などを更新する
    for (auto& lLight : m_lights)
        lLight->Update();
}

// ------------------------------------------------------------
// Draw
// 全ライトのギズモを描画する
// 引数：なし
// ------------------------------------------------------------
void LIGHT_MANAGER::Draw()
{
    for (auto& lLight : m_lights)
        lLight->Draw();
}

// ------------------------------------------------------------
// DrawImGui
// ライト一覧・選択・パラメーター編集の ImGui ウィンドウを描画する
// 引数：なし
// ------------------------------------------------------------
void LIGHT_MANAGER::DrawImGui()
{
    if (!ImGui::Begin("Light Manager"))
    {
        ImGui::End();
        return;
    }

    // ─── ライトの追加・削除ボタンを表示する
    if (ImGui::Button("Add Light"))
    {
        AddLight();
        m_selectedIdx = static_cast<int>(m_lights.size()) - 1;
    }

    ImGui::SameLine();

    // ─── ライトが 1 灯以下の場合は削除ボタンを無効にする
    bool lCanRemove = (m_lights.size() > 1) && (m_pendingRemoveIdx < 0);
    if (!lCanRemove) ImGui::BeginDisabled();
    if (ImGui::Button("Remove Selected"))
    {
        if (m_selectedIdx >= 0 && m_selectedIdx < static_cast<int>(m_lights.size()))
            m_pendingRemoveIdx = m_selectedIdx;
    }
    if (!lCanRemove) ImGui::EndDisabled();

    if (m_pendingRemoveIdx >= 0)
        ImGui::TextColored({ 1.0f, 0.6f, 0.0f, 1.0f }, "Removing Light%d next frame...", m_pendingRemoveIdx);

    ImGui::Separator();

    // ─── ライト一覧をリストボックスで表示して選択できるようにする
    std::vector<const char*> lNames;
    for (auto& lLight : m_lights)
        lNames.push_back(lLight->GetName().c_str());

    ImGui::Text("Select Light");
    ImGui::ListBox("##LightList", &m_selectedIdx,
        lNames.data(), static_cast<int>(lNames.size()), 4);

    ImGui::Separator();

    // ─── 選択中のライトのパラメーターを編集する
    if (m_selectedIdx >= 0 && m_selectedIdx < static_cast<int>(m_lights.size()))
    {
        LIGHT* lLight = m_lights[m_selectedIdx].get();
        std::string lLabel = "##L" + std::to_string(m_selectedIdx);
        lLight->DrawImGuiParams(lLabel);
    }

    ImGui::End();
}

// ------------------------------------------------------------
// Finalize
// 全ライトのリソースを解放してリストをクリアする
// 引数：なし
// ------------------------------------------------------------
void LIGHT_MANAGER::Finalize()
{
    // ─── 削除フラグをリセットして全ライトを解放する
    m_pendingRemoveIdx = -1;
    for (auto& lLight : m_lights)
        lLight->Finalize();
    m_lights.clear();
}

// ------------------------------------------------------------
// BindAll
// 全ライトのデータを CB_LIGHTS にまとめて指定スロットにバインドする
// 引数：slot - 定数バッファを設定するスロット番号（デフォルト 1）
// ------------------------------------------------------------
void LIGHT_MANAGER::BindAll(UINT slot)
{
    if (m_lights.empty()) return;

    // ─── 有効なライト数（MAX_LIGHTS 上限）で CB_LIGHTS を構築する
    CB_LIGHTS lCbData = {};
    lCbData.lightCount = static_cast<int>(
        min(m_lights.size(), static_cast<size_t>(MAX_LIGHTS)));

    for (int lI = 0; lI < lCbData.lightCount; lI++)
    {
        LIGHT* lLight = m_lights[lI].get();
        lCbData.lights[lI] = lLight->GetCbData();
    }

    // ─── GPU に転送して VS・PS の両方にバインドする
    m_pContext->UpdateSubresource(m_pCbLights.Get(), 0, nullptr, &lCbData, 0, 0);
    ID3D11Buffer* lCb = m_pCbLights.Get();
    m_pContext->VSSetConstantBuffers(slot, 1, &lCb);
    m_pContext->PSSetConstantBuffers(slot, 1, &lCb);
}