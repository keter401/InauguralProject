// ============================================================
// Animator.cpp
// 骨格の保持とアニメーション再生の実装
// 制作者：Chan WingChung
// ============================================================
#include "Animator.h"
#include "AssimpConvert.h"
#include "Imgui/imgui.h"
#include <Windows.h>
#include <cstdio>
#include <cmath>

// ------------------------------------------------------------
// AddNode : 1ノードを登録して子を再帰処理する
// 親は必ず自分より前に積まれる（親→子の順序を保証）
// 引数：pNode       - 現在のノード
//        parentIndex - 親の index（根は -1）
// ------------------------------------------------------------
void ANIMATOR::AddNode(const aiNode* pNode, int parentIndex)
{
    int lMyIndex = static_cast<int>(m_skeleton.size());

    // ─── このノードを骨格配列に積み、名前→indexの辞書に登録する
    SKELETON_NODE lNode;
    lNode.name = pNode->mName.C_Str();
    lNode.localTransform = ConvertAiMatrix(pNode->mTransformation);
    lNode.parentIndex = parentIndex;
    m_skeleton.push_back(lNode);
    m_nodeNameToIndex[lNode.name] = lMyIndex;

    // ─── 子ノードを再帰的に登録する
    for (unsigned int lI = 0; lI < pNode->mNumChildren; lI++)
        AddNode(pNode->mChildren[lI], lMyIndex);
}

// ------------------------------------------------------------
// GetDurationSeconds : アニメーションの総尺を秒で返す
// 引数：なし（tps が 0 のときは 0 を返す）
// ------------------------------------------------------------
float ANIMATOR::GetDurationSeconds() const
{
    float lTps = m_clip.GetTicksPerSecond();
    return (lTps > 0.0f) ? m_clip.GetDurationTicks() / lTps : 0.0f;
}

// ------------------------------------------------------------
// BuildSkeleton : aiNode 階層を親→子順でフラット保持する
// 引数：pRootNode - シーンのルートノード
// ------------------------------------------------------------
void ANIMATOR::BuildSkeleton(const aiNode* pRootNode)
{
    // ─── 骨格と辞書をクリアしてから再帰構築する
    m_skeleton.clear();
    m_nodeNameToIndex.clear();
    AddNode(pRootNode, -1);
}

// ------------------------------------------------------------
// RegisterBone : ボーンを採番する（同名は既存番号を返す）
// 引数：name   - ボーン名
//        offset - メッシュローカル→ボーンローカルの逆バインドポーズ行列
// ------------------------------------------------------------
int ANIMATOR::RegisterBone(const std::string& name, const MATRIX4X4& offset)
{
    // ─── 既に採番済みなら同じ番号を返す
    auto lIt = m_boneNameToIndex.find(name);
    if (lIt != m_boneNameToIndex.end())
        return lIt->second;

    // ─── 新規ボーンを登録して番号を割り当てる
    int lIndex = static_cast<int>(m_bones.size());
    BONE_INFO lInfo;
    lInfo.name = name;
    lInfo.offsetMatrix = offset;
    m_bones.push_back(lInfo);
    m_boneNameToIndex[name] = lIndex;
    return lIndex;
}

// ------------------------------------------------------------
// LinkBones : 各ボーンを名前で骨格ノードへひも付ける
// 引数：なし（各ボーンの nodeIndex を確定させる）
// ------------------------------------------------------------
void ANIMATOR::LinkBones()
{
    // ─── ボーン名と一致する骨格ノードの index を引き当てる
    for (auto& lBone : m_bones)
    {
        auto lIt = m_nodeNameToIndex.find(lBone.name);
        lBone.nodeIndex = (lIt != m_nodeNameToIndex.end()) ? lIt->second : -1;
    }
}

// ------------------------------------------------------------
// LoadClip : モーションFBXを読み込む
// 引数：motionFilePath - モーション FBX のパス
// ------------------------------------------------------------
bool ANIMATOR::LoadClip(const std::string& motionFilePath)
{
    return m_clip.Load(motionFilePath);
}

// ------------------------------------------------------------
// Update : 再生時刻を進めて尺でループ／クランプする
// 引数：deltaSeconds - 前フレームからの経過秒
// ------------------------------------------------------------
void ANIMATOR::Update(float deltaSeconds)
{
    if (!m_bPlaying || !HasAnimation())
        return;

    // ─── 時刻を進める（再生速度を掛ける）
    m_timeSeconds += deltaSeconds * m_speed;

    // ─── 尺でループ（またはクランプ）する
    float lTps = m_clip.GetTicksPerSecond();
    float lDurTicks = m_clip.GetDurationTicks();
    if (lTps > 0.0f && lDurTicks > 0.0f)
    {
        float lDurSeconds = lDurTicks / lTps;
        if (m_bLoop)
            m_timeSeconds = fmodf(m_timeSeconds, lDurSeconds);
        else if (m_timeSeconds > lDurSeconds)
            m_timeSeconds = lDurSeconds;
    }
}

