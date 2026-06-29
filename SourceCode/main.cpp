// ============================================================
// main.cpp
// アプリケーションのエントリーポイント
// メッセージループと APP クラスのライフサイクルを管理する
// 制作者：Chan WingChung
// ============================================================

#include "App/App.h"

// ------------------------------------------------------------
// WinMain
// Win32 アプリケーションのエントリーポイント
// 引数：hInstance  - アプリケーションインスタンスハンドル
//        nCmdShow   - ウィンドウ表示状態
// ------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE,
    LPSTR, int nCmdShow)
{
    // ─── 実行ファイルのある場所をカレントディレクトリにする
    //     （ダブルクリック起動でも Assets/ Shaders/ を解決できるようにする）
    wchar_t lExePath[MAX_PATH];
    GetModuleFileNameW(nullptr, lExePath, MAX_PATH);
    std::wstring lExeDir(lExePath);
    lExeDir = lExeDir.substr(0, lExeDir.find_last_of(L"\\/"));
    SetCurrentDirectoryW(lExeDir.c_str());

    APP lApp;

    // ─── アプリケーションの初期化（失敗時は即終了）
    if (!lApp.Initialize(hInstance, nCmdShow))
        return -1;

    // ─── メインメッセージループ
    MSG lMsg = {};
    while (lApp.IsActive())
    {
        if (PeekMessage(&lMsg, nullptr, 0, 0, PM_REMOVE))
        {
            // ─── Windows メッセージを処理する
            TranslateMessage(&lMsg);
            DispatchMessage(&lMsg);
        }
        else
        {
            // ─── ゲームロジックの更新と描画
            lApp.Update();
            lApp.Draw();
        }
    }

    // ─── リソース解放
    lApp.Finalize();
    return static_cast<int>(lMsg.wParam);
}