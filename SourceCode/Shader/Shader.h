// ============================================================
// Shader.h
// 頂点シェーダー・ピクセルシェーダー・入力レイアウトを管理するクラスの宣言
// HLSL ファイルをコンパイルしてバインド可能な状態に保持する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

class SHADER
{
private:
    // ─── GPU リソース
    ComPtr<ID3D11VertexShader>    m_pVs;
    ComPtr<ID3D11PixelShader>     m_pPs;
    ComPtr<ID3D11InputLayout>     m_pInputLayout;
    ComPtr<ID3D11SamplerState>    m_pSampler;
    ComPtr<ID3D11RasterizerState> m_pRasterizerState;

    // ─── フラグ・識別子
    unsigned int m_extraCb = EXTRA_CB_NONE;
    std::string  m_name;

public:
    // ─── 列挙型定義
    // 入力レイアウト種別・カリングモード・追加 CB フラグを定義する
    enum class INPUT_LAYOUT_TYPE
    {
        MODEL_3D,           // 位置・法線・UV（基本）
        MODEL_3D_TANGENT,   // 位置・法線・UV・接線
        OUTLINE,            // アウトライン専用（スムーズ法線付き）
        GIZMO,              // ギズモ専用（位置・カラー）
        SKINNED,            // 位置・法線・UV・接線・スムーズ法線・ボーン番号・重み
    };

    enum class CULL_MODE
    {
        BACK,   // 背面カリング（通常）
        FRONT,  // 前面カリング（アウトライン押し出し用）
        NONE,   // カリングなし（両面描画）
    };

    enum EXTRA_CB : unsigned int
    {
        EXTRA_CB_NONE = 0,
        EXTRA_CB_OUTLINE = 1 << 0,  // アウトライン用 CB（b3）が必要
        EXTRA_CB_MIRROR = 1 << 1,  // ミラー用 CB（b4）が必要
        EXTRA_CB_DISSOLVE = 1 << 2,  // ディゾルブ用 CB（b5）が必要
        EXTRA_CB_PBR = 1 << 3,  // PBR 用 CB（b6）が必要
    };

    // ─── 初期化
    bool Initialize(
        ID3D11Device* pDevice,
        const wchar_t* vsPath,
        const wchar_t* psPath,
        const std::string& name,
        INPUT_LAYOUT_TYPE  layoutType = INPUT_LAYOUT_TYPE::MODEL_3D,
        CULL_MODE          cullMode = CULL_MODE::BACK,
        unsigned int       extraCb = EXTRA_CB_NONE);

    // ─── GPU バインド
    void Bind(ID3D11DeviceContext* pContext);

    // ─── ゲッター
    const std::string& GetName()                          const { return m_name; }
    bool               RequiresExtraCb(unsigned int flag) const { return (m_extraCb & flag) != 0; }
};