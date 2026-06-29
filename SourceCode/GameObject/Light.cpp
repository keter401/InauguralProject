// ============================================================
// Light.cpp
// ライトクラスの実装
// ポイント・ディレクショナル・スポット・エリアの 4 種とシャドウマップを管理する
// 制作者：Chan WingChung
// ============================================================

#include "Light.h"
#include "Math/Vertex.h"
#include "Camera.h"
#include <string>
#include "Imgui/imgui.h"
#include <cmath>

using namespace DirectX;

// ------------------------------------------------------------
// Initialize
// ライト CB・ギズモ CB・ギズモメッシュ・シャドウマップを生成する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool LIGHT::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;

    // ─── ライトデータ用の定数バッファを生成する（b1 スロット用）
    D3D11_BUFFER_DESC lDesc = {};
    lDesc.ByteWidth = sizeof(CB_LIGHT_DATA);
    lDesc.Usage = D3D11_USAGE_DEFAULT;
    lDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(pDevice->CreateBuffer(&lDesc, nullptr, m_pCb.GetAddressOf())))
        return false;

    // ─── ギズモ描画用の定数バッファを生成する（b0 スロット用）
    D3D11_BUFFER_DESC lGizmoDesc = {};
    lGizmoDesc.ByteWidth = sizeof(CB_GIZMO);
    lGizmoDesc.Usage = D3D11_USAGE_DEFAULT;
    lGizmoDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(pDevice->CreateBuffer(&lGizmoDesc, nullptr, m_pGizmoCb.GetAddressOf())))
        return false;

    // ─── ライトタイプ別のギズモメッシュを生成する
    CreateSphereWire(1.0f, 16);
    CreateCrossMarker(1.0f);
    CreateArrow(1.0f);
    CreateConeWire(1.0f, m_spotAngleDeg, 16);
    CreateAreaRect(1.0f, 1.0f);

    return true;
}

// ------------------------------------------------------------
// CreateSphereWire
// ポイントライトの範囲を示す球形ワイヤーフレームの頂点バッファを生成する
// 引数：radius   - 球の半径
//        segments - 分割数（円弧の滑らかさ）
// ------------------------------------------------------------
void LIGHT::CreateSphereWire(float radius, int segments)
{
    std::vector<VERTEX_GIZMO> lVertices;
    const float lColor[3] = { 1.0f, 1.0f, 0.3f };

    // ─── XY・YZ・XZ の 3 平面で円弧を描く
    for (int lPlane = 0; lPlane < 3; lPlane++)
    {
        for (int lIdx = 0; lIdx < segments; lIdx++)
        {
            float lTheta0 = (XM_2PI * lIdx) / segments;
            float lTheta1 = (XM_2PI * (lIdx + 1)) / segments;

            VERTEX_GIZMO lV0 = {}, lV1 = {};
            lV0.r = lColor[0]; lV0.g = lColor[1]; lV0.b = lColor[2];
            lV1.r = lColor[0]; lV1.g = lColor[1]; lV1.b = lColor[2];

            if (lPlane == 0)
            {
                lV0.x = radius * cosf(lTheta0); lV0.y = radius * sinf(lTheta0); lV0.z = 0;
                lV1.x = radius * cosf(lTheta1); lV1.y = radius * sinf(lTheta1); lV1.z = 0;
            }
            else if (lPlane == 1)
            {
                lV0.x = 0; lV0.y = radius * cosf(lTheta0); lV0.z = radius * sinf(lTheta0);
                lV1.x = 0; lV1.y = radius * cosf(lTheta1); lV1.z = radius * sinf(lTheta1);
            }
            else
            {
                lV0.x = radius * cosf(lTheta0); lV0.y = 0; lV0.z = radius * sinf(lTheta0);
                lV1.x = radius * cosf(lTheta1); lV1.y = 0; lV1.z = radius * sinf(lTheta1);
            }

            lVertices.push_back(lV0);
            lVertices.push_back(lV1);
        }
    }

    // ─── 頂点バッファを生成する
    m_sphereVertexCount = static_cast<UINT>(lVertices.size());
    D3D11_BUFFER_DESC lVbDesc = {};
    lVbDesc.ByteWidth = sizeof(VERTEX_GIZMO) * m_sphereVertexCount;
    lVbDesc.Usage = D3D11_USAGE_DEFAULT;
    lVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA lInit = { lVertices.data() };
    m_pDevice->CreateBuffer(&lVbDesc, &lInit, m_pSphereVb.GetAddressOf());
}

