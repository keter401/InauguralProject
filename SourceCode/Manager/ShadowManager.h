// ============================================================
// ShadowManager.h
// キューブ配列シャドウマップを管理するクラスの宣言
// 全ライト分のシャドウを 1 つの TextureCubeArray にベイクして提供する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

#include "GameObject/Light.h"
#include "GameObject/Model.h"
#include "GameObject/Field.h"
#include "GameObject/Cube.h"
#include "GameObject/Mirror.h"
#include "Shader/Shader.h"

using Microsoft::WRL::ComPtr;

struct SHADOW_PASS_CONTEXT
{
    std::vector<LIGHT*>     lights;
    SHADER* pShadowShader = nullptr;
    SHADER* pShadowSkinnedShader = nullptr;

    std::vector<MODEL*>     models;
    FIELD*  pField  = nullptr;
    CUBE*   pCube   = nullptr;
    MIRROR* pMirror = nullptr;

    ID3D11RenderTargetView* pMainRtv = nullptr;
    ID3D11DepthStencilView* pMainDsv = nullptr;
    UINT                    mainWidth = 0;
    UINT                    mainHeight = 0;
};

class SHADOW_MANAGER
{
private:
    // ─── DirectX リソース
    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    // ─── キューブ配列シャドウマップ
    ComPtr<ID3D11Texture2D>          m_pCubeArrayTexture;
    ComPtr<ID3D11ShaderResourceView> m_pCubeArraySrv;
    std::vector<ComPtr<ID3D11RenderTargetView>> m_cubeRtvs;

    // ─── 共有デプスバッファ
    ComPtr<ID3D11Texture2D>        m_pDepthTexture;
    ComPtr<ID3D11DepthStencilView> m_pDepthDsv;

    // ─── 定数バッファ・サンプラー
    ComPtr<ID3D11Buffer>       m_pShadowCb;
    ComPtr<ID3D11Buffer>       m_pShadowGenCb;
    ComPtr<ID3D11SamplerState> m_pSampler;

    // ─── 内部ヘルパー
    bool CreateResources();
    void RenderLightCube(const SHADOW_PASS_CONTEXT& context, int lightIndex);
    void RestoreMainTarget(const SHADOW_PASS_CONTEXT& context);

public:
    // ─── 定数
    static const UINT SHADOW_MAP_SIZE = 1024;

    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    void Finalize() {}

    // ─── シャドウパス実行
    void Execute(const SHADOW_PASS_CONTEXT& context);

    // ─── GPU バインド（ライティングパス用）
    void BindForLighting(UINT cubeArraySlot = 2, UINT cbSlot = 2, UINT samplerSlot = 1);
};