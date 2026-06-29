// ============================================================
// LightManager.h
// シーン内の複数ライトを管理するクラスの宣言
// ライトの追加・削除・GPU バインド・ImGui 操作を提供する
// 制作者：Chan WingChung
// ============================================================

#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>
#include <memory>
#include "GameObject/Light.h"
#include "Math/ConstantBuffers.h"
#include "GameObject/Camera.h"

using Microsoft::WRL::ComPtr;

class LIGHT_MANAGER
{
private:
    // ─── DirectX リソース
    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    // ─── 依存オブジェクト
    CAMERA* m_pCamera = nullptr;
    SHADER* m_pGizmoShader = nullptr;

    // ─── ライトリスト
    std::vector<std::unique_ptr<LIGHT>> m_lights;
    ComPtr<ID3D11Buffer>                m_pCbLights;

    // ─── ImGui 選択・削除状態
    int m_selectedIdx = 0;
    int m_pendingRemoveIdx = -1;

    // ─── 削除コールバック
    void* m_pCallbackTarget = nullptr;
    void(*m_pCallbackFunc)(void*, LIGHT*) = nullptr;

    // ─── 内部ヘルパー
    void FlushPendingRemove();
    void NotifyLightRemoved(LIGHT* pNewPrimary);

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    void Update();
    void Draw();
    void Finalize();
    void DrawImGui();

    // ─── ライト追加・取得
    LIGHT* AddLight();
    LIGHT* GetLight(size_t index = 0)    const;
    size_t GetLightCount()               const { return m_lights.size(); }
    const std::vector<std::unique_ptr<LIGHT>>& GetLights() const { return m_lights; }

    // ─── セッター（依存オブジェクト）
    void SetCamera(CAMERA* pCamera);
    void SetGizmoShader(SHADER* pShader);

    // ─── コールバック登録
    void SetOnLightRemovedTarget(void* pTarget, void(*pCallback)(void*, LIGHT*))
    {
        m_pCallbackTarget = pTarget;
        m_pCallbackFunc = pCallback;
    }

    // ─── GPU バインド
    void BindAll(UINT slot = 1);
};