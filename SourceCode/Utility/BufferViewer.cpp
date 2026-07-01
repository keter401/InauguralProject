// ============================================================
// BufferViewer.cpp
// 中間バッファビューアの実装
// 制作者：Chan WingChung
// ============================================================
#include "BufferViewer.h"
#include "Imgui/imgui.h"          // ← 既存の include 規約に合わせて調整
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

// ------------------------------------------------------------
// Initialize
// シェーダー・サンプラー・ステートを生成する
// ------------------------------------------------------------
bool BUFFER_VIEWER::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    if (!pDevice || !pContext) return false;
    m_pDevice = pDevice;
    m_pContext = pContext;

    ComPtr<ID3DBlob> lBlob, lErr;

    // ─── 全画面 VS
    if (FAILED(D3DCompileFromFile(L"Shaders/ViewerVS.hlsl", nullptr, nullptr,
        "main", "vs_5_0", 0, 0, &lBlob, &lErr))) return false;
    if (FAILED(m_pDevice->CreateVertexShader(lBlob->GetBufferPointer(), lBlob->GetBufferSize(), nullptr, &m_pVs)))
        return false;

    // ─── 2D 用 PS
    if (FAILED(D3DCompileFromFile(L"Shaders/ViewerPS2D.hlsl", nullptr, nullptr,
        "main", "ps_5_0", 0, 0, &lBlob, &lErr))) return false;
    if (FAILED(m_pDevice->CreatePixelShader(lBlob->GetBufferPointer(), lBlob->GetBufferSize(), nullptr, &m_pPs2D)))
        return false;

    // ─── キューブ用 PS
    if (FAILED(D3DCompileFromFile(L"Shaders/ViewerPS.hlsl", nullptr, nullptr,
        "main", "ps_5_0", 0, 0, &lBlob, &lErr))) return false;
    if (FAILED(m_pDevice->CreatePixelShader(lBlob->GetBufferPointer(), lBlob->GetBufferSize(), nullptr, &m_pPsCube)))
        return false;

    // ─── キューブ配列用 PS
    if (FAILED(D3DCompileFromFile(L"Shaders/ViewerPSCube.hlsl", nullptr, nullptr,
        "main", "ps_5_0", 0, 0, &lBlob, &lErr)))
    {
        if (lErr) OutputDebugStringA(static_cast<const char*>(lErr->GetBufferPointer()));
        return false;
    }
    if (FAILED(m_pDevice->CreatePixelShader(lBlob->GetBufferPointer(), lBlob->GetBufferSize(), nullptr, &m_pPsCubeArray)))
        return false;

    // ─── キューブ番号を渡す定数バッファ
    D3D11_BUFFER_DESC lCbDesc = {};
    lCbDesc.ByteWidth = sizeof(CB_VIEWER_DATA);
    lCbDesc.Usage = D3D11_USAGE_DEFAULT;
    lCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(m_pDevice->CreateBuffer(&lCbDesc, nullptr, &m_pCb)))
        return false;

    // ─── サンプラー（線形・クランプ）
    D3D11_SAMPLER_DESC lSampDesc = {};
    lSampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    lSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    lSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    lSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (FAILED(m_pDevice->CreateSamplerState(&lSampDesc, &m_pSampler))) return false;

    // ─── 深度無効（バッファを常に上書き表示する）
    D3D11_DEPTH_STENCIL_DESC lDepthDesc = {};
    lDepthDesc.DepthEnable = FALSE;
    if (FAILED(m_pDevice->CreateDepthStencilState(&lDepthDesc, &m_pDepthOff))) return false;

    // ─── カリング無し
    D3D11_RASTERIZER_DESC lRasterDesc = {};
    lRasterDesc.FillMode = D3D11_FILL_SOLID;
    lRasterDesc.CullMode = D3D11_CULL_NONE;
    if (FAILED(m_pDevice->CreateRasterizerState(&lRasterDesc, &m_pRaster))) return false;

    m_bInitialized = true;
    return true;
}

// ------------------------------------------------------------
// AddSource
// 表示候補を登録する（起動時に1回ずつ）
// 引数：name - 表示名 / pSrv - SRV / type - 種類 / cubeCount - 配列本数
// ------------------------------------------------------------
void BUFFER_VIEWER::AddSource(const char* name, ID3D11ShaderResourceView* pSrv,
    VIEW_TYPE type, int cubeCount)
{
    SOURCE lSource;
    lSource.name = name;
    lSource.pSrv = pSrv;
    lSource.type = type;
    lSource.cubeCount = cubeCount;
    m_sources.push_back(lSource);
}

