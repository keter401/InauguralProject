// ============================================================
// AnimationClip.cpp
// モーションFBXのキーフレーム資産。時刻 t でノードのローカル変換を補間して返す
// 制作者：Chan WingChung
// ============================================================
#include "AnimationClip.h"

#include <Windows.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/version.h>

using namespace DirectX;

// ------------------------------------------------------------
// Load : モーションFBXを読み込み、ノード名ごとのキーフレームを保持する
// 引数：filePath - モーション FBX のパス
// ------------------------------------------------------------
bool ANIMATION_CLIP::Load(const std::string& filePath)
{
    Assimp::Importer lImporter;

    // ─── モデルと同じ左手系規約に揃える（ねじれ防止のため）
    const aiScene* lScene = lImporter.ReadFile(
        filePath,
        aiProcess_ConvertToLeftHanded);

    if (!lScene || lScene->mNumAnimations == 0)
    {
        OutputDebugStringA("[AnimClip] No animation found\n");
        return false;
    }

    // ─── 最初のアニメーション（"Take 001"）を採用する
    const aiAnimation* lAnim = lScene->mAnimations[0];

    m_durationTicks = static_cast<float>(lAnim->mDuration);
    m_ticksPerSecond = (lAnim->mTicksPerSecond != 0.0)
        ? static_cast<float>(lAnim->mTicksPerSecond)
        : 30.0f;   // tps が 0 のファイル対策のフォールバック

    // ─── 各チャンネル（=ノード）のキーをコピーする
    m_channels.clear();
    for (unsigned int lC = 0; lC < lAnim->mNumChannels; lC++)
    {
        const aiNodeAnim* lNodeAnim = lAnim->mChannels[lC];
        CHANNEL lChannel;

        // ─── 位置キー
        for (unsigned int lK = 0; lK < lNodeAnim->mNumPositionKeys; lK++)
        {
            const aiVectorKey& lKey = lNodeAnim->mPositionKeys[lK];
            lChannel.positionKeys.push_back(
                { static_cast<float>(lKey.mTime), { lKey.mValue.x, lKey.mValue.y, lKey.mValue.z } });
        }

        // ─── 回転キー（aiQuaternion は w,x,y,z 順。XMFLOAT4 は x,y,z,w 順に並べ替える）
        for (unsigned int lK = 0; lK < lNodeAnim->mNumRotationKeys; lK++)
        {
            const aiQuatKey& lKey = lNodeAnim->mRotationKeys[lK];
            lChannel.rotationKeys.push_back(
                { static_cast<float>(lKey.mTime), { lKey.mValue.x, lKey.mValue.y, lKey.mValue.z, lKey.mValue.w } });
        }

        // ─── スケールキー
        for (unsigned int lK = 0; lK < lNodeAnim->mNumScalingKeys; lK++)
        {
            const aiVectorKey& lKey = lNodeAnim->mScalingKeys[lK];
            lChannel.scaleKeys.push_back(
                { static_cast<float>(lKey.mTime), { lKey.mValue.x, lKey.mValue.y, lKey.mValue.z } });
        }

        m_channels[lNodeAnim->mNodeName.C_Str()] = std::move(lChannel);
    }

    m_bLoaded = true;

    return true;
}

// ------------------------------------------------------------
// InterpVector : 位置/スケールキーを時刻 t で線形補間する
// 引数：keys      - キー列
//        timeTicks - 時刻（ticks）
//        fallback  - キーが空のときに返す値（位置=0 / スケール=1）
// ------------------------------------------------------------
XMVECTOR ANIMATION_CLIP::InterpVector(const std::vector<VECTOR_KEY>& keys, float timeTicks, float fallback)
{
    if (keys.empty())     return XMVectorReplicate(fallback);
    if (keys.size() == 1) return XMLoadFloat3(&keys[0].value);

    // ─── timeTicks を超える最初のキーを探す（キー数は少ないので線形探索で十分）
    size_t lNext = 0;
    while (lNext < keys.size() && keys[lNext].timeTicks < timeTicks)
        lNext++;

    if (lNext == 0)           return XMLoadFloat3(&keys.front().value);
    if (lNext >= keys.size()) return XMLoadFloat3(&keys.back().value);

    // ─── 前後キーの間を比率 t で線形補間する
    const VECTOR_KEY& lK0 = keys[lNext - 1];
    const VECTOR_KEY& lK1 = keys[lNext];
    float lSpan = lK1.timeTicks - lK0.timeTicks;
    float lT = (lSpan > 1e-6f) ? (timeTicks - lK0.timeTicks) / lSpan : 0.0f;

    return XMVectorLerp(XMLoadFloat3(&lK0.value), XMLoadFloat3(&lK1.value), lT);
}

// ------------------------------------------------------------
// InterpQuat : 回転キーを時刻 t で球面線形補間（slerp）する
// 引数：keys      - 回転キー列
//        timeTicks - 時刻（ticks）
// ------------------------------------------------------------
XMVECTOR ANIMATION_CLIP::InterpQuat(const std::vector<QUAT_KEY>& keys, float timeTicks)
{
    if (keys.empty())     return XMQuaternionIdentity();
    if (keys.size() == 1) return XMLoadFloat4(&keys[0].value);

    size_t lNext = 0;
    while (lNext < keys.size() && keys[lNext].timeTicks < timeTicks)
        lNext++;

    if (lNext == 0)           return XMLoadFloat4(&keys.front().value);
    if (lNext >= keys.size()) return XMLoadFloat4(&keys.back().value);

    const QUAT_KEY& lK0 = keys[lNext - 1];
    const QUAT_KEY& lK1 = keys[lNext];
    float lSpan = lK1.timeTicks - lK0.timeTicks;
    float lT = (lSpan > 1e-6f) ? (timeTicks - lK0.timeTicks) / lSpan : 0.0f;

    // ─── slerp は内部で正規化・最短経路をとる
    return XMQuaternionSlerp(XMLoadFloat4(&lK0.value), XMLoadFloat4(&lK1.value), lT);
}

// ------------------------------------------------------------
// SampleNode : 指定ノードの時刻 t のローカル変換（S*R*T）を返す
// 引数：nodeName  - ノード名
//        timeTicks - 時刻（ticks）
//        out       - 出力行列
// 戻り値：チャンネルが無ければ false（呼び出し側はバインドポーズを使う）
// ------------------------------------------------------------
bool ANIMATION_CLIP::SampleNode(const std::string& nodeName, float timeTicks, MATRIX4X4& out) const
{
    auto lIt = m_channels.find(nodeName);
    if (lIt == m_channels.end())
        return false;

    const CHANNEL& lCh = lIt->second;

    // ─── 各成分を補間する（空なら位置0 / 回転単位 / スケール1）
    XMVECTOR lScale = InterpVector(lCh.scaleKeys, timeTicks, 1.0f);
    XMVECTOR lQuat = InterpQuat(lCh.rotationKeys, timeTicks);
    XMVECTOR lPos = InterpVector(lCh.positionKeys, timeTicks, 0.0f);

    XMMATRIX lS = XMMatrixScalingFromVector(lScale);
    XMMATRIX lR = XMMatrixRotationQuaternion(lQuat);
    XMMATRIX lT = XMMatrixTranslationFromVector(lPos);

    // ─── S * R * T（行ベクトル規約：CalcWorldMatrix / ConvertAiMatrix と一致）
    out = MATRIX4X4(lS * lR * lT);
    return true;
}