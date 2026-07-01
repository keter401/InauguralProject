// ============================================================
// Profiler.cpp
// GPU プロファイラの実装
// 制作者：Chan WingChung
// ============================================================
#include "Profiler.h"
#include "Imgui/imgui.h"

// ------------------------------------------------------------
// Initialize
// デバイス・コンテキストを保持し、全フレーム分のクエリを生成する
// 引数：pDevice  - D3D11 デバイス / pContext - D3D11 コンテキスト
// ------------------------------------------------------------
bool PROFILER::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    if (!pDevice || !pContext) return false;
    m_pDevice = pDevice;
    m_pContext = pContext;

    // ─── disjoint（GPU 周波数・有効判定）用のクエリ記述
    D3D11_QUERY_DESC lDisjointDesc = {};
    lDisjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

    // ─── タイムスタンプ用のクエリ記述
    D3D11_QUERY_DESC lStampDesc = {};
    lStampDesc.Query = D3D11_QUERY_TIMESTAMP;

    for (int lF = 0; lF < FRAME_LATENCY; ++lF)
    {
        if (FAILED(m_pDevice->CreateQuery(&lDisjointDesc, m_frames[lF].disjointQuery.GetAddressOf())))
            return false;

        for (int lP = 0; lP < MAX_PASSES; ++lP)
        {
            if (FAILED(m_pDevice->CreateQuery(&lStampDesc, m_frames[lF].passes[lP].beginQuery.GetAddressOf())))
                return false;
            if (FAILED(m_pDevice->CreateQuery(&lStampDesc, m_frames[lF].passes[lP].endQuery.GetAddressOf())))
                return false;
        }
    }

    m_bInitialized = true;
    return true;
}

// ------------------------------------------------------------
// BeginFrame
// 今から書き込むスロットの「前回分」を回収してから計測を開始する
// ------------------------------------------------------------
void PROFILER::BeginFrame()
{
    if (!m_bInitialized) return;

    m_writeIndex = m_frameCount % FRAME_LATENCY;

    // ─── このスロットは FRAME_LATENCY フレーム前のデータ＝もう完了しているので回収
    if (m_frameCount >= FRAME_LATENCY)
        Resolve(m_frames[m_writeIndex]);

    FRAME_SLOT& lSlot = m_frames[m_writeIndex];
    lSlot.passCount = 0;
    m_activePass = -1;

    // ─── このフレームの GPU 周波数計測を開始
    m_pContext->Begin(lSlot.disjointQuery.Get());
}

// ------------------------------------------------------------
// BeginPass
// パス開始位置にタイムスタンプを打つ（ネストは非対応）
// 引数：passName - 表示名（"Shadow" など）
// ------------------------------------------------------------
void PROFILER::BeginPass(const char* passName)
{
    if (!m_bInitialized || m_activePass >= 0) return;

    FRAME_SLOT& lSlot = m_frames[m_writeIndex];
    if (lSlot.passCount >= MAX_PASSES) return;

    int lIndex = lSlot.passCount;
    lSlot.passes[lIndex].name = passName;
    m_pContext->End(lSlot.passes[lIndex].beginQuery.Get());  // タイムスタンプは End() で刻む
    m_activePass = lIndex;
}

// ------------------------------------------------------------
// EndPass
// 直前に開始したパスの終了位置にタイムスタンプを打つ
// ------------------------------------------------------------
void PROFILER::EndPass()
{
    if (!m_bInitialized || m_activePass < 0) return;

    FRAME_SLOT& lSlot = m_frames[m_writeIndex];
    m_pContext->End(lSlot.passes[m_activePass].endQuery.Get());
    lSlot.passCount = m_activePass + 1;
    m_activePass = -1;
}

// ------------------------------------------------------------
// EndFrame
// disjoint を閉じてフレーム番号を進める
// ------------------------------------------------------------
void PROFILER::EndFrame()
{
    if (!m_bInitialized) return;

    m_pContext->End(m_frames[m_writeIndex].disjointQuery.Get());
    ++m_frameCount;
}

// ------------------------------------------------------------
// Resolve
// 1フレーム分のクエリを読み出し、パス毎の ms に変換する
// 引数：slot - 対象フレームスロット
// ------------------------------------------------------------
void PROFILER::Resolve(FRAME_SLOT& slot)
{
    // ─── disjoint を取得（古いフレームなので即 S_OK が返る）
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT lDisjoint = {};
    while (m_pContext->GetData(slot.disjointQuery.Get(), &lDisjoint, sizeof(lDisjoint), 0) != S_OK) {}

    // ─── クロックが乱れたフレームは無効。前回の結果を残す
    if (lDisjoint.Disjoint || lDisjoint.Frequency == 0)
        return;

    m_results.clear();
    m_gpuTotalMs = 0.0f;

    for (int lP = 0; lP < slot.passCount; ++lP)
    {
        UINT64 lBegin = 0;
        UINT64 lEnd = 0;
        while (m_pContext->GetData(slot.passes[lP].beginQuery.Get(), &lBegin, sizeof(lBegin), 0) != S_OK) {}
        while (m_pContext->GetData(slot.passes[lP].endQuery.Get(), &lEnd, sizeof(lEnd), 0) != S_OK) {}

        // ─── (終了tick − 開始tick) / 周波数 × 1000 = ミリ秒
        float lMs = (lEnd > lBegin)
            ? static_cast<float>(static_cast<double>(lEnd - lBegin) / lDisjoint.Frequency * 1000.0)
            : 0.0f;

        m_results.push_back({ slot.passes[lP].name, lMs });
        m_gpuTotalMs += lMs;
    }
}

// ------------------------------------------------------------
// DrawImGui
// 計測結果をウィンドウに表示する
// ------------------------------------------------------------
void PROFILER::DrawImGui()
{
    if (!m_bInitialized) return;

    if (ImGui::Begin("GPU Profiler"))
    {
        if (m_results.empty())
        {
            ImGui::TextDisabled("Collecting timing data...");
        }
        else
        {
            ImGui::Text("GPU total : %6.3f ms", m_gpuTotalMs);
            ImGui::Separator();

            for (const PASS_RESULT& lR : m_results)
            {
                // ─── パス名・実時間・全体に占める割合バー
                float lFraction = (m_gpuTotalMs > 0.0f) ? (lR.milliseconds / m_gpuTotalMs) : 0.0f;
                ImGui::Text("%-8s %6.3f ms", lR.name.c_str(), lR.milliseconds);
                ImGui::SameLine();
                ImGui::ProgressBar(lFraction, ImVec2(-1.0f, 0.0f), "");
            }
        }
    }
    ImGui::End();
}