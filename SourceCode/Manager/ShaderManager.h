// ============================================================
// ShaderManager.h
// プロジェクトで使用する全シェーダーを管理するクラスの宣言
// シェーダーの登録・検索と ImGui によるモデルへの割り当てを提供する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <string>
#include "Shader/Shader.h"

class MODEL;

using Microsoft::WRL::ComPtr;

class SHADER_MANAGER
{
private:
    // ─── DirectX リソース
    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    // ─── シェーダーリスト
    std::vector<std::unique_ptr<SHADER>> m_shaders;

    // ─── ImGui 選択状態
    std::vector<MODEL*> m_models;
    int                 m_selectedShaderIdx = 0;
    int                 m_selectedModelIdx = 0;

    // ─── 内部ヘルパー
    bool RegisterShaders();

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    void DrawImGui();
    void Finalize();

    // ─── セッター
    void SetModelList(std::vector<MODEL*> models);

    // ─── ゲッター
    SHADER* GetByName(const std::string& name) const;
};