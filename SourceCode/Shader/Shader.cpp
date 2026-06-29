// ============================================================
// Shader.cpp
// 頂点シェーダー・ピクセルシェーダー・入力レイアウトを管理するクラスの実装
// HLSL ファイルをランタイムコンパイルして GPU リソースを生成する
// 制作者：Chan WingChung
// ============================================================

#include "Shader.h"
#include <Windows.h>

// ------------------------------------------------------------
// Initialize
// 指定した HLSL ファイルをコンパイルし GPU リソースを生成する  
// 引数：pDevice    - D3D11 デバイスポインタ
//        vsPath     - 頂点シェーダー HLSL ファイルパス
//        psPath     - ピクセルシェーダー HLSL ファイルパス
//        name       - シェーダー識別名
//        layoutType - 入力レイアウト種別（MODEL_3D / TANGENT / OUTLINE / GIZMO）
//        cullMode   - ラスタライザーのカリングモード
//        extraCb    - 追加定数バッファのフラグ（EXTRA_CB_XXX の組み合わせ）
// ------------------------------------------------------------
bool SHADER::Initialize(
    ID3D11Device* pDevice,
    const wchar_t* vsPath,
    const wchar_t* psPath,
    const std::string& name,
    INPUT_LAYOUT_TYPE layoutType,
    CULL_MODE cullMode,
    unsigned int extraCb)
{
    m_name = name;
    m_extraCb = extraCb;

    ComPtr<ID3DBlob> lVsBlob, lPsBlob, lErrBlob;

    // ─── 頂点シェーダーのコンパイル
    HRESULT lHrVs = D3DCompileFromFile(
        vsPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main", "vs_5_0", 0, 0,
        lVsBlob.GetAddressOf(), lErrBlob.GetAddressOf());
    if (FAILED(lHrVs))
    {
        if (lErrBlob)
            OutputDebugStringA(static_cast<char*>(lErrBlob->GetBufferPointer()));
        return false;
    }

    // ─── ピクセルシェーダーのコンパイル
    HRESULT lHrPs = D3DCompileFromFile(
        psPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main", "ps_5_0", 0, 0,
        lPsBlob.GetAddressOf(), lErrBlob.GetAddressOf());
    if (FAILED(lHrPs))
    {
        if (lErrBlob)
            OutputDebugStringA(static_cast<char*>(lErrBlob->GetBufferPointer()));
        return false;
    }

    // ─── コンパイル済みバイトコードから GPU シェーダーオブジェクトを生成
    HRESULT lHrVsObj = pDevice->CreateVertexShader(
        lVsBlob->GetBufferPointer(), lVsBlob->GetBufferSize(),
        nullptr, m_pVs.GetAddressOf());
    if (FAILED(lHrVsObj)) return false;

    HRESULT lHrPsObj = pDevice->CreatePixelShader(
        lPsBlob->GetBufferPointer(), lPsBlob->GetBufferSize(),
        nullptr, m_pPs.GetAddressOf());
    if (FAILED(lHrPsObj)) return false;

    // ─── 入力レイアウトの定義（layoutType に応じて切り替える）
    //      バイトオフセットは VERTEX_3D / VERTEX_GIZMO 構造体に対応している

    // 基本：位置(0) + 法線(12) + UV(24) = 32 バイト
    D3D11_INPUT_ELEMENT_DESC lLayoutModel[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // 接線付き：位置(0) + 法線(12) + UV(24) + 接線(32) = 44 バイト
    D3D11_INPUT_ELEMENT_DESC lLayoutModelTangent[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // アウトライン：位置(0) + 法線(12) + UV(24) + 接線(32) + スムーズ法線(44) = 56 バイト
    D3D11_INPUT_ELEMENT_DESC lLayoutOutline[] =
    {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "SMOOTHNORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // ギズモ：位置(0) + カラー(12) = 24 バイト（VERTEX_GIZMO に対応）
    D3D11_INPUT_ELEMENT_DESC lLayoutGizmo[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // スキン：位置・法線・UV・接線・スムーズ法線・ボーン番号・重み
    D3D11_INPUT_ELEMENT_DESC lLayoutSkinned[] =
    {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "SMOOTHNORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // ─── layoutType に対応するレイアウト配列とカウントを選択
    const D3D11_INPUT_ELEMENT_DESC* lLayout = nullptr;
    UINT lLayoutCount = 0;

    if (layoutType == INPUT_LAYOUT_TYPE::GIZMO)
    {
        lLayout = lLayoutGizmo;
        lLayoutCount = 2;
    }
    else if (layoutType == INPUT_LAYOUT_TYPE::MODEL_3D_TANGENT)
    {
        lLayout = lLayoutModelTangent;
        lLayoutCount = 4;
    }
    else if (layoutType == INPUT_LAYOUT_TYPE::OUTLINE)
    {
        lLayout = lLayoutOutline;
        lLayoutCount = 5;
    }
    else if (layoutType == INPUT_LAYOUT_TYPE::SKINNED)
    {
        lLayout = lLayoutSkinned;
        lLayoutCount = _countof(lLayoutSkinned);
    }
    else
    {
        lLayout = lLayoutModel;
        lLayoutCount = 3;
    }

    // ─── 共通の生成
    HRESULT lHrIl = pDevice->CreateInputLayout(
        lLayout, lLayoutCount,
        lVsBlob->GetBufferPointer(), lVsBlob->GetBufferSize(),
        m_pInputLayout.GetAddressOf());
    if (FAILED(lHrIl)) return false;

    // ─── テクスチャサンプラーの生成（線形フィルタリング・ラップアドレス）
    D3D11_SAMPLER_DESC lSampDesc = {};
    lSampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    lSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    lSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    lSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    lSampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    lSampDesc.MinLOD = 0;
    lSampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT lHrSamp = pDevice->CreateSamplerState(&lSampDesc, m_pSampler.GetAddressOf());
    if (FAILED(lHrSamp)) return false;

    // ─── ラスタライザーステートの生成（cullMode に応じてカリング方向を変える）
    D3D11_RASTERIZER_DESC lRasterDesc = {};
    lRasterDesc.FillMode = D3D11_FILL_SOLID;
    lRasterDesc.DepthClipEnable = TRUE;

    switch (cullMode)
    {
    case CULL_MODE::FRONT:
        lRasterDesc.CullMode = D3D11_CULL_FRONT;
        break;
    case CULL_MODE::NONE:
        lRasterDesc.CullMode = D3D11_CULL_NONE;
        break;
    case CULL_MODE::BACK:
    default:
        lRasterDesc.CullMode = D3D11_CULL_BACK;
        break;
    }

    HRESULT lHrRaster = pDevice->CreateRasterizerState(
        &lRasterDesc, m_pRasterizerState.GetAddressOf());
    if (FAILED(lHrRaster)) return false;

    return true;
}

// ------------------------------------------------------------
// Bind
// このシェーダーを GPU パイプラインにバインドする
// 引数：pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
void SHADER::Bind(ID3D11DeviceContext* pContext)
{
    // ─── VS・PS・入力レイアウトをパイプラインにセット
    pContext->VSSetShader(m_pVs.Get(), nullptr, 0);
    pContext->PSSetShader(m_pPs.Get(), nullptr, 0);
    pContext->IASetInputLayout(m_pInputLayout.Get());

    // ─── PS にテクスチャサンプラーをセット（スロット 0）
    ID3D11SamplerState* lSampler = m_pSampler.Get();
    pContext->PSSetSamplers(0, 1, &lSampler);

    // ─── アウトライン用のときは VS にもサンプラーをセット（VTF 用スロット 2）
    if (RequiresExtraCb(EXTRA_CB_OUTLINE))
    {
        pContext->VSSetSamplers(2, 1, &lSampler);
    }

    // ─── ラスタライザーステートをセット
    pContext->RSSetState(m_pRasterizerState.Get());
}