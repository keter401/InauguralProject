// ============================================================
// ImguiManager.h
// Dear ImGui の初期化・フレーム管理・Win32 メッセージ処理のラッパー宣言
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <Windows.h>
#include <d3d11.h>

class IMGUI_MANAGER
{
public:
    // ─── ライフサイクル
    bool Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    void BeginFrame();
    void EndFrame();
    void Finalize();

    // ─── ウィンドウメッセージ処理
    static LRESULT HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};