// ============================================================
// GameObjectManager.cpp
// シーン上の全ゲームオブジェクトを統括管理するクラスの実装
// オブジェクトの生成・配置・更新・描画・シェーダー割り当てを担う
// 制作者：Chan WingChung
// ============================================================

#include "GameObjectManager.h"

// ------------------------------------------------------------
// Initialize
// 全ゲームオブジェクトを生成して初期設定を行う
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool GAME_OBJECT_MANAGER::Initialize(
    ID3D11Device* pDevice,
    ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;

    // ─── ライトマネージャーを先に初期化する
    // 各オブジェクトにライトポインタを渡すため最初に生成する
    if (!m_lightManager.Initialize(pDevice, pContext))
        return false;

    // ─── カメラを生成して全オブジェクトに共有する
    auto lCamera = std::make_unique<CAMERA>();
    m_pCamera = Add(std::move(lCamera));

    // ─── モデルを生成してカメラとライトを設定する
    auto lModel = std::make_unique<MODEL>();
    lModel->SetFilePath(CONFIG::MODEL_PATH);
    lModel->SetAnimationPath(CONFIG::ANIMATION_PATH);
    lModel->SetCamera(m_pCamera);
    MODEL* lModelPtr = Add(std::move(lModel));

    // ─── ライトマネージャーにカメラを渡し最初のライトを追加する
    m_lightManager.SetCamera(m_pCamera);
    LIGHT* lLight = m_lightManager.AddLight();
    //lModelPtr->SetLight(lLight);    

    // ─── フィールド（床面）を生成して設定する
    auto lField = std::make_unique<FIELD>();
    lField->SetCamera(m_pCamera);
    lField->SetLight(lLight);
    lField->SetScale(CONFIG::FIELD_SCALE);
    m_pField = Add(std::move(lField));

    // ─── ミラーを生成して位置・回転・スケールを設定する
    auto lMirror = std::make_unique<MIRROR>();
    lMirror->SetCamera(m_pCamera);
    lMirror->SetPosition(CONFIG::MIRROR_POSITION);
    lMirror->SetRotationAxis(CONFIG::MIRROR_ROTATION_AXIS);
    lMirror->SetScale(CONFIG::MIRROR_SCALE);
    lMirror->SetRotationAngleDeg(CONFIG::MIRROR_ROTATION_ANGLE_DEG);
    m_pMirror = Add(std::move(lMirror));

    // ─── PBR キューブを生成して位置とスケールを設定する
    auto lCube = std::make_unique<CUBE>();
    lCube->SetCamera(m_pCamera);
    lCube->SetLight(lLight);
    lCube->SetPosition(CONFIG::CUBE_POSITION);
    lCube->SetScale(CONFIG::CUBE_SCALE);
    m_pCube = Add(std::move(lCube));

    // ─── スカイボックスを生成する（HDR SRV は App から後で注入される）
    auto lSkyBox = std::make_unique<SKY_BOX>();
    lSkyBox->SetCamera(m_pCamera);
    m_pSkyBox = Add(std::move(lSkyBox));

    // ─── 全オブジェクトを D3D11 デバイスで初期化する
    for (auto& lObj : m_gameObjects)
    {
        if (!lObj->Initialize(m_pDevice, m_pContext))
            return false;
    }

    // ─── ライト削除時に全オブジェクトへ新しいライトを再配布するコールバックを登録する
    m_lightManager.SetOnLightRemovedTarget(
        this,
        [](void* pSelf, LIGHT* pLight)
        {
            static_cast<GAME_OBJECT_MANAGER*>(pSelf)->RedistributeLight(pLight);
        });

    return true;
}

