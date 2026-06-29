// ============================================================
// AnimationClip.h
// モーションFBXのキーフレーム資産を保持するクラスの宣言
// 時刻 t でノードのローカル変換を補間して返す
// 制作者：Chan WingChung
// ============================================================
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <DirectXMath.h>

#include "Math/Matrix4x4.h"

class ANIMATION_CLIP
{
private:
    // ─── キーフレーム型定義
    struct VECTOR_KEY
    {
        float             timeTicks;
        DirectX::XMFLOAT3 value;
    };
    struct QUAT_KEY
    {
        float             timeTicks;
        DirectX::XMFLOAT4 value;
    };
    struct CHANNEL
    {
        std::vector<VECTOR_KEY> positionKeys;
        std::vector<QUAT_KEY>   rotationKeys;
        std::vector<VECTOR_KEY> scaleKeys;
    };

    // ─── クリップデータ
    std::unordered_map<std::string, CHANNEL> m_channels;
    float m_durationTicks = 0.0f;
    float m_ticksPerSecond = 30.0f;
    bool  m_bLoaded = false;

    // ─── キー補間ヘルパー（時刻 t での値を返す）
    static DirectX::XMVECTOR InterpVector(const std::vector<VECTOR_KEY>& keys, float timeTicks, float fallback);
    static DirectX::XMVECTOR InterpQuat(const std::vector<QUAT_KEY>& keys, float timeTicks);

public:
    // ─── ロード
    bool  Load(const std::string& filePath);

    // ─── ゲッター
    bool  IsLoaded()          const { return m_bLoaded; }
    float GetDurationTicks()  const { return m_durationTicks; }
    float GetTicksPerSecond() const { return m_ticksPerSecond; }

    // ─── サンプリング
    bool  SampleNode(const std::string& nodeName, float timeTicks, MATRIX4X4& out) const;
};