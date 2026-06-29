// ============================================================
// RenderTexture.cpp
// オフスクリーンレンダリング用テクスチャクラスの実装
// カラー RTV / SRV とデプス DSV を持つレンダーターゲットを管理する
// 制作者：Chan WingChung
// ============================================================

#include "RenderTexture.h"

// ------------------------------------------------------------
// Initialize
// カラーバッファとデプスバッファを生成しレンダーターゲットを準備する
// 引数：pDevice - D3D11 デバイスポインタ
//        width   - テクスチャの横幅（ピクセル）
//        height  - テクスチャの縦幅（ピクセル）
// ------------------------------------------------------------
bool RENDER_TEXTURE::Initialize(ID3D11Device* pDevice, UINT width, UINT height)
{
    m_width = width;
    m_height = height;

    // ─── カラーバッファ用テクスチャを生成する
    // sRGB フォーマットで RTV と SRV の両方にバインドできる設定にする
    D3D11_TEXTURE2D_DESC lColorDesc = {};
    lColorDesc.Width = width;
    lColorDesc.Height = height;
    lColorDesc.MipLevels = 1;
    lColorDesc.ArraySize = 1;
    lColorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    lColorDesc.SampleDesc.Count = 1;
    lColorDesc.Usage = D3D11_USAGE_DEFAULT;
    lColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT lHrTex = pDevice->CreateTexture2D(&lColorDesc, nullptr, m_pColorTexture.GetAddressOf());
    if (FAILED(lHrTex)) return false;

    // ─── カラーバッファの RTV（レンダーターゲットビュー）を生成する
    HRESULT lHrRtv = pDevice->CreateRenderTargetView(m_pColorTexture.Get(), nullptr, m_pRtv.GetAddressOf());
    if (FAILED(lHrRtv)) return false;

    // ─── カラーバッファの SRV（シェーダーリソースビュー）を生成する
    HRESULT lHrSrv = pDevice->CreateShaderResourceView(m_pColorTexture.Get(), nullptr, m_pSrv.GetAddressOf());
    if (FAILED(lHrSrv)) return false;

    // ─── デプスバッファ用テクスチャを生成する
    D3D11_TEXTURE2D_DESC lDepthDesc = {};
    lDepthDesc.Width = width;
    lDepthDesc.Height = height;
    lDepthDesc.MipLevels = 1;
    lDepthDesc.ArraySize = 1;
    lDepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    lDepthDesc.SampleDesc.Count = 1;
    lDepthDesc.Usage = D3D11_USAGE_DEFAULT;
    lDepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT lHrDepthTex = pDevice->CreateTexture2D(&lDepthDesc, nullptr, m_pDepthTexture.GetAddressOf());
    if (FAILED(lHrDepthTex)) return false;

    // ─── デプスバッファの DSV（深度ステンシルビュー）を生成する
    HRESULT lHrDsv = pDevice->CreateDepthStencilView(m_pDepthTexture.Get(), nullptr, m_pDsv.GetAddressOf());
    if (FAILED(lHrDsv)) return false;

    return true;
}

// ------------------------------------------------------------
// BeginRender
// レンダーターゲットをセットしてバッファをクリアする
// 引数：pContext   - D3D11 デバイスコンテキストポインタ
//        clearColor - クリアカラー（nullptr の場合はデフォルト色を使用）
// ------------------------------------------------------------
void RENDER_TEXTURE::BeginRender(ID3D11DeviceContext* pContext, const float clearColor[4])
{
    // ─── clearColor が省略された場合のデフォルト色（青みがかったグレー）
    static const float lDefaultColor[4] = { 0.5f, 0.7f, 0.9f, 1.0f };
    const float* lColor = clearColor ? clearColor : lDefaultColor;

    // ─── レンダーターゲットをセットしてカラー・デプスをクリアする
    ID3D11RenderTargetView* lRtv = m_pRtv.Get();
    pContext->OMSetRenderTargets(1, &lRtv, m_pDsv.Get());

    pContext->ClearRenderTargetView(lRtv, lColor);
    pContext->ClearDepthStencilView(m_pDsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // ─── このテクスチャのサイズに合わせてビューポートを設定する
    D3D11_VIEWPORT lViewport = {};
    lViewport.Width = static_cast<float>(m_width);
    lViewport.Height = static_cast<float>(m_height);
    lViewport.MinDepth = 0.0f;
    lViewport.MaxDepth = 1.0f;
    pContext->RSSetViewports(1, &lViewport);
}