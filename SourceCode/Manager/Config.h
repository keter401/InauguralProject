// ============================================================
// SceneConfig.h
// シーン初期配置の設定データ（位置・スケール・パス等）を一元管理する
// ロジック（GameObjectManager）からデータを分離するための定義層
// 制作者：Chan WingChung
// ============================================================

#pragma once
#include <string>
#include "Math/Vector3.h"

// ─── シーン初期配置パラメータ（配置の変更はここだけを編集する）
struct CONFIG
{
    // ─── パス（アセット）
    static inline const std::string MODEL_PATH =
        "Assets/Model/SD_unitychan_generic.fbx";
    static inline const std::string ANIMATION_PATH =
        "Assets/Animation/SD_unitychan_motion_Generic.fbx";
    static inline const std::string MODEL_TEXTURE_DIR =
        "Assets/Texture/ChibiUnityChan/";
    static inline const std::string IBL_HDR_PATH =
        "Assets/Texture/ferndale_studio_11_2k.hdr";

    // ─── フィールド（床）
    static inline const VECTOR3 FIELD_SCALE = VECTOR3(10.0f, 1.0f, 10.0f);

    // ─── ミラー
    static inline const VECTOR3 MIRROR_POSITION = VECTOR3(2.0f, 1.0f, 2.0f);
    static inline const VECTOR3 MIRROR_ROTATION_AXIS = VECTOR3(0.0f, 1.0f, 0.0f);
    static inline const VECTOR3 MIRROR_SCALE = VECTOR3(3.0f, 3.0f, 1.0f);
    static inline const float   MIRROR_ROTATION_ANGLE_DEG = 45.0f;

    // ─── PBR キューブ
    static inline const VECTOR3 CUBE_POSITION = VECTOR3(-2.5f, 1.5f, 0.0f);
    static inline const VECTOR3 CUBE_SCALE = VECTOR3(0.5f, 0.5f, 0.5f);
};