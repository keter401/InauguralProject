// ============================================================
// Animator.h
// 骨格の保持とアニメーション再生を担うクラスの宣言
// 骨格構築・ボーン採番・パレット生成・再生制御をまとめる
// 制作者：Chan WingChung
// ============================================================
#pragma once

#include <assimp/scene.h>
#include "AnimationClip.h"

class ANIMATOR
{
private:
    // ─── 内部型定義
    struct SKELETON_NODE
    {
        std::string name;
        MATRIX4X4   localTransform;
        int         parentIndex = -1;
    };
    struct BONE_INFO
    {
        std::string name;
        MATRIX4X4   offsetMatrix;
        int         nodeIndex = -1;
    };

    // ─── 骨格・ボーンデータ
    std::vector<SKELETON_NODE>           m_skeleton;
    std::unordered_map<std::string, int> m_nodeNameToIndex;
    std::vector<BONE_INFO>               m_bones;
    std::unordered_map<std::string, int> m_boneNameToIndex;
    std::vector<MATRIX4X4>               m_nodeGlobals;

    // ─── 再生状態
    ANIMATION_CLIP m_clip;
    float m_timeSeconds = 0.0f;
    float m_speed = 1.0f;
    bool  m_bPlaying = true;
    bool  m_bLoop = true;

    // ─── 内部ヘルパー
    void  AddNode(const aiNode* pNode, int parentIndex);
    float GetDurationSeconds() const;

public:
    // ─── ロード時（MODEL から呼ぶ）
    void BuildSkeleton(const aiNode* pRootNode);
    int  RegisterBone(const std::string& name, const MATRIX4X4& offset);
    void LinkBones();
    bool LoadClip(const std::string& motionFilePath);

    // ─── 再生
    void Update(float deltaSeconds);
    void ComputePalette(std::vector<MATRIX4X4>& outPalette);
    bool HasAnimation() const { return m_clip.IsLoaded() && !m_bones.empty(); }

    // ─── ノード参照（追従メッシュ用）
    bool GetNodeGlobal(const std::string& nodeName, MATRIX4X4& out) const;

    // ─── ImGui
    void DrawImGui();
};