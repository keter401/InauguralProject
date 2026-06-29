// ============================================================
// Texture.h
// 2D テクスチャのロードと GPU バインドを管理するクラスの宣言
// WIC（PNG/JPG 等）と TGA の両フォーマットに対応する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

class TEXTURE
{
private:
    // ─── GPU リソース
    ComPtr<ID3D11ShaderResourceView> m_srv;

    // ─── DirectX リソース
    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    // ─── メタデータ
    std::string m_filePath;
    bool        m_bLoaded = false;

    // ─── ロードヘルパー
    bool LoadWIC(const std::wstring& wPath, bool bSrgb);
    bool LoadTGA(const std::string& path, bool bSrgb);
    bool CreateSRV(const BYTE* pixels, UINT width, UINT height, bool bSrgb);

public:
    // ─── ライフサイクル
    bool Initialize(
        ID3D11Device* pDevice,
        ID3D11DeviceContext* pContext,
        const std::string& filePath,
        bool                 bSrgb = true);
    void Finalize();

    // ─── GPU バインド
    void Bind(UINT slot = 0) const;
    void BindVS(UINT slot)   const;

    // ─── ゲッター
    bool               IsLoaded()    const { return m_bLoaded; }
    const std::string& GetFilePath() const { return m_filePath; }
};