// ------------------------------------------------------------
// DrawImGui
// 表示対象を選ぶコンボ（Final ＝ 通常の最終画）
// ------------------------------------------------------------
void BUFFER_VIEWER::DrawImGui()
{
    if (!m_bInitialized) return;

    if (ImGui::Begin("Buffer Viewer"))
    {
        // ─── "Final" ＋ 各ソース名でコンボを構築
        std::vector<const char*> lNames;
        lNames.push_back("Final");
        for (const SOURCE& lSource : m_sources) lNames.push_back(lSource.name.c_str());

        if (m_selected >= (int)lNames.size()) m_selected = 0;
        ImGui::Combo("Show", &m_selected, lNames.data(), (int)lNames.size());

        if (m_selected > 0)
        {
            const SOURCE& lSource = m_sources[m_selected - 1];
            ImGui::TextDisabled("Showing: %s (fullscreen)", lSource.name.c_str());

            // ─── キューブ配列のときだけ「どのキューブ（ライト）か」を選ぶ
            if (lSource.type == VIEW_CUBE_ARRAY && lSource.cubeCount > 1)
                ImGui::SliderInt("Cube Index (Light)", &m_cubeIndex, 0, lSource.cubeCount - 1);
        }
    }
    ImGui::End();
}

// ------------------------------------------------------------
// Execute
// 選択中の SRV を全画面へ描く（Final のときは何もしない）
// 引数：pRtv - 出力先 / width, height - ビューポート
// ------------------------------------------------------------
void BUFFER_VIEWER::Execute(ID3D11RenderTargetView* pRtv, UINT width, UINT height)
{
    if (!m_bInitialized || !pRtv) return;
    if (m_selected <= 0) return;                        // Final：何もしない

    int lIndex = m_selected - 1;
    if (lIndex >= (int)m_sources.size()) return;
    const SOURCE& lSource = m_sources[lIndex];
    if (!lSource.pSrv) return;

    // ─── 出力先とビューポート（深度は使わない）
    m_pContext->OMSetRenderTargets(1, &pRtv, nullptr);

    D3D11_VIEWPORT lViewport = {};
    lViewport.Width = static_cast<float>(width);
    lViewport.Height = static_cast<float>(height);
    lViewport.MaxDepth = 1.0f;
    m_pContext->RSSetViewports(1, &lViewport);

    // ─── 種類に応じて PS を選ぶ
    ID3D11PixelShader* lPs = m_pPs2D.Get();
    if (lSource.type == VIEW_CUBE)       lPs = m_pPsCube.Get();
    if (lSource.type == VIEW_CUBE_ARRAY) lPs = m_pPsCubeArray.Get();

    m_pContext->IASetInputLayout(nullptr);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pContext->VSSetShader(m_pVs.Get(), nullptr, 0);
    m_pContext->PSSetShader(lPs, nullptr, 0);

    // ─── キューブ配列のときはキューブ番号を送る
    if (lSource.type == VIEW_CUBE_ARRAY)
    {
        CB_VIEWER_DATA lData = {};
        lData.cubeIndex = m_cubeIndex;
        m_pContext->UpdateSubresource(m_pCb.Get(), 0, nullptr, &lData, 0, 0);

        ID3D11Buffer* lCb = m_pCb.Get();
        m_pContext->PSSetConstantBuffers(0, 1, &lCb);
    }

    ID3D11SamplerState* lSampler = m_pSampler.Get();
    m_pContext->PSSetSamplers(0, 1, &lSampler);
    ID3D11ShaderResourceView* lSrv = lSource.pSrv;
    m_pContext->PSSetShaderResources(0, 1, &lSrv);

    m_pContext->OMSetDepthStencilState(m_pDepthOff.Get(), 0);
    m_pContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    m_pContext->RSSetState(m_pRaster.Get());

    m_pContext->Draw(3, 0);

    // ─── SRV を外して深度を既定へ
    ID3D11ShaderResourceView* lNull = nullptr;
    m_pContext->PSSetShaderResources(0, 1, &lNull);
    m_pContext->OMSetDepthStencilState(nullptr, 0);
}