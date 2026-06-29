// ============================================================
// ShadowManager.cpp
// キューブ配列シャドウマップを管理するクラスの実装
// 全ライト分のキューブシャドウを TextureCubeArray に焼き込む
// 制作者：Chan WingChung
// ============================================================

#include "ShadowManager.h"
#include "Math/ConstantBuffers.h"

// ─── シャドウマップ生成パス専用の定数バッファ（b7）
// 1 フェイス分の描画に必要なライト位置と far 距離を持つ
struct CB_SHADOW_GEN
{
    DirectX::XMFLOAT3 genLightPos;
    float             genFarZ;
};

// ------------------------------------------------------------
// Initialize
// デバイスを保持して GPU リソースを生成する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool SHADOW_MANAGER::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;
    return CreateResources();
}

// ------------------------------------------------------------
// CreateResources
// TextureCubeArray・共有デプスバッファ・CB・サンプラーを生成する
// 引数：なし
// ------------------------------------------------------------
bool SHADOW_MANAGER::CreateResources()
{
    // ─── MAX_LIGHTS × 6 フェイス分のキューブ配列テクスチャを生成する
    const UINT lTotalSlices = 6 * MAX_LIGHTS;

    D3D11_TEXTURE2D_DESC lColorDesc = {};
    lColorDesc.Width = SHADOW_MAP_SIZE;
    lColorDesc.Height = SHADOW_MAP_SIZE;
    lColorDesc.MipLevels = 1;
    lColorDesc.ArraySize = lTotalSlices;
    lColorDesc.Format = DXGI_FORMAT_R32_FLOAT;
    lColorDesc.SampleDesc = { 1, 0 };
    lColorDesc.Usage = D3D11_USAGE_DEFAULT;
    lColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    lColorDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    if (FAILED(m_pDevice->CreateTexture2D(&lColorDesc, nullptr, m_pCubeArrayTexture.GetAddressOf())))
        return false;

    // ─── 各スライス（フェイス）に対して個別の RTV を生成する
    m_cubeRtvs.resize(lTotalSlices);
    for (UINT lSlice = 0; lSlice < lTotalSlices; lSlice++)
    {
        D3D11_RENDER_TARGET_VIEW_DESC lRtvDesc = {};
        lRtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        lRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        lRtvDesc.Texture2DArray.MipSlice = 0;
        lRtvDesc.Texture2DArray.FirstArraySlice = lSlice;
        lRtvDesc.Texture2DArray.ArraySize = 1;

        if (FAILED(m_pDevice->CreateRenderTargetView(
            m_pCubeArrayTexture.Get(), &lRtvDesc, m_cubeRtvs[lSlice].GetAddressOf())))
            return false;
    }

    // ─── 全キューブを参照できる TextureCubeArray SRV を生成する
    D3D11_SHADER_RESOURCE_VIEW_DESC lSrvDesc = {};
    lSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    lSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
    lSrvDesc.TextureCubeArray.MostDetailedMip = 0;
    lSrvDesc.TextureCubeArray.MipLevels = 1;
    lSrvDesc.TextureCubeArray.First2DArrayFace = 0;
    lSrvDesc.TextureCubeArray.NumCubes = MAX_LIGHTS;

    if (FAILED(m_pDevice->CreateShaderResourceView(
        m_pCubeArrayTexture.Get(), &lSrvDesc, m_pCubeArraySrv.GetAddressOf())))
        return false;

    // ─── 全フェイスで共有するデプスバッファを生成する（ArraySize = 1）
    D3D11_TEXTURE2D_DESC lDepthDesc = {};
    lDepthDesc.Width = SHADOW_MAP_SIZE;
    lDepthDesc.Height = SHADOW_MAP_SIZE;
    lDepthDesc.MipLevels = 1;
    lDepthDesc.ArraySize = 1;
    lDepthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    lDepthDesc.SampleDesc = { 1, 0 };
    lDepthDesc.Usage = D3D11_USAGE_DEFAULT;
    lDepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    if (FAILED(m_pDevice->CreateTexture2D(&lDepthDesc, nullptr, m_pDepthTexture.GetAddressOf())))
        return false;

    D3D11_DEPTH_STENCIL_VIEW_DESC lDsvDesc = {};
    lDsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    lDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    lDsvDesc.Texture2D.MipSlice = 0;

    if (FAILED(m_pDevice->CreateDepthStencilView(
        m_pDepthTexture.Get(), &lDsvDesc, m_pDepthDsv.GetAddressOf())))
        return false;

    // ─── シャドウデータ（ライト位置・farZ）の定数バッファを生成する
    D3D11_BUFFER_DESC lCbDesc = {};
    lCbDesc.ByteWidth = sizeof(CB_SHADOW);
    lCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lCbDesc, nullptr, m_pShadowCb.GetAddressOf())))
        return false;

    // ─── 1 フェイス描画時にライト情報を渡す生成パス専用の CB を生成する
    D3D11_BUFFER_DESC lGenDesc = {};
    lGenDesc.ByteWidth = sizeof(CB_SHADOW_GEN);
    lGenDesc.Usage = D3D11_USAGE_DEFAULT;
    lGenDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lGenDesc, nullptr, m_pShadowGenCb.GetAddressOf())))
        return false;

    // ─── シャドウマップをサンプリングするためのサンプラーを生成する
    D3D11_SAMPLER_DESC lSampDesc = {};
    lSampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    lSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    lSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    lSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    lSampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    lSampDesc.MinLOD = 0;
    lSampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_pDevice->CreateSamplerState(&lSampDesc, m_pSampler.GetAddressOf())))
        return false;

    return true;
}

