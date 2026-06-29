// ============================================================
// Mirror.cpp
// 平面鏡反射クラスの実装
// 反射ビューで全オブジェクトを描画してレンダーテクスチャに焼き込む
// 制作者：Chan WingChung
// ============================================================

#include "Mirror.h"
#include "GameObject.h"
#include "Imgui/imgui.h"

// ------------------------------------------------------------
// Initialize
// メッシュ構築・POLYGON 初期化・ミラー CB・レンダーテクスチャを生成する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool MIRROR::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    // ─── まずクワッドメッシュを設定してから基底クラスを初期化する
    BuildMesh();
    if (!POLYGON::Initialize(pDevice, pContext))
        return false;

    // ─── ミラー用定数バッファ（スクリーンサイズ）を生成する
    D3D11_BUFFER_DESC lCbDesc = {};
    lCbDesc.ByteWidth = sizeof(CB_MIRROR);
    lCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(pDevice->CreateBuffer(&lCbDesc, nullptr, m_pMirrorCb.GetAddressOf())))
        return false;

    // ─── 反射テクスチャを生成する（REFLECTION_WIDTH × REFLECTION_HEIGHT）
    m_renderTexture = std::make_unique<RENDER_TEXTURE>();
    if (!m_renderTexture->Initialize(pDevice, REFLECTION_WIDTH, REFLECTION_HEIGHT))
        return false;

    m_pReflectionSrv = m_renderTexture->GetSrv();
    m_reflectionWidth = static_cast<float>(REFLECTION_WIDTH);
    m_reflectionHeight = static_cast<float>(REFLECTION_HEIGHT);

    return true;
}

// ------------------------------------------------------------
// ExecuteReflectionPass
// 反射カメラで全オブジェクトを描画して反射テクスチャを生成する
// 引数：context - 描画対象オブジェクト・カメラ・RTV/DSV を含むコンテキスト
// ------------------------------------------------------------
void MIRROR::ExecuteReflectionPass(const MIRROR_REFLECTION_CONTEXT& context)
{
    if (!context.pCamera || !m_renderTexture) return;

    // ─── 鏡面に対してカメラ位置・注視点・上方向を反射した View 行列を計算する
    MATRIX4X4 lReflectionView = CalcReflectionView(
        context.pCamera->GetPosition(),
        context.pCamera->GetTarget(),
        VECTOR3(0.0f, 1.0f, 0.0f));

    // ─── 反射テクスチャをレンダーターゲットにしてクリアする
    m_renderTexture->BeginRender(m_pContext);

    MATRIX4X4 lReflectionViewProj = lReflectionView * context.pCamera->GetProjectionMatrix();
    VECTOR3   lReflectionEye = CalcReflectionEye(context.pCamera->GetPosition());

    // ─── ミラー自身を除いた全オブジェクトを反射 viewProj で描画する
    for (auto* lObj : context.drawables)
    {
        if (lObj && lObj != this)
            lObj->DrawWithViewProj(lReflectionViewProj, lReflectionEye);
    }

    // ─── 描画後にメインレンダーターゲットを復元する
    RestoreMainTarget(context);
}

// ------------------------------------------------------------
// RestoreMainTarget
// 反射パス後にメインの RTV / DSV とビューポートを復元する
// 引数：context - メインの RTV・DSV・解像度を持つコンテキスト
// ------------------------------------------------------------
void MIRROR::RestoreMainTarget(const MIRROR_REFLECTION_CONTEXT& context)
{
    // ─── メインレンダーターゲットに戻す
    ID3D11RenderTargetView* lRtvs[] = { context.pMainRtv };
    m_pContext->OMSetRenderTargets(1, lRtvs, context.pMainDsv);

    // ─── メイン解像度でビューポートを設定する
    D3D11_VIEWPORT lVp = {};
    lVp.Width = static_cast<float>(context.mainWidth);
    lVp.Height = static_cast<float>(context.mainHeight);
    lVp.MaxDepth = 1.0f;
    m_pContext->RSSetViewports(1, &lVp);
}

