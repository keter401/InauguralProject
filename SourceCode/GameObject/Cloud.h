// ============================================================
// Cloud.h
// レイマーチング雲パス（全画面）
// 深度テストで背景に合成し、アルファでブレンドする専用レンダラー
// 制作者：Chan WingChung
// ============================================================
#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

class CAMERA;

// ─── 全画面レイマーチングで雲を描く専用パス
class CLOUD
{
private:
    // ─── 定数バッファ（HLSL の CB_CLOUD とレイアウト一致）
    struct CB_CLOUD_DATA
    {
        DirectX::XMFLOAT4X4 invViewProj;  // 64
        DirectX::XMFLOAT3   camPos;
        float               time;         // 16
        DirectX::XMFLOAT3   sunDir;
        float               coverage;     // 16
        float               cloudBottom;
        float               cloudTop;
        int                 marchSteps;
        int                 lightSteps;   // 16
        float               densityMult;
        float               wind;
        float               _pad0, _pad1; // 16
    };

    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_pVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pPs;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_pCb;
    Microsoft::WRL::ComPtr<ID3D11BlendState>        m_pBlendState;   // アルファ合成
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_pDepthState;   // 深度テスト有・書き込み無
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_pRasterState;  // カリング無

    bool m_bInitialized = false;

    // ─── ImGui で調整するパラメータ
    float m_coverage = 0.35f;
    float m_cloudBottom = 40.0f;
    float m_cloudTop = 90.0f;
    int   m_marchSteps = 32;
    int   m_lightSteps = 4;
    float m_densityMult = 1.4f;
    float m_sunDir[3] = { 0.3f, 0.8f, 0.4f };   // 太陽方向（正規化前）
    float m_wind = 6.0f;                        // 風速（X方向スクロール）

public:
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

    // ─── メイン描画後に呼ぶ：指定 RTV/DSV へ雲を合成する
    void Execute(ID3D11RenderTargetView* pRtv,
        ID3D11DepthStencilView* pDsv,
        CAMERA* pCamera,
        float   timeSeconds,
        UINT    width,
        UINT    height);

    void DrawImGui();
};