// ------------------------------------------------------------
// Execute
// 全ライト分のシャドウマップを生成して CB_SHADOW を更新する
// 引数：context - シャドウパスに必要なライト・モデル・RTV/DSV 情報
// ------------------------------------------------------------
void SHADOW_MANAGER::Execute(const SHADOW_PASS_CONTEXT& context)
{
    if (!context.pShadowShader || context.lights.empty())
        return;

    // ─── シャドウマップ解像度に合わせてビューポートを設定する
    D3D11_VIEWPORT lShadowVp = {};
    lShadowVp.Width = static_cast<float>(SHADOW_MAP_SIZE);
    lShadowVp.Height = static_cast<float>(SHADOW_MAP_SIZE);
    lShadowVp.MaxDepth = 1.0f;
    m_pContext->RSSetViewports(1, &lShadowVp);

    // ─── 各ライトのキューブシャドウをそれぞれ 6 フェイス分描画する
    int lLightCount = static_cast<int>(context.lights.size());
    if (lLightCount > MAX_LIGHTS) lLightCount = MAX_LIGHTS;

    for (int lI = 0; lI < lLightCount; lI++)
        RenderLightCube(context, lI);

    // ─── ライティングパスで使う CB_SHADOW（ライト位置・farZ）を更新する
    CB_SHADOW lCbData = {};
    lCbData.shadowCount = lLightCount;
    for (int lI = 0; lI < lLightCount; lI++)
    {
        const VECTOR3& lPos = context.lights[lI]->GetPosition();
        lCbData.shadows[lI].lightPos = { lPos.x, lPos.y, lPos.z };
        lCbData.shadows[lI].farZ = context.lights[lI]->GetRange();
    }
    m_pContext->UpdateSubresource(m_pShadowCb.Get(), 0, nullptr, &lCbData, 0, 0);

    // ─── シャドウパス終了後にメインの RTV/DSV を復元する
    RestoreMainTarget(context);
}