// ------------------------------------------------------------
// ComputePalette : 現在時刻のポーズからボーン行列パレットを生成する
// 引数：outPalette - 出力（ボーン数ぶんの行列）
// ------------------------------------------------------------
void ANIMATOR::ComputePalette(std::vector<MATRIX4X4>& outPalette)
{
    // ─── アニメが無ければ単位行列で埋める（バインドポーズ）
    if (!HasAnimation())
    {
        outPalette.assign(m_bones.size(), MATRIX4X4::Identity());
        return;
    }

    // ─── 現在時刻を ticks に変換して尺で折り返す
    float lTps = m_clip.GetTicksPerSecond();
    float lDurTicks = m_clip.GetDurationTicks();
    float lTimeTicks = m_timeSeconds * lTps;
    if (lDurTicks > 0.0f)
        lTimeTicks = fmodf(lTimeTicks, lDurTicks);

    // ─── 各ノードのグローバル変換を親→子順に1パスで計算する
    //     行ベクトル規約：global = local * parentGlobal（local が左）
    std::vector<MATRIX4X4> lGlobals(m_skeleton.size());
    for (size_t lI = 0; lI < m_skeleton.size(); lI++)
    {
        const SKELETON_NODE& lNode = m_skeleton[lI];

        // ─── アニメチャンネルがあれば差し替え、無ければバインドポーズのローカル
        MATRIX4X4 lLocal;
        if (!m_clip.SampleNode(lNode.name, lTimeTicks, lLocal))
            lLocal = lNode.localTransform;

        lGlobals[lI] = (lNode.parentIndex >= 0)
            ? lLocal * lGlobals[lNode.parentIndex]
            : lLocal;
    }

    // ─── 追従メッシュ用に現在のノードグローバルを保持する
    m_nodeGlobals = lGlobals;

    // ─── ルート変換を打ち消す globalInverse（行ベクトル規約：inverse(rootGlobal)）
    DirectX::XMMATRIX lRootInvXm = DirectX::XMMatrixInverse(nullptr, lGlobals[0].ToXMMatrix());
    MATRIX4X4 lGlobalInverse(lRootInvXm);

    // ─── ボーンパレット：palette = offset * nodeGlobal * globalInverse（行ベクトル規約）
    outPalette.resize(m_bones.size());
    for (size_t lB = 0; lB < m_bones.size(); lB++)
    {
        const BONE_INFO& lBone = m_bones[lB];
        if (lBone.nodeIndex < 0)
        {
            outPalette[lB] = MATRIX4X4::Identity();
            continue;
        }
        outPalette[lB] = lBone.offsetMatrix * lGlobals[lBone.nodeIndex] * lGlobalInverse;
    }
}

// ------------------------------------------------------------
// GetNodeGlobal : 指定ノードの現在グローバル変換を返す（追従メッシュ用）
// 引数：nodeName - ノード名
//        out      - 出力行列
// 戻り値：ノードが無い or まだ計算前なら false
// ------------------------------------------------------------
bool ANIMATOR::GetNodeGlobal(const std::string& nodeName, MATRIX4X4& out) const
{
    auto lIt = m_nodeNameToIndex.find(nodeName);
    if (lIt == m_nodeNameToIndex.end())
        return false;
    if (static_cast<size_t>(lIt->second) >= m_nodeGlobals.size())
        return false;

    out = m_nodeGlobals[lIt->second];
    return true;
}

// ------------------------------------------------------------
// DrawImGui : 再生/停止・速度・スクラブの UI を描画する
// 引数：なし
// ------------------------------------------------------------
void ANIMATOR::DrawImGui()
{
    // ─── アニメが無いモデルでは何も出さない
    if (!HasAnimation())
        return;

    if (ImGui::CollapsingHeader("Animation"))
    {
        // ─── 再生/一時停止トグル
        if (ImGui::Button(m_bPlaying ? "Pause" : "Play"))
            m_bPlaying = !m_bPlaying;

        ImGui::SameLine();
        // ─── 先頭に戻す
        if (ImGui::Button("Reset"))
            m_timeSeconds = 0.0f;

        ImGui::SameLine();
        ImGui::Checkbox("Loop", &m_bLoop);

        // ─── 再生速度（0=停止相当?2倍速）
        ImGui::SliderFloat("Speed", &m_speed, 0.0f, 2.0f, "%.2fx");

        // ─── スクラブ：時刻を直接ドラッグして任意フレームへ
        float lDur = GetDurationSeconds();
        if (lDur > 0.0f)
        {
            float lTime = m_timeSeconds;
            if (ImGui::SliderFloat("Time", &lTime, 0.0f, lDur, "%.2f s"))
                m_timeSeconds = lTime;   // スクラブ中はその時刻に固定

            ImGui::Text("Duration: %.2f s", lDur);
        }
    }
}