// ------------------------------------------------------------
// CreateCrossMarker
// ライト位置を示す十字マーカーの頂点バッファを生成する
// 引数：size - 十字の腕の長さ
// ------------------------------------------------------------
void LIGHT::CreateCrossMarker(float size)
{
    std::vector<VERTEX_GIZMO> lVertices;
    const float lColor[3] = { 1.0f, 0.2f, 0.2f };

    // ─── XYZ 各軸方向に 2 点ずつ（合計 6 点）追加する
    lVertices.push_back({ -size, 0, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ size, 0, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ 0, -size, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ 0,  size, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ 0, 0, -size, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ 0, 0,  size, lColor[0], lColor[1], lColor[2] });

    m_crossVertexCount = static_cast<UINT>(lVertices.size());
    D3D11_BUFFER_DESC lVbDesc = {};
    lVbDesc.ByteWidth = sizeof(VERTEX_GIZMO) * m_crossVertexCount;
    lVbDesc.Usage = D3D11_USAGE_DEFAULT;
    lVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA lInit = { lVertices.data() };
    m_pDevice->CreateBuffer(&lVbDesc, &lInit, m_pCrossVb.GetAddressOf());
}

// ------------------------------------------------------------
// CreateArrow
// ディレクショナルライトの方向を示す矢印の頂点バッファを生成する
// 引数：length - 矢印の全長
// ------------------------------------------------------------
void LIGHT::CreateArrow(float length)
{
    std::vector<VERTEX_GIZMO> lVertices;
    const float lColor[3] = { 0.3f, 0.7f, 1.0f };
    const float lHeadSize = length * 0.2f;

    // ─── 矢印のシャフト（原点 → 先端）を追加する
    lVertices.push_back({ 0, 0, 0,      lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ 0, 0, length, lColor[0], lColor[1], lColor[2] });

    // ─── 矢印の羽根（先端から 4 方向に放射）を追加する
    auto lAddHeadLine = [&](float dx, float dy)
        {
            lVertices.push_back({ 0, 0, length, lColor[0], lColor[1], lColor[2] });
            lVertices.push_back({ dx * lHeadSize, dy * lHeadSize, length - lHeadSize, lColor[0], lColor[1], lColor[2] });
        };
    lAddHeadLine(1, 0);
    lAddHeadLine(-1, 0);
    lAddHeadLine(0, 1);
    lAddHeadLine(0, -1);

    m_arrowVertexCount = static_cast<UINT>(lVertices.size());
    D3D11_BUFFER_DESC lVbDesc = {};
    lVbDesc.ByteWidth = sizeof(VERTEX_GIZMO) * m_arrowVertexCount;
    lVbDesc.Usage = D3D11_USAGE_DEFAULT;
    lVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA lInit = { lVertices.data() };
    m_pDevice->CreateBuffer(&lVbDesc, &lInit, m_pArrowVb.GetAddressOf());
}

