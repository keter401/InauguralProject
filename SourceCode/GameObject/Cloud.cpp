// ============================================================
// Cloud.cpp
// レイマーチング雲パスの実装
// 制作者：Chan WingChung
// ============================================================
#include "Cloud.h"
#include "GameObject/Camera.h"
#include "Math/Matrix4x4.h"
#include "Imgui/imgui.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ------------------------------------------------------------
// Initialize
// シェーダー・ステート・定数バッファを生成する
// 引数：pDevice / pContext - D3D11 デバイスとコンテキスト
// ------------------------------------------------------------
bool CLOUD::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    if (!pDevice || !pContext) return false;
    m_pDevice = pDevice;
    m_pContext = pContext;

    // ─── VS/PS をランタイムコンパイル（実行時コンパイル方針に合わせる）
    ComPtr<ID3DBlob> lVsBlob, lPsBlob, lErr;

    if (FAILED(D3DCompileFromFile(L"Shaders/CloudVS.hlsl", nullptr, nullptr,
        "main", "vs_5_0", 0, 0, &lVsBlob, &lErr)))
        return false;
    if (FAILED(m_pDevice->CreateVertexShader(
        lVsBlob->GetBufferPointer(), lVsBlob->GetBufferSize(), nullptr, &m_pVs)))
        return false;

    if (FAILED(D3DCompileFromFile(L"Shaders/CloudPS.hlsl", nullptr, nullptr,
        "main", "ps_5_0", 0, 0, &lPsBlob, &lErr)))
        return false;
    if (FAILED(m_pDevice->CreatePixelShader(
        lPsBlob->GetBufferPointer(), lPsBlob->GetBufferSize(), nullptr, &m_pPs)))
        return false;

    // ─── 定数バッファ
    D3D11_BUFFER_DESC lCbDesc = {};
    lCbDesc.ByteWidth = sizeof(CB_CLOUD_DATA);
    lCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lCbDesc, nullptr, &m_pCb)))
        return false;

    // ─── アルファ合成（src.a, 1-src.a）
    D3D11_BLEND_DESC lBlendDesc = {};
    lBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    lBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    lBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    lBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    lBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    lBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    lBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    lBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_pDevice->CreateBlendState(&lBlendDesc, &m_pBlendState)))
        return false;

    // ─── 深度：テスト有効・書き込み無効・LESS_EQUAL（背景に隠される）
    D3D11_DEPTH_STENCIL_DESC lDepthDesc = {};
    lDepthDesc.DepthEnable = TRUE;
    lDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    lDepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(m_pDevice->CreateDepthStencilState(&lDepthDesc, &m_pDepthState)))
        return false;

    // ─── ラスタライザ：全画面三角形なのでカリング無し
    D3D11_RASTERIZER_DESC lRasterDesc = {};
    lRasterDesc.FillMode = D3D11_FILL_SOLID;
    lRasterDesc.CullMode = D3D11_CULL_NONE;
    if (FAILED(m_pDevice->CreateRasterizerState(&lRasterDesc, &m_pRasterState)))
        return false;

    m_bInitialized = true;
    return true;
}

