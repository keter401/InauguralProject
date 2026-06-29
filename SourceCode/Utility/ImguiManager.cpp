// ============================================================
// ImguiManager.cpp
// Dear ImGui の初期化・フレーム管理・Win32 メッセージ処理のラッパー実装
// 制作者：Chan WingChung
// ============================================================

#include "ImGuiManager.h"
#include "Imgui/imgui.h"
#include "Imgui/imgui_impl_win32.h"
#include "Imgui/imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ------------------------------------------------------------
// Initialize
// ImGui コンテキストと Win32・DX11 バックエンドを初期化する
// 引数：hWnd     - ImGui を描画するウィンドウハンドル
//        pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool IMGUI_MANAGER::Initialize(
    HWND                 hWnd,
    ID3D11Device* pDevice,
    ID3D11DeviceContext* pContext)
{
    // ─── ImGui コンテキストの生成とスタイルの設定
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // ─── Win32 と DX11 バックエンドの初期化
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(pDevice, pContext);

    return true;
}

// ------------------------------------------------------------
// BeginFrame
// 毎フレームの ImGui 描画を開始する（Draw() の冒頭で呼ぶ）
// 引数：なし
// ------------------------------------------------------------
void IMGUI_MANAGER::BeginFrame()
{
    // ─── バックエンドの新フレーム開始処理と ImGui フレームの開始
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

// ------------------------------------------------------------
// EndFrame
// 毎フレームの ImGui 描画を完了し GPU に送る（Draw() の末尾で呼ぶ）
// 引数：なし
// ------------------------------------------------------------
void IMGUI_MANAGER::EndFrame()
{
    // ─── ImGui の描画コマンドを確定させてバックエンドに渡す
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// ------------------------------------------------------------
// Finalize
// ImGui バックエンドとコンテキストを解放する
// 引数：なし
// ------------------------------------------------------------
void IMGUI_MANAGER::Finalize()
{
    // ─── DX11・Win32 バックエンドのシャットダウンとコンテキスト破棄
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

// ------------------------------------------------------------
// HandleWindowMessage
// Win32 メッセージを ImGui に転送する（WindowProc の先頭で呼ぶ）
// 引数：hWnd    - メッセージ対象ウィンドウ
//        msg     - メッセージ識別子
//        wParam  - メッセージ追加パラメータ
//        lParam  - メッセージ追加パラメータ
// ------------------------------------------------------------
LRESULT IMGUI_MANAGER::HandleWindowMessage(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // ─── ImGui の Win32 ウィンドウプロシージャハンドラに転送する
    return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}