// ------------------------------------------------------------
// AssignShaders
// シェーダーマネージャーから各オブジェクトに適切なシェーダーを割り当てる
// 引数：shaderManager - 全シェーダーを所持する SHADER_MANAGER の参照
// ------------------------------------------------------------
void GAME_OBJECT_MANAGER::AssignShaders(SHADER_MANAGER& shaderManager)
{
    // ─── モデルに シェーダーを割り当てる
    for (auto* lModel : GetModels())
    {
        if (SHADER* lSk = shaderManager.GetByName("BasicSkinned"))
            lModel->SetSkinnedShaderFor("Basic", lSk);

        if (SHADER* lSk = shaderManager.GetByName("PhongSkinned"))
            lModel->SetSkinnedShaderFor("Phong", lSk);

        if (SHADER* lTs = shaderManager.GetByName("ToonSkinned"))
            lModel->SetSkinnedShaderFor("Toon", lTs);

        if (SHADER* lOs = shaderManager.GetByName("OutlineSkinned"))
            lModel->SetSkinnedShaderFor("Outline", lOs);

        SHADER* lToonShader = shaderManager.GetByName("Toon");
        lModel->AddShader(lToonShader);
    }

    // ─── ライトマネージャーにギズモシェーダーを設定する
    SHADER* lGizmoShader = shaderManager.GetByName("Gizmo");
    if (lGizmoShader)
        m_lightManager.SetGizmoShader(lGizmoShader);

    // ─── フィールドに Phong シェーダーを設定する
    SHADER* lPhongShader = shaderManager.GetByName("Phong");
    if (lPhongShader && m_pField)
        m_pField->SetShader(lPhongShader);

    // ─── ミラーに Mirror シェーダーを設定する
    SHADER* lMirrorShader = shaderManager.GetByName("Mirror");
    if (lMirrorShader && m_pMirror)
        m_pMirror->SetShader(lMirrorShader);

    // ─── PBR キューブに PBR シェーダーを設定する
    SHADER* lPbrShader = shaderManager.GetByName("PBR");
    if (lPbrShader && m_pCube)
        m_pCube->AddShader(lPbrShader);

    // ─── スカイボックスに Skybox シェーダーを設定する
    SHADER* lSkyShader = shaderManager.GetByName("Skybox");
    if (lSkyShader && m_pSkyBox)
        m_pSkyBox->SetShader(lSkyShader);
}

// ------------------------------------------------------------
// Update
// アクティブな全オブジェクトの状態を毎フレーム更新する
// 引数：なし
// ------------------------------------------------------------
void GAME_OBJECT_MANAGER::Update(float deltaSeconds)
{
    // ─── ライトの状態（位置・シャドウ行列など）を更新する
    m_lightManager.Update();

    // ─── アクティブなゲームオブジェクトを順に更新する
    for (auto& lObj : m_gameObjects)
    {
        if (lObj->IsActive())
            lObj->Update(deltaSeconds);
    }
}

// ------------------------------------------------------------
// Draw
// 全オブジェクトを適切な順序で描画する
// 引数：なし
// ------------------------------------------------------------
void GAME_OBJECT_MANAGER::Draw()
{
    // ─── ライトの定数バッファを b1 スロットにバインドする
    m_lightManager.BindAll(1);

    // ─── スカイボックスを最初に描画する（深度書き込みなし・最奥面に固定）
    if (m_pSkyBox)
        m_pSkyBox->Draw();

    // ─── ライトのギズモ（デバッグ表示）を描画する
    m_lightManager.Draw();

    // ─── スカイボックス以外の全オブジェクトを描画する
    for (auto& lObj : m_gameObjects)
    {
        if (lObj.get() == m_pSkyBox) continue;
        lObj->Draw();
    }
}

// ------------------------------------------------------------
// DrawImGui
// 全オブジェクトの ImGui ウィジェットを描画する
// 引数：なし
// ------------------------------------------------------------
void GAME_OBJECT_MANAGER::DrawImGui()
{
    // ─── ライトマネージャーの ImGui を描画する
    m_lightManager.DrawImGui();

    // ─── 各オブジェクトの ImGui を描画する
    for (auto& lObj : m_gameObjects)
        lObj->DrawImGui();
}

// ------------------------------------------------------------
// Finalize
// 全オブジェクトのリソースを解放してリストをクリアする
// 引数：なし
// ------------------------------------------------------------
void GAME_OBJECT_MANAGER::Finalize()
{
    // ─── ライトマネージャーのリソースを先に解放する
    m_lightManager.Finalize();

    // ─── 全ゲームオブジェクトを解放してリストをクリアする
    for (auto& lObj : m_gameObjects)
        lObj->Finalize();

    m_gameObjects.clear();
    m_models.clear();
    m_pCamera = nullptr;
    m_pField = nullptr;
}