// ------------------------------------------------------------
// Execute
// 指定 RTV/DSV に対して全画面の雲パスを描く
// 引数：pRtv/pDsv    - 合成先（メインバッファと深度）
//       pCamera      - ビュー/プロジェクション/カメラ位置の取得元
//       timeSeconds  - アニメ用の経過時間
//       width/height - ビューポート
// ------------------------------------------------------------
void CLOUD::Execute(ID3D11RenderTargetView* pRtv, ID3D11DepthStencilView* pDsv,
    CAMERA* pCamera, float timeSeconds, UINT width, UINT height)
{
    if (!m_bInitialized || !pCamera || !pRtv || !pDsv) return;

    // ─── 逆 ViewProj を計算（clip→world のレイ復元用）
    MATRIX4X4 lViewProj = pCamera->GetViewMatrix() * pCamera->GetProjectionMatrix();
    XMMATRIX  lInv = XMMatrixInverse(nullptr, lViewProj.ToXMMatrix());
    MATRIX4X4 lInvVp(lInv);

    // ─── 定数バッファ更新（行列は既存規約どおり転置して送る）
    const VECTOR3& lPos = pCamera->GetPosition();

    CB_CLOUD_DATA lData = {};
    lData.invViewProj = lInvVp.Transpose().m_mat;
    lData.camPos = XMFLOAT3(lPos.x, lPos.y, lPos.z);
    lData.time = timeSeconds;
    lData.sunDir = XMFLOAT3(m_sunDir[0], m_sunDir[1], m_sunDir[2]);
    lData.coverage = m_coverage;
    lData.cloudBottom = m_cloudBottom;
    lData.cloudTop = m_cloudTop;
    lData.marchSteps = m_marchSteps;
    lData.lightSteps = m_lightSteps;
    lData.densityMult = m_densityMult;
    lData.wind = m_wind;
    m_pContext->UpdateSubresource(m_pCb.Get(), 0, nullptr, &lData, 0, 0);

    // ─── 合成先と全画面ビューポートを明示（前パスの状態に依存しない）
    m_pContext->OMSetRenderTargets(1, &pRtv, pDsv);

    D3D11_VIEWPORT lVp = {};
    lVp.Width = static_cast<float>(width);
    lVp.Height = static_cast<float>(height);
    lVp.MaxDepth = 1.0f;
    m_pContext->RSSetViewports(1, &lVp);

    // ─── 全画面三角形（頂点バッファ無し）
    m_pContext->IASetInputLayout(nullptr);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pContext->VSSetShader(m_pVs.Get(), nullptr, 0);
    m_pContext->PSSetShader(m_pPs.Get(), nullptr, 0);

    ID3D11Buffer* lCb = m_pCb.Get();
    m_pContext->PSSetConstantBuffers(0, 1, &lCb);

    // ─── ステート適用
    float lBlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_pContext->OMSetBlendState(m_pBlendState.Get(), lBlendFactor, 0xffffffff);
    m_pContext->OMSetDepthStencilState(m_pDepthState.Get(), 0);
    m_pContext->RSSetState(m_pRasterState.Get());

    // ─── 全画面を1発描画
    m_pContext->Draw(3, 0);

    // ─── ブレンド・深度を既定へ戻す（後続の ImGui・次フレームに影響させない）
    m_pContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    m_pContext->OMSetDepthStencilState(nullptr, 0);
}

// ------------------------------------------------------------
// DrawImGui
// 雲パラメータを実行時に調整するウィンドウ
// ------------------------------------------------------------
void CLOUD::DrawImGui()
{
    if (!m_bInitialized) return;

    if (ImGui::Begin("Cloud"))
    {
        // ─── 見た目
        ImGui::SliderFloat("Coverage", &m_coverage, 0.0f, 1.0f);
        ImGui::SliderFloat("Density", &m_densityMult, 0.1f, 4.0f);
        ImGui::SliderFloat("Wind", &m_wind, 0.0f, 30.0f);

        ImGui::Separator();
        // ─── 太陽方向（逆光でシルバーラインが変わる）
        ImGui::SliderFloat3("Sun Dir", m_sunDir, -1.0f, 1.0f);

        ImGui::Separator();
        // ─── 雲の高度（スラブの上下端）
        ImGui::SliderFloat("Cloud Bottom", &m_cloudBottom, 0.0f, 200.0f);
        ImGui::SliderFloat("Cloud Top", &m_cloudTop, 0.0f, 300.0f);
        if (m_cloudTop < m_cloudBottom + 1.0f)
            m_cloudTop = m_cloudBottom + 1.0f;

        ImGui::Separator();
        // ─── 負荷（プロファイラの Cloud を見ながら調整）
        ImGui::SliderInt("March Steps", &m_marchSteps, 8, 96);
        ImGui::SliderInt("Light Steps", &m_lightSteps, 1, 12);
    }
    ImGui::End();
}