// ------------------------------------------------------------
// CreateConeWire
// スポットライトの照射範囲を示すコーン形ワイヤーフレームを生成する
// 引数：length   - コーンの深さ
//        angleDeg - 半頂角（度数法）
//        segments - 底面円の分割数
// ------------------------------------------------------------
void LIGHT::CreateConeWire(float length, float angleDeg, int segments)
{
    std::vector<VERTEX_GIZMO> lVertices;
    const float lColor[3] = { 0.8f, 0.5f, 1.0f };

    // ─── 半頂角から底面円の半径を計算する
    float lAngleRad = XMConvertToRadians(angleDeg);
    float lBaseRadius = length * tanf(lAngleRad);

    // ─── 頂点から底面円上の 4 点へのライン（稜線）を追加する
    for (int lIdx = 0; lIdx < 4; lIdx++)
    {
        float lTheta = (XM_2PI * lIdx) / 4;
        float lX = lBaseRadius * cosf(lTheta);
        float lY = lBaseRadius * sinf(lTheta);

        lVertices.push_back({ 0, 0, 0, lColor[0], lColor[1], lColor[2] });
        lVertices.push_back({ lX, lY, length, lColor[0], lColor[1], lColor[2] });
    }

    // ─── 底面円を分割数に応じたライン列で描く
    for (int lIdx = 0; lIdx < segments; lIdx++)
    {
        float lTheta0 = (XM_2PI * lIdx) / segments;
        float lTheta1 = (XM_2PI * (lIdx + 1)) / segments;

        lVertices.push_back({ lBaseRadius * cosf(lTheta0), lBaseRadius * sinf(lTheta0), length, lColor[0], lColor[1], lColor[2] });
        lVertices.push_back({ lBaseRadius * cosf(lTheta1), lBaseRadius * sinf(lTheta1), length, lColor[0], lColor[1], lColor[2] });
    }

    m_coneVertexCount = static_cast<UINT>(lVertices.size());
    D3D11_BUFFER_DESC lVbDesc = {};
    lVbDesc.ByteWidth = sizeof(VERTEX_GIZMO) * m_coneVertexCount;
    lVbDesc.Usage = D3D11_USAGE_DEFAULT;
    lVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA lInit = { lVertices.data() };
    m_pDevice->CreateBuffer(&lVbDesc, &lInit, m_pConeVb.GetAddressOf());
}

// ------------------------------------------------------------
// CreateAreaRect
// エリアライトの発光面を示す矩形ワイヤーフレームを生成する
// 引数：width  - 矩形の横幅
//        height - 矩形の縦幅
// ------------------------------------------------------------
void LIGHT::CreateAreaRect(float width, float height)
{
    std::vector<VERTEX_GIZMO> lVertices;
    const float lColor[3] = { 1.0f, 0.6f, 0.3f };
    float lHw = width * 0.5f;
    float lHh = height * 0.5f;

    // ─── 矩形の 4 辺を 8 頂点のライン列で定義する
    lVertices.push_back({ -lHw, -lHh, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ lHw, -lHh, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ lHw, -lHh, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ lHw,  lHh, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ lHw,  lHh, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ -lHw,  lHh, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ -lHw,  lHh, 0, lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ -lHw, -lHh, 0, lColor[0], lColor[1], lColor[2] });

    // ─── 発光方向を示す短いライン（法線方向）を追加する
    lVertices.push_back({ 0, 0, 0,    lColor[0], lColor[1], lColor[2] });
    lVertices.push_back({ 0, 0, 0.5f, lColor[0], lColor[1], lColor[2] });

    m_areaVertexCount = static_cast<UINT>(lVertices.size());
    D3D11_BUFFER_DESC lVbDesc = {};
    lVbDesc.ByteWidth = sizeof(VERTEX_GIZMO) * m_areaVertexCount;
    lVbDesc.Usage = D3D11_USAGE_DEFAULT;
    lVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA lInit = { lVertices.data() };
    m_pDevice->CreateBuffer(&lVbDesc, &lInit, m_pAreaVb.GetAddressOf());
}

