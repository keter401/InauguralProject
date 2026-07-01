// ============================================================
// Profiler.h
// GPU プロファイラ：D3D11 タイムスタンプクエリでパス毎の GPU 時間を計測
// 制作者：Chan WingChung
// ============================================================
#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>

// ─── パス毎の GPU 実行時間を計測して ImGui に表示するプロファイラ
class PROFILER
{
public:
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

    void BeginFrame();                    // フレーム先頭：古い結果を回収し計測を開始
    void BeginPass(const char* passName); // パス開始点にタイムスタンプを打つ
    void EndPass();                       // パス終了点にタイムスタンプを打つ
    void EndFrame();                      // フレーム末尾：disjoint を閉じる
    void DrawImGui();                     // 計測結果をウィンドウ表示

private:
    // ─── GPU 待ちを避けるため、数フレーム遅らせて結果を読み戻す
    static const int FRAME_LATENCY = 3;
    static const int MAX_PASSES = 16;

    // ─── 1パスぶんの開始・終了タイムスタンプクエリ
    struct PASS_QUERY
    {
        std::string                         name;
        Microsoft::WRL::ComPtr<ID3D11Query> beginQuery;
        Microsoft::WRL::ComPtr<ID3D11Query> endQuery;
    };

    // ─── 1フレームぶんのクエリ一式（disjoint ＋ 各パス）
    struct FRAME_SLOT
    {
        Microsoft::WRL::ComPtr<ID3D11Query> disjointQuery;
        PASS_QUERY                          passes[MAX_PASSES];
        int                                 passCount = 0;
    };

    // ─── 表示用の解決済み結果
    struct PASS_RESULT
    {
        std::string name;
        float       milliseconds;
    };

    void Resolve(FRAME_SLOT& slot);       // 1フレーム分のクエリを ms に変換

    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    FRAME_SLOT m_frames[FRAME_LATENCY];
    int  m_frameCount = 0;               // 累計フレーム数
    int  m_writeIndex = 0;               // 今書き込んでいるスロット
    int  m_activePass = -1;              // BeginPass〜EndPass 中のパス番号
    bool m_bInitialized = false;

    std::vector<PASS_RESULT> m_results;    // 最後に解決した結果
    float m_gpuTotalMs = 0.0f;
};