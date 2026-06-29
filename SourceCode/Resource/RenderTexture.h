// ============================================================
// RenderTexture.h
// オフスクリーンレンダリング用テクスチャクラスの宣言
// カラーバッファ・デプスバッファを持ち、反射や影マップ等に使用する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class RENDER_TEXTURE
{
private:
    // ─── カラーバッファ
    ComPtr<ID3D11Texture2D>          m_pColorTexture;
    ComPtr<ID3D11RenderTargetView>   m_pRtv;
    ComPtr<ID3D11ShaderResourceView> m_pSrv;

    // ─── デプスバッファ
    ComPtr<ID3D11Texture2D>        m_pDepthTexture;
    ComPtr<ID3D11DepthStencilView> m_pDsv;

    // ─── サイズ
    UINT m_width = 0;
    UINT m_height = 0;

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, UINT width, UINT height);

    // ─── レンダリング開始
    void BeginRender(ID3D11DeviceContext* pContext, const float clearColor[4] = nullptr);

    // ─── ゲッター
    ID3D11ShaderResourceView* GetSrv()    const { return m_pSrv.Get(); }
    ID3D11RenderTargetView*   GetRtv()    const { return m_pRtv.Get(); }
    UINT                      GetWidth()  const { return m_width; }
    UINT                      GetHeight() const { return m_height; }
};