// ------------------------------------------------------------
// BindExtraCb
// ミラー CB（スクリーンサイズ）と反射テクスチャ SRV をバインドする
// 引数：なし
// ------------------------------------------------------------
void MIRROR::BindExtraCb() const
{
    // ─── CB_MIRROR にスクリーンサイズを設定して b4 にバインドする
    CB_MIRROR lCbData = {};
    lCbData.screenSize = { m_reflectionWidth, m_reflectionHeight };

    m_pContext->UpdateSubresource(m_pMirrorCb.Get(), 0, nullptr, &lCbData, 0, 0);
    ID3D11Buffer* lCb = m_pMirrorCb.Get();
    m_pContext->PSSetConstantBuffers(4, 1, &lCb);

    // ─── 反射テクスチャを t0 にバインドする
    ID3D11ShaderResourceView* lSrv = m_pReflectionSrv;
    m_pContext->PSSetShaderResources(0, 1, &lSrv);
}

// ------------------------------------------------------------
// DrawImGui
// ミラーの位置・回転軸・角度・スケールを操作する ImGui ウィンドウを描画する
// 引数：なし
// ------------------------------------------------------------
void MIRROR::DrawImGui()
{
    if (ImGui::Begin("Mirror"))
    {
        // ─── 位置の編集
        VECTOR3 lPos = GetPosition();
        if (ImGui::DragFloat3("Position", &lPos.x, 0.1f))
            SetPosition(lPos);

        ImGui::Separator();

        // ─── 任意軸周りの回転設定
        VECTOR3 lAxis = m_rotationAxis;
        if (ImGui::DragFloat3("Rotation Axis", &lAxis.x, 0.01f, -1.0f, 1.0f))
            SetRotationAxis(lAxis);

        float lAngle = m_rotationAngleDeg;
        if (ImGui::DragFloat("Rotation Angle (deg)", &lAngle, 1.0f, -360.0f, 360.0f))
            SetRotationAngleDeg(lAngle);

        ImGui::Separator();

        // ─── スケールの編集
        VECTOR3 lScale = m_scale;
        if (ImGui::DragFloat3("Scale", &lScale.x, 0.01f, 0.01f, 10.0f))
            SetScale(lScale);
    }
    ImGui::End();
}

// ------------------------------------------------------------
// CalcWorldMatrix
// 任意軸回転を含むミラーのワールド行列を計算して返す
// 引数：なし
// ------------------------------------------------------------
MATRIX4X4 MIRROR::CalcWorldMatrix() const
{
    using namespace DirectX;

    // ─── 回転軸が零ベクトルの場合は Y 軸を代替として使用する
    XMVECTOR lAxis = XMVectorSet(m_rotationAxis.x, m_rotationAxis.y, m_rotationAxis.z, 0.0f);
    if (XMVector3LengthSq(lAxis).m128_f32[0] < 1e-8f)
        lAxis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // ─── Scale * 任意軸回転 * Translation を合成する
    float    lAngleRad = XMConvertToRadians(m_rotationAngleDeg);
    XMMATRIX lScaleMat = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
    XMMATRIX lRotMat = XMMatrixRotationAxis(lAxis, lAngleRad);
    XMMATRIX lTransMat = XMMatrixTranslation(GetPosition().x, GetPosition().y, GetPosition().z);

    return MATRIX4X4(lScaleMat * lRotMat * lTransMat);
}

// ------------------------------------------------------------
// SetupMaterialCb
// 反射テクスチャの有無に応じてマテリアルカラーと useTexture を設定する
// 引数：cbData - 書き込む CB_WVP 構造体の参照
// ------------------------------------------------------------
void MIRROR::SetupMaterialCb(CB_WVP& cbData) const
{
    // ─── 反射テクスチャがある場合はそれを使い、なければ青みがかった色で表示する
    if (m_pReflectionSrv)
    {
        cbData.useTexture = 1;
        cbData.materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    }
    else
    {
        cbData.useTexture = 0;
        cbData.materialColor = { 0.4f, 0.6f, 0.8f, 1.0f };
    }
}

// ------------------------------------------------------------
// GetWorldNormal
// ミラー平面のワールド法線ベクトルを計算して返す
// 引数：なし
// ------------------------------------------------------------
VECTOR3 MIRROR::GetWorldNormal() const
{
    using namespace DirectX;

    // ─── ローカル法線 (0,0,-1) を回転行列で変換してワールド法線を得る
    XMVECTOR lAxis = XMVectorSet(m_rotationAxis.x, m_rotationAxis.y, m_rotationAxis.z, 0.0f);
    if (XMVector3LengthSq(lAxis).m128_f32[0] < 1e-8f)
        lAxis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    float    lAngleRad = XMConvertToRadians(m_rotationAngleDeg);
    XMMATRIX lRotMat = XMMatrixRotationAxis(lAxis, lAngleRad);
    XMVECTOR lDefaultNormal = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
    XMVECTOR lWorldNormal = XMVector3Normalize(XMVector3TransformNormal(lDefaultNormal, lRotMat));

    XMFLOAT3 lResult;
    XMStoreFloat3(&lResult, lWorldNormal);
    return VECTOR3(lResult.x, lResult.y, lResult.z);
}

