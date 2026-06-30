// ============================================================
// Polygon.cpp
// 汎用ポリゴン描画クラスの実装
// 通常・反射・シャドウの各描画パスに対応し FIELD / MIRROR の基底となる
// 制作者：Chan WingChung
// ============================================================

#include "Polygon.h"

// ------------------------------------------------------------
// Initialize
// D3D11 デバイスを保持して定数バッファと GPU バッファを生成する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool POLYGON::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;

    // ─── 定数バッファを生成してから頂点・インデックスバッファを生成する
    if (!InitConstantBuffer()) return false;
    if (!BuildBuffers())       return false;

    return true;
}

// ------------------------------------------------------------
// Draw
// 通常描画パスで DrawInternal を呼び出す
// 引数：なし
// ------------------------------------------------------------
void POLYGON::Draw()
{
    DrawInternal(nullptr);
}

// ------------------------------------------------------------
// DrawWithViewProj
// 反射パス用に overrideEye を保存してから DrawInternal に渡す
// 引数：viewProj - 反射後のビュー × プロジェクション行列
//        eye      - 反射後のカメラ位置
// ------------------------------------------------------------
void POLYGON::DrawWithViewProj(const MATRIX4X4& viewProj, const VECTOR3& eye)
{
    // ─── 反射後の Eye を保存して内部描画に渡す
    m_overrideEye = eye;
    DrawInternal(&viewProj);
}

// ------------------------------------------------------------
// DrawInternal
// CB 設定・シェーダーバインド・IA バインドを行いポリゴンを描画する内部関数
// 引数：pOverrideViewProj - 反射パス用 viewProj（nullptr なら通常カメラを使用）
// ------------------------------------------------------------
void POLYGON::DrawInternal(const MATRIX4X4* pOverrideViewProj)
{
    // ─── シェーダーをバインドする
    if (m_pShader)
        m_pShader->Bind(m_pContext);

    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    MATRIX4X4 lWorld = CalcWorldMatrix();

    // ─── world 行列と WVP 行列を設定する（反射パスは override viewProj を使用）
    m_cbData.world = lWorld.Transpose().m_mat;
    m_cbData.wvp = pOverrideViewProj
        ? (lWorld * (*pOverrideViewProj)).Transpose().m_mat
        : m_pCamera->GetWVP(lWorld).m_mat;

    m_cbData.materialColor = { 0.6f, 0.6f, 0.6f, 1.0f };
    m_cbData.useTexture = 0;

    // ─── 反射パスでは反射後の Eye、通常描画では実カメラ位置を使う
    if (pOverrideViewProj)
    {
        m_cbData.cameraPos = { m_overrideEye.x, m_overrideEye.y, m_overrideEye.z };
    }
    else
    {
        const VECTOR3& lEye = m_pCamera->GetPosition();
        m_cbData.cameraPos = { lEye.x, lEye.y, lEye.z };
    }

    // ─── サブクラスのマテリアル設定フック（MIRROR は反射テクスチャを設定する）
    SetupMaterialCb(m_cbData);

    // ─── 定数バッファを GPU に転送して VS・PS にバインドする
    m_pContext->UpdateSubresource(m_pConstantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);

    ID3D11Buffer* lCb = m_pConstantBuffer.Get();
    m_pContext->VSSetConstantBuffers(0, 1, &lCb);
    m_pContext->PSSetConstantBuffers(0, 1, &lCb);

    // ─── このオブジェクトはディゾルブしないので b5 を OFF で上書きする
    ID3D11Buffer* lDisOff = m_pDissolveOffCb.Get();
    m_pContext->PSSetConstantBuffers(5, 1, &lDisOff);

    // ─── サブクラスの追加 CB バインドフック（MIRROR はスクリーンサイズ CB を設定する）
    BindExtraCb();

    // ─── IA に頂点・インデックスバッファをセットして描画する
    UINT lStride = sizeof(VERTEX_3D);
    UINT lOffset = 0;
    ID3D11Buffer* lVb = m_pVertexBuffer.Get();
    m_pContext->IASetVertexBuffers(0, 1, &lVb, &lStride, &lOffset);
    m_pContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_pContext->DrawIndexed(m_indexCount, 0, 0);
}