// ------------------------------------------------------------
// BuildShadowContext
// シャドウパスに必要なライト・モデル・フィールド・出力先を一括収集する
// 引数：pShadowShader - シャドウ生成シェーダー
//        pMainRtv/pMainDsv - パス後に復元するメイン出力先
//        mainWidth/mainHeight - メイン画面解像度
// ------------------------------------------------------------
SHADOW_PASS_CONTEXT GAME_OBJECT_MANAGER::BuildShadowContext(
    SHADER* pShadowShader,
    SHADER* pShadowSkinnedShader,
    ID3D11RenderTargetView* pMainRtv,
    ID3D11DepthStencilView* pMainDsv,
    UINT mainWidth, UINT mainHeight) const
{
    SHADOW_PASS_CONTEXT lCtx;

    // ─── 全ライトを収集する（LIGHT_MANAGER は自分が所有している）
    for (size_t lI = 0; lI < m_lightManager.GetLightCount(); lI++)
        lCtx.lights.push_back(m_lightManager.GetLight(lI));

    // ─── 描画対象とシェーダーを設定する
    lCtx.pShadowShader = pShadowShader;
    lCtx.pShadowSkinnedShader = pShadowSkinnedShader;
    lCtx.models = m_models;
    lCtx.pField = m_pField;
    lCtx.pCube = m_pCube;
    lCtx.pMirror = m_pMirror;

    // ─── パス終了後に復元するメイン出力先を引き継ぐ
    lCtx.pMainRtv = pMainRtv;
    lCtx.pMainDsv = pMainDsv;
    lCtx.mainWidth = mainWidth;
    lCtx.mainHeight = mainHeight;

    return lCtx;
}

// ------------------------------------------------------------
// ExecuteMirrorPass
// ミラーの反射パスを実行して反射テクスチャを生成する
// 引数：pMainRtv  - メインのレンダーターゲットビュー
//        pMainDsv  - メインの深度ステンシルビュー
//        mainWidth / mainHeight - メイン画面の解像度
// ------------------------------------------------------------
void GAME_OBJECT_MANAGER::ExecuteMirrorPass(
    ID3D11RenderTargetView* pMainRtv,
    ID3D11DepthStencilView* pMainDsv,
    UINT mainWidth, UINT mainHeight)
{
    if (!m_pMirror) return;

    // ─── 反射コンテキストを構築してミラー以外の全オブジェクトを描画対象に登録する
    MIRROR_REFLECTION_CONTEXT lCtx;
    lCtx.pCamera = m_pCamera;
    lCtx.pMainRtv = pMainRtv;
    lCtx.pMainDsv = pMainDsv;
    lCtx.mainWidth = mainWidth;
    lCtx.mainHeight = mainHeight;

    for (auto& lObj : m_gameObjects)
    {
        if (lObj.get() != m_pMirror && lObj->IsActive())
            lCtx.drawables.push_back(lObj.get());
    }

    // ─── ミラーに反射パスの実行を委譲する
    m_pMirror->ExecuteReflectionPass(lCtx);
}

// ------------------------------------------------------------
// RedistributeLight
// ライト削除後に全オブジェクトへ新しいプライマリライトを再設定する
// 引数：pLight - 削除後に使用する新しいプライマリライトポインタ
// ------------------------------------------------------------
void GAME_OBJECT_MANAGER::RedistributeLight(LIGHT* pLight)
{
    // ─── ライトを受ける全オブジェクトへ一括再設定する
    for (auto& lObj : m_gameObjects)
        lObj->SetLight(pLight);
}

// ------------------------------------------------------------
// SetIblSrvs
// IBL ベイク済みの SRV を PBR キューブに注入する
// 引数：pDiffuse  - ディフューズ Irradiance キューブマップ SRV
//        pSpecular - プリフィルタード Specular キューブマップ SRV
//        pBrdfLut  - BRDF LUT テクスチャ SRV
// ------------------------------------------------------------
void GAME_OBJECT_MANAGER::SetIblSrvs(
    ID3D11ShaderResourceView* pDiffuse,
    ID3D11ShaderResourceView* pSpecular,
    ID3D11ShaderResourceView* pBrdfLut)
{
    // ─── PBR キューブに IBL テクスチャを設定する
    if (m_pCube)
    {
        m_pCube->SetIblDiffuseSrv(pDiffuse);
        m_pCube->SetIblSpecularSrv(pSpecular);
        m_pCube->SetBrdfLutSrv(pBrdfLut);
    }
}

// ------------------------------------------------------------
// SetHdrSrv
// HDR 環境マップの SRV をスカイボックスに注入する
// 引数：pSrv - HDR テクスチャの SRV
// ------------------------------------------------------------
void GAME_OBJECT_MANAGER::SetHdrSrv(ID3D11ShaderResourceView* pSrv)
{
    // ─── スカイボックスに HDR SRV を渡す
    if (m_pSkyBox)
        m_pSkyBox->SetHdrSrv(pSrv);
}