// ------------------------------------------------------------
// Update
// ライトデータを CB に書き込みキューブシャドウ行列を更新する
// 引数：なし
// ------------------------------------------------------------
void LIGHT::Update(float deltaSeconds)
{
    // ─── 方向ベクトルを正規化して CB_LIGHT_DATA に設定する
    VECTOR3 lDirNorm = m_direction.Normalize();

    m_cbData.lightType = static_cast<int>(m_type);
    m_cbData.lightPos = { m_position.x,  m_position.y,  m_position.z };
    m_cbData.lightDir = { lDirNorm.x,    lDirNorm.y,    lDirNorm.z };
    m_cbData.lightColor = m_color;
    m_cbData.intensity = m_intensity;
    m_cbData.range = m_range;
    m_cbData.spotAngle = XMConvertToRadians(m_spotAngleDeg);
    m_cbData.spotFalloff = m_spotFalloff;
    m_cbData.areaWidth = m_areaWidth;
    m_cbData.areaHeight = m_areaHeight;

    // ─── 変更したデータを GPU に転送する
    m_pContext->UpdateSubresource(m_pCb.Get(), 0, nullptr, &m_cbData, 0, 0);

    // ─── キューブシャドウマップ用の 6 方向ビュー行列を更新する
    UpdateCubeShadowMatrices();
}

// ------------------------------------------------------------
// UpdateCubeShadowMatrices
// 6 方向（+X/-X/+Y/-Y/+Z/-Z）のビュー行列と投影行列を更新する
// 引数：なし
// ------------------------------------------------------------
void LIGHT::UpdateCubeShadowMatrices()
{
    // ─── 6 方向と各方向の上ベクトルを定義する
    const VECTOR3 lDirs[6] =
    {
        {  1.0f,  0.0f,  0.0f },
        { -1.0f,  0.0f,  0.0f },
        {  0.0f,  1.0f,  0.0f },
        {  0.0f, -1.0f,  0.0f },
        {  0.0f,  0.0f,  1.0f },
        {  0.0f,  0.0f, -1.0f },
    };

    const VECTOR3 lUps[6] =
    {
        { 0.0f, 1.0f,  0.0f },
        { 0.0f, 1.0f,  0.0f },
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f,  1.0f },
        { 0.0f, 1.0f,  0.0f },
        { 0.0f, 1.0f,  0.0f },
    };

    // ─── 各フェイスのビュー行列をライト位置を中心として計算する
    for (UINT lFace = 0; lFace < 6; lFace++)
    {
        VECTOR3 lAt =
        {
            m_position.x + lDirs[lFace].x,
            m_position.y + lDirs[lFace].y,
            m_position.z + lDirs[lFace].z
        };
        m_cubeView[lFace] = MATRIX4X4::LookAtLH(m_position, lAt, lUps[lFace]);
    }

    // ─── 90 度視野角・正方形アスペクト・range をファークリップとする投影行列
    m_cubeProj = MATRIX4X4::PerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, m_range);
}

// ------------------------------------------------------------
// Bind
// ライトデータ CB を指定スロットにバインドする
// 引数：slot - 定数バッファスロット番号
// ------------------------------------------------------------
void LIGHT::Bind(UINT slot)
{
    // ─── VS と PS の両方に同じスロットでバインドする
    m_pContext->VSSetConstantBuffers(slot, 1, m_pCb.GetAddressOf());
    m_pContext->PSSetConstantBuffers(slot, 1, m_pCb.GetAddressOf());
}

// ------------------------------------------------------------
// DrawGizmoMesh
// ギズモ用頂点バッファをワールド行列と色で描画する汎用関数
// 引数：pVb         - 頂点バッファポインタ
//        vertexCount - 頂点数
//        world       - ワールド行列
//        color       - ギズモの描画色（XMFLOAT4）
//        topology    - プリミティブトポロジー（デフォルト LINELIST）
// ------------------------------------------------------------
void LIGHT::DrawGizmoMesh(ID3D11Buffer* pVb, UINT vertexCount,
    const MATRIX4X4& world, const XMFLOAT4& color,
    D3D11_PRIMITIVE_TOPOLOGY topology)
{
    m_pContext->IASetPrimitiveTopology(topology);

    // ─── CB_GIZMO（WVP・色）を更新してバインドする
    CB_GIZMO lCbData = {};
    lCbData.wvp = m_pCamera->GetWVP(world).m_mat;
    lCbData.color = color;
    m_pContext->UpdateSubresource(m_pGizmoCb.Get(), 0, nullptr, &lCbData, 0, 0);

    ID3D11Buffer* lCb = m_pGizmoCb.Get();
    m_pContext->VSSetConstantBuffers(0, 1, &lCb);

    // ─── 頂点バッファをバインドして描画する
    UINT lStride = sizeof(VERTEX_GIZMO);
    UINT lOffset = 0;
    m_pContext->IASetVertexBuffers(0, 1, &pVb, &lStride, &lOffset);
    m_pContext->Draw(vertexCount, 0);
}