// ------------------------------------------------------------
// DrawShadow
// キューブシャドウマップ生成パスでポリゴンを描画する
// 引数：cubeViewProj      - キューブの 1 フェイス分のビュー × プロジェクション行列
//        pShadowCubeShader - キューブシャドウ生成に使用するシェーダー
// ------------------------------------------------------------
void POLYGON::DrawShadow(const MATRIX4X4& cubeViewProj, SHADER* pShadowCubeShader)
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
// SetMeshData
// 頂点とインデックスデータを CPU 側に保持する（BuildBuffers で GPU に転送する）
// 引数：vertices - 頂点データの配列
//        indices  - インデックスデータの配列
// ------------------------------------------------------------
void POLYGON::SetMeshData(const std::vector<VERTEX_3D>& vertices, const std::vector<UINT>& indices)
{
    // ─── CPU 側バッファにコピーする（GPU 転送は BuildBuffers で行う）
    m_vertices = vertices;
    m_indices = indices;
}

// ------------------------------------------------------------
// CalcWorldMatrix
// スケールと平行移動の合成ワールド行列を返す（POLYGON は回転なし）
// 引数：なし
// ------------------------------------------------------------
MATRIX4X4 POLYGON::CalcWorldMatrix() const
{
    // ─── Scale * Translation の順で合成する（回転はサブクラスでオーバーライドする）
    MATRIX4X4 lScale = MATRIX4X4::Scale(m_scale.x, m_scale.y, m_scale.z);
    MATRIX4X4 lTrans = MATRIX4X4::Translation(m_position.x, m_position.y, m_position.z);
    return lScale * lTrans;
}

// ------------------------------------------------------------
// InitConstantBuffer
// CB_WVP サイズの定数バッファを生成する
// 引数：なし
// ------------------------------------------------------------
bool POLYGON::InitConstantBuffer()
{
    // ─── 既存バッファを解放してから生成する
    D3D11_BUFFER_DESC lCbDesc = {};
    lCbDesc.ByteWidth = sizeof(CB_WVP);
    lCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return SUCCEEDED(m_pDevice->CreateBuffer(
        &lCbDesc, nullptr, m_pConstantBuffer.ReleaseAndGetAddressOf()));

    // ─── ディゾルブ OFF 用 CB を生成する
    CB_DISSOLVE lDissolveOff = {};            // dissolveEnabled = 0
    D3D11_BUFFER_DESC lDisDesc = {};
    lDisDesc.ByteWidth = sizeof(CB_DISSOLVE);
    lDisDesc.Usage = D3D11_USAGE_DEFAULT;
    lDisDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA lDisData = { &lDissolveOff };
    if (FAILED(m_pDevice->CreateBuffer(&lDisDesc, &lDisData, &m_pDissolveOffCb)))
        return false;
}

// ------------------------------------------------------------
// BuildBuffers
// CPU 側の頂点・インデックスデータを GPU バッファに転送する
// 引数：なし
// ------------------------------------------------------------
bool POLYGON::BuildBuffers()
{
    if (m_vertices.empty() || m_indices.empty()) return false;

    // ─── 既存バッファを解放してから頂点バッファを生成する
    D3D11_BUFFER_DESC lVbDesc = {};
    lVbDesc.ByteWidth = sizeof(VERTEX_3D) * static_cast<UINT>(m_vertices.size());
    lVbDesc.Usage = D3D11_USAGE_DEFAULT;
    lVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA lVbData = { m_vertices.data() };
    if (FAILED(m_pDevice->CreateBuffer(
        &lVbDesc, &lVbData, m_pVertexBuffer.ReleaseAndGetAddressOf())))
        return false;

    // ─── インデックスバッファを生成する
    D3D11_BUFFER_DESC lIbDesc = {};
    lIbDesc.ByteWidth = sizeof(UINT) * static_cast<UINT>(m_indices.size());
    lIbDesc.Usage = D3D11_USAGE_DEFAULT;
    lIbDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA lIbData = { m_indices.data() };
    if (FAILED(m_pDevice->CreateBuffer(
        &lIbDesc, &lIbData, m_pIndexBuffer.ReleaseAndGetAddressOf())))
        return false;

    // ─── インデックス数を更新する（DrawIndexed に使用）
    m_indexCount = static_cast<UINT>(m_indices.size());
    return true;
}