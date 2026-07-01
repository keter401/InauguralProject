// ============================================================
// App.h
// アプリケーション全体を統括するクラスの宣言
// ウィンドウ・DirectX・マネージャー群を所有し協調させる
// 制作者：Chan WingChung
// ============================================================

#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "Manager/GameObjectManager.h"
#include "Manager/ShaderManager.h"
#include "Manager/ShadowManager.h"
#include "GameObject/Cloud.h"
#include "ImGuiManager.h"
#include "Profiler.h"
#include "IblBaker.h"
#include "BufferViewer.h" 

using Microsoft::WRL::ComPtr;

class APP
{
private:
    // ─── ウィンドウ
    HWND m_hWnd = nullptr;
    int  m_width = 1280;
    int  m_height = 720;
    bool m_bActive = true;

    // ─── 背景色
    float m_clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    // ─── DirectX リソース
    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_context;
    ComPtr<IDXGISwapChain>         m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;

    // ─── マネージャー群
    GAME_OBJECT_MANAGER m_gameObjectManager;
    SHADER_MANAGER      m_shaderManager;
    SHADOW_MANAGER      m_shadowManager;
    IMGUI_MANAGER       m_imguiManager;
    PROFILER            m_profiler;
    IBL_BAKER           m_iblBaker;
    BUFFER_VIEWER       m_bufferViewer;
        
    // ─── 初期化ヘルパー
    bool InitWindow(HINSTANCE hInstance, int nCmdShow);
    bool InitDirectX();
    bool InitManagers();

    // ─── ウィンドウプロシージャ
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // ─── フレーム経過時間計測
    LARGE_INTEGER m_perfFreq = {};
    LARGE_INTEGER m_prevCounter = {};
    float         m_deltaSeconds = 0.0f;

	// ─── 雲
    CLOUD m_cloud;
    float m_timeSeconds = 0.0f;

public:
    // ─── ライフサイクル
    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    void Update();
    void Draw();
    void Finalize();

    // ─── ゲッター
    bool IsActive() const { return m_bActive; }
};