// ─── 方向ベクトルから YZ 回転行列を生成するスタティックヘルパー
static MATRIX4X4 MakeDirectionRotation(const VECTOR3& dir)
{
    VECTOR3 lTarget = dir;
    float lYaw = atan2f(lTarget.x, lTarget.z);
    float lPitch = -asinf(lTarget.y / (sqrtf(lTarget.x * lTarget.x + lTarget.y * lTarget.y + lTarget.z * lTarget.z) + 0.00001f));
    return MATRIX4X4::RotationX(lPitch) * MATRIX4X4::RotationY(lYaw);
}

// ------------------------------------------------------------
// DrawPointGizmo
// ポイントライト用の球形ワイヤーと位置マーカーを描画する
// 引数：なし
// ------------------------------------------------------------
void LIGHT::DrawPointGizmo()
{
    // ─── 範囲を半径とした球形ワイヤーで光の届く範囲を表示する
    MATRIX4X4 lSphereWorld = MATRIX4X4::Scale(m_range, m_range, m_range)
        * MATRIX4X4::Translation(m_position.x, m_position.y, m_position.z);
    DrawGizmoMesh(m_pSphereVb.Get(), m_sphereVertexCount, lSphereWorld,
        { m_color.x, m_color.y, m_color.z, 0.5f });

    // ─── 強度に比例したサイズの十字マーカーで位置を表示する
    const float lMarkerSize = 0.3f * (0.5f + m_intensity * 0.3f);
    MATRIX4X4 lCrossWorld = MATRIX4X4::Scale(lMarkerSize, lMarkerSize, lMarkerSize)
        * MATRIX4X4::Translation(m_position.x, m_position.y, m_position.z);
    DrawGizmoMesh(m_pCrossVb.Get(), m_crossVertexCount, lCrossWorld, { 1.0f, 0.2f, 0.2f, 1.0f });
}

// ------------------------------------------------------------
// DrawDirectionalGizmo
// ディレクショナルライト用の方向矢印を描画する
// 引数：なし
// ------------------------------------------------------------
void LIGHT::DrawDirectionalGizmo()
{
    // ─── 光の方向に回転させた矢印を描画する
    MATRIX4X4 lRot = MakeDirectionRotation(m_direction);
    MATRIX4X4 lWorld = MATRIX4X4::Scale(1.0f, 1.0f, 1.0f) * lRot
        * MATRIX4X4::Translation(m_position.x, m_position.y, m_position.z);
    DrawGizmoMesh(m_pArrowVb.Get(), m_arrowVertexCount, lWorld, { 0.3f, 0.7f, 1.0f, 1.0f });
}

// ------------------------------------------------------------
// DrawSpotGizmo
// スポットライト用のコーン形ワイヤーを描画する
// 引数：なし
// ------------------------------------------------------------
void LIGHT::DrawSpotGizmo()
{
    // ─── 光の方向に回転させた範囲サイズのコーンを描画する
    MATRIX4X4 lRot = MakeDirectionRotation(m_direction);
    MATRIX4X4 lWorld = MATRIX4X4::Scale(m_range, m_range, m_range) * lRot
        * MATRIX4X4::Translation(m_position.x, m_position.y, m_position.z);
    DrawGizmoMesh(m_pConeVb.Get(), m_coneVertexCount, lWorld, { 0.8f, 0.5f, 1.0f, 0.6f });
}

