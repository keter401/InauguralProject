// ============================================================
// IblBaker.h
// Image Based Lighting（IBL）テクスチャをベイクするクラスの宣言
// HDR 環境マップから拡散・鏡面・BRDF LUT テクスチャを CPU で生成する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

class IBL_BAKER
{
private:
    // ─── ベイク解像度・サンプル数の定数
    static const int DIFFUSE_SIZE = 16;
    static const int SPECULAR_SIZE = 64;
    static const int SPECULAR_MIPS = 5;
    static const int LUT_SIZE = 256;
    static const int DIFFUSE_SAMPLES = 1024;
    static const int SPECULAR_SAMPLES = 512;

    // ─── DirectX リソース
    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;

    // ─── HDR 元データ
    float* m_pHdrData = nullptr;
    int    m_hdrWidth = 0;
    int    m_hdrHeight = 0;

    // ─── ベイク済みテクスチャ
    ComPtr<ID3D11Texture2D> m_pDiffuseTex;
    ComPtr<ID3D11Texture2D> m_pSpecularTex;
    ComPtr<ID3D11Texture2D> m_pBrdfLutTex;
    ComPtr<ID3D11Texture2D> m_pHdrTex;

    // ─── SRV（シェーダーリソースビュー）
    ComPtr<ID3D11ShaderResourceView> m_pDiffuseSrv;
    ComPtr<ID3D11ShaderResourceView> m_pSpecularSrv;
    ComPtr<ID3D11ShaderResourceView> m_pBrdfLutSrv;
    ComPtr<ID3D11ShaderResourceView> m_pHdrSrv;

    // ─── 状態フラグ
    bool m_bReady = false;

    // ─── ベイク内部ヘルパー
    bool LoadHdr(const std::string& hdrPath);
    bool BakeDiffuse();
    bool BakeSpecular();
    bool BakeBrdfLut();

    // ─── 座標変換ユーティリティ
    void FaceUvToDir(int face, float u, float v, float& outX, float& outY, float& outZ);
    void DirToEquirect(float x, float y, float z, float& outU, float& outV);
    void SampleHdr(float x, float y, float z, float& outR, float& outG, float& outB);

    // ─── GGX 重点サンプリング・準乱数生成
    void ImportanceSampleGgx(
        float xi1, float xi2, float roughness,
        float nx, float ny, float nz,
        float& outHx, float& outHy, float& outHz);

    float Halton(int index, int base);

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
    void Finalize();

    // ─── ベイク実行
    bool BakeAll(const std::string& hdrPath);

    // ─── ゲッター
    ID3D11ShaderResourceView* GetDiffuseSrv()  const { return m_pDiffuseSrv.Get(); }
    ID3D11ShaderResourceView* GetSpecularSrv() const { return m_pSpecularSrv.Get(); }
    ID3D11ShaderResourceView* GetBrdfLutSrv()  const { return m_pBrdfLutSrv.Get(); }
    ID3D11ShaderResourceView* GetHdrSrv()      const { return m_pHdrSrv.Get(); }
    bool                      IsReady()         const { return m_bReady; }
};