// ============================================================
// GameObjectManager.h
// シーン上の全ゲームオブジェクトを統括管理するクラスの宣言
// カメラ・モデル・フィールド・ミラー・キューブ・スカイボックスを所有する
// 制作者：Chan WingChung
// ============================================================

#pragma once
#include <Windows.h>
#include <vector>
#include <memory>
#include <type_traits> 
#include "GameObject/GameObject.h"
#include "GameObject/Camera.h"
#include "GameObject/Model.h"
#include "GameObject/Field.h"
#include "GameObject/Mirror.h"
#include "GameObject/Cube.h"
#include "GameObject/SkyBox.h"
#include "Manager/ShaderManager.h"
#include "Manager/LightManager.h"
#include "Manager/ShadowManager.h"
#include "Manager/Config.h"
#include "ImGuiManager.h"

class GAME_OBJECT_MANAGER
{
private:
    // ─── DirectX リソース
    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    // ─── ゲームオブジェクト（ポインタキャッシュ）
    CAMERA*  m_pCamera = nullptr;
    FIELD*   m_pField  = nullptr;
    MIRROR*  m_pMirror = nullptr;
    CUBE*    m_pCube   = nullptr;
    SKY_BOX* m_pSkyBox = nullptr;

    // ─── ライトマネージャー
    LIGHT_MANAGER m_lightManager;

    // ─── 全オブジェクトリスト（所有権）
    std::vector<std::unique_ptr<GAME_OBJECT>> m_gameObjects;

    // ─── MODEL 型付きキャッシュ
    std::vector<MODEL*> m_models;

    // ─── 内部ヘルパー
    template <typename T>
    T* Add(std::unique_ptr<T> pObj)
    {
        T* lRawPtr = pObj.get();

        // ─── MODEL 派生なら型付きキャッシュにも登録する
        if constexpr (std::is_base_of_v<MODEL, T>)
            m_models.push_back(lRawPtr);

        m_gameObjects.push_back(std::move(pObj));
        return lRawPtr;
    }

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    void Update(float deltaSeconds = 0);
    void Draw();
    void Finalize();
    void DrawImGui();

    // ─── シェーダー割り当て
    void AssignShaders(SHADER_MANAGER& shaderManager);

    // ─── シャドウパス用コンテキストを構築する（クロスオブジェクト集約）
    SHADOW_PASS_CONTEXT BuildShadowContext(
        SHADER* pShadowShader,
        SHADER* pShadowSkinnedShader,
        ID3D11RenderTargetView* pMainRtv,
        ID3D11DepthStencilView* pMainDsv,
        UINT mainWidth, UINT mainHeight) const;

    // ─── ミラー反射パス
    void ExecuteMirrorPass(
        ID3D11RenderTargetView* pMainRtv,
        ID3D11DepthStencilView* pMainDsv,
        UINT mainWidth, UINT mainHeight);

    // ─── ライト再配布（ライト削除時）
    void RedistributeLight(LIGHT* pLight);

    // ─── セッター（外部リソース注入）
    void SetIblSrvs(
        ID3D11ShaderResourceView* pDiffuse,
        ID3D11ShaderResourceView* pSpecular,
        ID3D11ShaderResourceView* pBrdfLut);
    void SetHdrSrv(ID3D11ShaderResourceView* pSrv);

    // ─── ゲッター
    CAMERA*             GetCamera()        const { return m_pCamera; }
    FIELD*              GetField()         const { return m_pField; }
    MIRROR*             GetMirror()        const { return m_pMirror; }
    CUBE*               GetCube()          const { return m_pCube; }
    SKY_BOX*            GetSkyBox()        const { return m_pSkyBox; }
    LIGHT_MANAGER&      GetLightManager()        { return m_lightManager; }
    const std::vector<MODEL*>& GetModels() const { return m_models; }
};