// ------------------------------------------------------------
// DrawAreaGizmo
// エリアライト用の矩形ワイヤーを描画する
// 引数：なし
// ------------------------------------------------------------
void LIGHT::DrawAreaGizmo()
{
    // ─── 光の方向に回転させた発光面サイズの矩形を描画する
    MATRIX4X4 lRot = MakeDirectionRotation(m_direction);
    MATRIX4X4 lWorld = MATRIX4X4::Scale(m_areaWidth, m_areaHeight, 1.0f) * lRot
        * MATRIX4X4::Translation(m_position.x, m_position.y, m_position.z);
    DrawGizmoMesh(m_pAreaVb.Get(), m_areaVertexCount, lWorld, { 1.0f, 0.6f, 0.3f, 1.0f });
}

// ------------------------------------------------------------
// Draw
// ライトタイプに応じたギズモを描画する
// 引数：なし
// ------------------------------------------------------------
void LIGHT::Draw()
{
    if (!m_pGizmoShader || !m_pCamera || !m_bShowGizmo) return;

    // ─── ギズモシェーダーをバインドしてタイプ別に描画する
    m_pGizmoShader->Bind(m_pContext);

    switch (m_type)
    {
    case LIGHT_TYPE::POINT:       DrawPointGizmo();       break;
    case LIGHT_TYPE::DIRECTIONAL: DrawDirectionalGizmo(); break;
    case LIGHT_TYPE::SPOT:        DrawSpotGizmo();        break;
    case LIGHT_TYPE::AREA:        DrawAreaGizmo();        break;
    }
}

// ------------------------------------------------------------
// DrawImGuiParams
// ライトタイプ・位置・方向・色・強度などのパラメーターを ImGui で編集する
// 引数：label - ImGui ウィジェットの ID プレフィックス（重複回避用）
// ------------------------------------------------------------
void LIGHT::DrawImGuiParams(const std::string& label)
{
    // ─── ライトタイプの選択コンボを表示する
    const char* lTypeNames[] = { "Point", "Directional", "Spot", "Area" };
    int lTypeIdx = static_cast<int>(m_type);
    if (ImGui::Combo((label + " Type").c_str(), &lTypeIdx, lTypeNames, 4))
        m_type = static_cast<LIGHT_TYPE>(lTypeIdx);

    ImGui::Checkbox((label + " Gizmo").c_str(), &m_bShowGizmo);
    ImGui::Separator();

    // ─── 位置・方向・色・強度を編集する
    float lPos[3] = { m_position.x, m_position.y, m_position.z };
    if (ImGui::DragFloat3((label + " Position").c_str(), lPos, 0.1f))
        m_position = { lPos[0], lPos[1], lPos[2] };

    float lDir[3] = { m_direction.x, m_direction.y, m_direction.z };
    if (ImGui::DragFloat3((label + " Direction").c_str(), lDir, 0.05f, -1.0f, 1.0f))
        m_direction = { lDir[0], lDir[1], lDir[2] };

    ImGui::ColorEdit3((label + " Color").c_str(), reinterpret_cast<float*>(&m_color));
    ImGui::SliderFloat((label + " Intensity").c_str(), &m_intensity, 0.0f, 5.0f);

    // ─── ポイント・スポット・エリアの場合のみ Range を表示する
    if (m_type == LIGHT_TYPE::POINT || m_type == LIGHT_TYPE::SPOT || m_type == LIGHT_TYPE::AREA)
        ImGui::SliderFloat((label + " Range").c_str(), &m_range, 0.1f, 100.0f);

    // ─── スポットライト専用パラメーター
    if (m_type == LIGHT_TYPE::SPOT)
    {
        if (ImGui::SliderFloat((label + " Spot Angle").c_str(), &m_spotAngleDeg, 1.0f, 89.0f))
            CreateConeWire(1.0f, m_spotAngleDeg, 16);
        ImGui::SliderFloat((label + " Spot Falloff").c_str(), &m_spotFalloff, 0.01f, 1.0f);
    }

    // ─── エリアライト専用パラメーター
    if (m_type == LIGHT_TYPE::AREA)
    {
        ImGui::SliderFloat((label + " Area Width").c_str(), &m_areaWidth, 0.1f, 10.0f);
        ImGui::SliderFloat((label + " Area Height").c_str(), &m_areaHeight, 0.1f, 10.0f);
    }
}