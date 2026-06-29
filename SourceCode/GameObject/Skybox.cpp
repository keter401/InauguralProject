// ============================================================
// Skybox.cpp
// HDR 環境マップをキューブボックスとして描画するクラスの実装
// 深度書き込みを無効にしてカメラ原点に固定されたキューブを描画する
// 制作者：Chan WingChung
// ============================================================

#include "SkyBox.h"

// ------------------------------------------------------------
// Initialize
// ジオメトリ・定数バッファ・深度ステンシルステートを生成する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool SKY_BOX::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;

    if (!CreateGeometry())     return false;
    if (!InitConstantBuffer()) return false;

    // ─── 深度書き込みを無効にして常に最奥面（ファークリップ）に描画する
    // LESS_EQUAL にすることでファークリップ境界でもクリップされないようにする
    D3D11_DEPTH_STENCIL_DESC lDsDesc = {};
    lDsDesc.DepthEnable = TRUE;
    lDsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    lDsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    lDsDesc.StencilEnable = FALSE;

    if (FAILED(m_pDevice->CreateDepthStencilState(&lDsDesc, &m_pDepthState)))
        return false;

    return true;
}

// ------------------------------------------------------------
// CreateGeometry
// スカイボックス用の単位キューブの頂点バッファとインデックスバッファを生成する
// 引数：なし
// ------------------------------------------------------------
bool SKY_BOX::CreateGeometry()
{
    // ─── 6 面分の頂点データを定義する（法線は内向き・UV は赤道方向マッピング用）
    std::vector<VERTEX_3D> lVertices =
    {
        { -1, 1, 1,  0, 0,-1,  0,0,  1,0,0,  0,0,-1 },
        {  1, 1, 1,  0, 0,-1,  1,0,  1,0,0,  0,0,-1 },
        {  1,-1, 1,  0, 0,-1,  1,1,  1,0,0,  0,0,-1 },
        { -1,-1, 1,  0, 0,-1,  0,1,  1,0,0,  0,0,-1 },

        {  1, 1,-1,  0, 0, 1,  0,0, -1,0,0,  0,0, 1 },
        { -1, 1,-1,  0, 0, 1,  1,0, -1,0,0,  0,0, 1 },
        { -1,-1,-1,  0, 0, 1,  1,1, -1,0,0,  0,0, 1 },
        {  1,-1,-1,  0, 0, 1,  0,1, -1,0,0,  0,0, 1 },

        {  1, 1, 1, -1, 0, 0,  0,0,  0,0, 1, -1,0,0 },
        {  1, 1,-1, -1, 0, 0,  1,0,  0,0, 1, -1,0,0 },
        {  1,-1,-1, -1, 0, 0,  1,1,  0,0, 1, -1,0,0 },
        {  1,-1, 1, -1, 0, 0,  0,1,  0,0, 1, -1,0,0 },

        { -1, 1,-1,  1, 0, 0,  0,0,  0,0,-1,  1,0,0 },
        { -1, 1, 1,  1, 0, 0,  1,0,  0,0,-1,  1,0,0 },
        { -1,-1, 1,  1, 0, 0,  1,1,  0,0,-1,  1,0,0 },
        { -1,-1,-1,  1, 0, 0,  0,1,  0,0,-1,  1,0,0 },

        { -1, 1,-1,  0,-1, 0,  0,0,  1,0,0,  0,-1,0 },
        {  1, 1,-1,  0,-1, 0,  1,0,  1,0,0,  0,-1,0 },
        {  1, 1, 1,  0,-1, 0,  1,1,  1,0,0,  0,-1,0 },
        { -1, 1, 1,  0,-1, 0,  0,1,  1,0,0,  0,-1,0 },

        { -1,-1, 1,  0, 1, 0,  0,0,  1,0,0,  0, 1,0 },
        {  1,-1, 1,  0, 1, 0,  1,0,  1,0,0,  0, 1,0 },
        {  1,-1,-1,  0, 1, 0,  1,1,  1,0,0,  0, 1,0 },
        { -1,-1,-1,  0, 1, 0,  0,1,  1,0,0,  0, 1,0 },
    };

    // ─── 6 面 × 2 三角形分のインデックスを生成する
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
// WVP 定数バッファを生成する
// 引数：なし
// ------------------------------------------------------------
bool SKY_BOX::InitConstantBuffer()
{
    // ─── CB_WVP サイズの定数バッファを生成する
    D3D11_BUFFER_DESC lCbDesc = {};
    lCbDesc.ByteWidth = sizeof(CB_WVP);
    lCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return !FAILED(m_pDevice->CreateBuffer(&lCbDesc, nullptr, &m_pConstantBuffer));
}

// ------------------------------------------------------------
// Draw
// 通常描画パスでスカイボックスを描画する
// 引数：なし
// ------------------------------------------------------------
void SKY_BOX::Draw()
{
    if (!m_pCamera || !m_pShader || !m_pHdrSrv) return;

    // ─── 深度書き込みを無効にする（最奥面に固定するため）
    m_pContext->OMSetDepthStencilState(m_pDepthState.Get(), 0);

    m_pShader->Bind(m_pContext);

    // ─── ワールド行列はスケールのみ（移動なし・カメラ原点に固定）
    MATRIX4X4 lWorld = MATRIX4X4::Scale(m_size, m_size, m_size);

    m_cbData.world = lWorld.Transpose().m_mat;
    m_cbData.wvp = m_pCamera->GetWVP(lWorld).m_mat;
    m_cbData.materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_cbData.useTexture = 1;
    m_cbData.useNormalMap = 0;

    const VECTOR3& lEye = m_pCamera->GetPosition();
    m_cbData.cameraPos = { lEye.x, lEye.y, lEye.z };
    m_cbData.useRimLight = 0;
    m_cbData.useToonLightTexture = 0;
    m_cbData.useToonDarkTexture = 0;

    // ─── 定数バッファを更新して VS・PS にバインドする
    m_pContext->UpdateSubresource(m_pConstantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);
    ID3D11Buffer* lCb = m_pConstantBuffer.Get();
    m_pContext->VSSetConstantBuffers(0, 1, &lCb);
    m_pContext->PSSetConstantBuffers(0, 1, &lCb);

    // ─── HDR テクスチャを t0 にバインドしてキューブを描画する
    m_pContext->PSSetShaderResources(0, 1, &m_pHdrSrv);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT lStride = sizeof(VERTEX_3D);
    UINT lOffset = 0;
    ID3D11Buffer* lVb = m_pVertexBuffer.Get();
    m_pContext->IASetVertexBuffers(0, 1, &lVb, &lStride, &lOffset);
    m_pContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_pContext->DrawIndexed(m_indexCount, 0, 0);

    // ─── 深度ステンシルステートをデフォルトに戻す
    m_pContext->OMSetDepthStencilState(nullptr, 0);
}

// ------------------------------------------------------------
// DrawWithViewProj
// ミラー反射パスで使用する外部の viewProj と eye でスカイボックスを描画する
// 引数：viewProj - 反射後のビュー × プロジェクション行列
//        eye      - 反射後のカメラ位置
// ------------------------------------------------------------
void SKY_BOX::DrawWithViewProj(const MATRIX4X4& viewProj, const VECTOR3& eye)
{
    if (!m_pCamera || !m_pShader || !m_pHdrSrv) return;

    // ─── 深度書き込みを無効にする
    m_pContext->OMSetDepthStencilState(m_pDepthState.Get(), 0);

    m_pShader->Bind(m_pContext);

    // ─── 外部から渡された viewProj を使って WVP を計算する（反射パス対応）
    MATRIX4X4 lWorld = MATRIX4X4::Scale(m_size, m_size, m_size);

    m_cbData.world = lWorld.Transpose().m_mat;
    m_cbData.wvp = (lWorld * viewProj).Transpose().m_mat;
    m_cbData.materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_cbData.useTexture = 1;
    m_cbData.useNormalMap = 0;
    m_cbData.cameraPos = { eye.x, eye.y, eye.z };
    m_cbData.useRimLight = 0;
    m_cbData.useToonLightTexture = 0;
    m_cbData.useToonDarkTexture = 0;

    // ─── 定数バッファを更新して VS・PS にバインドする
    m_pContext->UpdateSubresource(m_pConstantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);
    ID3D11Buffer* lCb = m_pConstantBuffer.Get();
    m_pContext->VSSetConstantBuffers(0, 1, &lCb);
    m_pContext->PSSetConstantBuffers(0, 1, &lCb);

    // ─── HDR テクスチャを t0 にバインドしてキューブを描画する
    m_pContext->PSSetShaderResources(0, 1, &m_pHdrSrv);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT lStride = sizeof(VERTEX_3D);
    UINT lOffset = 0;
    ID3D11Buffer* lVb = m_pVertexBuffer.Get();
    m_pContext->IASetVertexBuffers(0, 1, &lVb, &lStride, &lOffset);
    m_pContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_pContext->DrawIndexed(m_indexCount, 0, 0);

    // ─── 深度ステンシルステートをデフォルトに戻す
    m_pContext->OMSetDepthStencilState(nullptr, 0);
}