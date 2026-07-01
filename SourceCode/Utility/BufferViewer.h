// ============================================================
// BufferViewer.h
// 中間バッファビューア：各パスの SRV を全画面表示するデバッグビュー
// 制作者：Chan WingChung
// ============================================================
#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>

// ─── 表示対象の種類（サンプルの仕方が違う）
enum VIEW_TYPE
{
    VIEW_2D,          // Texture2D
    VIEW_CUBE,        // TextureCube
    VIEW_CUBE_ARRAY   // TextureCubeArray（スライス選択あり）
};

// ─── 各パスの中間テクスチャ（SRV）を全画面表示するデバッグビュー
class BUFFER_VIEWER
{
public:
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

    // ─── 表示候補を登録（cubeCount はキューブ配列のときの本数）
    void AddSource(const char* name, ID3D11ShaderResourceView* pSrv,
        VIEW_TYPE type, int cubeCount = 1);

    void DrawImGui();
    void Execute(ID3D11RenderTargetView* pRtv, UINT width, UINT height);

private:
    // ─── キューブ配列で「何番目のキューブを見るか」を渡す CB
    struct CB_VIEWER_DATA
    {
        int   cubeIndex;
        float pad0, pad1, pad2;
    };

    struct SOURCE
    {
        std::string               name;
        ID3D11ShaderResourceView* pSrv = nullptr;
        VIEW_TYPE                 type = VIEW_2D;
        int                       cubeCount = 1;
    };

    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_pVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pPs2D;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pPsCube;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pPsCubeArray;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_pCb;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_pSampler;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_pDepthOff;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_pRaster;

    std::vector<SOURCE> m_sources;
    int  m_selected = 0;      // 0 = Final
    int  m_cubeIndex = 0;     // キューブ配列の表示スライス
    bool m_bInitialized = false;
};