// ------------------------------------------------------------
// CalcReflectionEye
// 鏡面に対してカメラ位置を反射した Eye 座標を返す
// 引数：cameraPos - 実カメラの位置
// ------------------------------------------------------------
VECTOR3 MIRROR::CalcReflectionEye(const VECTOR3& cameraPos) const
{
    using namespace DirectX;

    // ─── カメラから鏡面への符号付き距離を計算して 2 倍折り返す
    VECTOR3  lNormalV3 = GetWorldNormal();
    XMVECTOR lNormal = XMVectorSet(lNormalV3.x, lNormalV3.y, lNormalV3.z, 0.0f);
    VECTOR3  lPosV3 = GetPosition();
    XMVECTOR lMirrorPos = XMVectorSet(lPosV3.x, lPosV3.y, lPosV3.z, 0.0f);
    XMVECTOR lEye = XMVectorSet(cameraPos.x, cameraPos.y, cameraPos.z, 0.0f);

    XMVECTOR lDiff = XMVectorSubtract(lEye, lMirrorPos);
    float    lDot = XMVectorGetX(XMVector3Dot(lDiff, lNormal));
    XMVECTOR lReflEye = XMVectorSubtract(lEye, XMVectorScale(lNormal, 2.0f * lDot));

    VECTOR3 lResult;
    lResult.x = XMVectorGetX(lReflEye);
    lResult.y = XMVectorGetY(lReflEye);
    lResult.z = XMVectorGetZ(lReflEye);
    return lResult;
}

// ------------------------------------------------------------
// CalcReflectionView
// 実カメラの Eye / Target / Up を鏡面反射してビュー行列を計算する
// 引数：realEye    - 実カメラの Eye 位置
//        realTarget - 実カメラの注視点
//        realUp     - 実カメラの上方向ベクトル
// ------------------------------------------------------------
MATRIX4X4 MIRROR::CalcReflectionView(
    const VECTOR3& realEye,
    const VECTOR3& realTarget,
    const VECTOR3& realUp) const
{
    VECTOR3 lPlanePoint = GetPosition();
    VECTOR3 lNormal = GetWorldNormal();

    // ─── Eye・Target・Up を鏡面に対してそれぞれ反射する
    VECTOR3 lEyeToPlane = realEye - lPlanePoint;
    float   lEyeDist = VECTOR3::Dot(lEyeToPlane, lNormal);
    VECTOR3 lReflectedEye = realEye - lNormal * (2.0f * lEyeDist);

    VECTOR3 lTargetToPlane = realTarget - lPlanePoint;
    float   lTargetDist = VECTOR3::Dot(lTargetToPlane, lNormal);
    VECTOR3 lReflectedTarget = realTarget - lNormal * (2.0f * lTargetDist);

    float   lUpDot = VECTOR3::Dot(realUp, lNormal);
    VECTOR3 lReflectedUp = realUp - lNormal * (2.0f * lUpDot);

    return MATRIX4X4::LookAtLH(lReflectedEye, lReflectedTarget, lReflectedUp);
}

// ------------------------------------------------------------
// BuildMesh
// ミラー用の正方形クワッドメッシュを設定する
// 引数：なし
// ------------------------------------------------------------
void MIRROR::BuildMesh()
{
    float lHalf = 0.5f;

    // ─── 単位サイズの正方形を 4 頂点で定義する（法線は -Z 向き）
    std::vector<VERTEX_3D> lVertices =
    {
        { -lHalf, -lHalf, 0.0f,    0.0f, 0.0f, -1.0f,    0.0f, 1.0f,    0,0,0,  0,0,0 },
        {  lHalf, -lHalf, 0.0f,    0.0f, 0.0f, -1.0f,    1.0f, 1.0f,    0,0,0,  0,0,0 },
        { -lHalf,  lHalf, 0.0f,    0.0f, 0.0f, -1.0f,    0.0f, 0.0f,    0,0,0,  0,0,0 },
        {  lHalf,  lHalf, 0.0f,    0.0f, 0.0f, -1.0f,    1.0f, 0.0f,    0,0,0,  0,0,0 },
    };

    std::vector<UINT> lIndices = { 0, 2, 1,  1, 2, 3 };
    SetMeshData(lVertices, lIndices);
}