// ------------------------------------------------------------
// RenderLightCube
// 指定ライトのキューブシャドウマップを 6 フェイス分描画する
// 引数：context    - シャドウパスコンテキスト
//        lightIndex - 描画対象のライトインデックス
// ------------------------------------------------------------
void SHADOW_MANAGER::RenderLightCube(const SHADOW_PASS_CONTEXT& context, int lightIndex)
{
    LIGHT* lLight = context.lights[lightIndex];
    if (!lLight) return;

    // ─── シャドウ生成パス用の CB（ライト位置・farZ）を b7 にバインドする
    CB_SHADOW_GEN lGen = {};
    const VECTOR3& lLp = lLight->GetPosition();
    lGen.genLightPos = { lLp.x, lLp.y, lLp.z };
    lGen.genFarZ = lLight->GetRange();
    m_pContext->UpdateSubresource(m_pShadowGenCb.Get(), 0, nullptr, &lGen, 0, 0);
    ID3D11Buffer* lGenCb = m_pShadowGenCb.Get();
    m_pContext->PSSetConstantBuffers(7, 1, &lGenCb);

    // ─── 6 方向（+X/-X/+Y/-Y/+Z/-Z）を順に描画する
    for (UINT lFace = 0; lFace < 6; lFace++)
    {
        UINT lSlice = lightIndex * 6 + lFace;

        // ─── このフェイスの RTV をセットしてクリアする
        ID3D11RenderTargetView* lRtvs[] = { m_cubeRtvs[lSlice].Get() };
        m_pContext->OMSetRenderTargets(1, lRtvs, m_pDepthDsv.Get());

        const float lClearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_pContext->ClearRenderTargetView(m_cubeRtvs[lSlice].Get(), lClearColor);
        m_pContext->ClearDepthStencilView(
            m_pDepthDsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // ─── ライトの視点行列と投影行列を合成して描画に使用する
        MATRIX4X4 lCubeViewProj = lLight->GetCubeView(lFace) * lLight->GetCubeProjection();

        for (auto* lModel : context.models)
        {
            if (lModel)
                lModel->DrawShadow(
                    lCubeViewProj,
                    context.pShadowShader,
                    context.pShadowSkinnedShader);
        }

        if (context.pField)
            context.pField->DrawShadow(lCubeViewProj, context.pShadowShader);

        if (context.pMirror)
            context.pMirror->DrawShadow(lCubeViewProj, context.pShadowShader);

        if (context.pCube)
            context.pCube->DrawShadow(lCubeViewProj, context.pShadowShader);
    }
}

// ------------------------------------------------------------
// BindForLighting
// ライティングパス用に TextureCubeArray・CB・サンプラーをバインドする
// 引数：cubeArraySlot - SRV を設定する PS テクスチャスロット（デフォルト 2）
//        cbSlot        - CB_SHADOW を設定する定数バッファスロット（デフォルト 2）
//        samplerSlot   - サンプラーを設定するスロット（デフォルト 1）
// ------------------------------------------------------------
void SHADOW_MANAGER::BindForLighting(UINT cubeArraySlot, UINT cbSlot, UINT samplerSlot)
{
    // ─── シャドウ距離データが入った SRV を PS テクスチャスロットにバインドする
    ID3D11ShaderResourceView* lSrv = m_pCubeArraySrv.Get();
    m_pContext->PSSetShaderResources(cubeArraySlot, 1, &lSrv);

    // ─── ライト位置・farZ データの CB をバインドする
    ID3D11Buffer* lCb = m_pShadowCb.Get();
    m_pContext->PSSetConstantBuffers(cbSlot, 1, &lCb);

    // ─── シャドウサンプリング用のサンプラーをバインドする
    ID3D11SamplerState* lSamp = m_pSampler.Get();
    m_pContext->PSSetSamplers(samplerSlot, 1, &lSamp);
}

// ------------------------------------------------------------
// RestoreMainTarget
// シャドウパス後にメインの RTV/DSV とビューポートを復元する
// 引数：context - メインの RTV・DSV・解像度を持つコンテキスト
// ------------------------------------------------------------
void SHADOW_MANAGER::RestoreMainTarget(const SHADOW_PASS_CONTEXT& context)
{
    // ─── メインレンダーターゲットを復元する
    ID3D11RenderTargetView* lRtvs[] = { context.pMainRtv };
    m_pContext->OMSetRenderTargets(1, lRtvs, context.pMainDsv);

    // ─── メイン画面のビューポートを復元する
    D3D11_VIEWPORT lMainVp = {};
    lMainVp.Width = static_cast<float>(context.mainWidth);
    lMainVp.Height = static_cast<float>(context.mainHeight);
    lMainVp.MaxDepth = 1.0f;
    m_pContext->RSSetViewports(1, &lMainVp);
}