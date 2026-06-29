// ============================================================
// IblBaker.cpp
// Image Based Lighting（IBL）テクスチャをベイクするクラスの実装
// HDR を CPU でサンプリングし Diffuse Irradiance / Specular / BRDF LUT を生成する
// 制作者：Chan WingChung
// ============================================================

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "IblBaker.h"
#include <cmath>
#include <vector>
#include <Windows.h>

static const float IBL_PI = 3.14159265f;

// ------------------------------------------------------------
// Initialize
// デバイスとコンテキストを保持する（ベイク処理は BakeAll で行う）
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool IBL_BAKER::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    m_pDevice = pDevice;
    m_pContext = pContext;
    return true;
}

// ------------------------------------------------------------
// Finalize
// stb_image でロードした HDR データを解放する
// 引数：なし
// ------------------------------------------------------------
void IBL_BAKER::Finalize()
{
    // ─── HDR ピクセルバッファを解放（stbi_image_free を使用）
    if (m_pHdrData)
    {
        stbi_image_free(m_pHdrData);
        m_pHdrData = nullptr;
    }
}

// ------------------------------------------------------------
// Halton
// 準乱数列（Halton 列）の 1 要素を生成する
// 引数：index - サンプルインデックス
//        base  - 基数（2 や 3 を使用）
// ------------------------------------------------------------
float IBL_BAKER::Halton(int index, int base)
{
    // ─── 基数変換により [0, 1) の準乱数を計算する
    float lResult = 0.0f;
    float lFraction = 1.0f;
    int   lI = index;
    while (lI > 0)
    {
        lFraction /= static_cast<float>(base);
        lResult += lFraction * static_cast<float>(lI % base);
        lI /= base;
    }
    return lResult;
}

// ------------------------------------------------------------
// FaceUvToDir
// キューブマップのフェイス UV 座標を 3D 方向ベクトルに変換する
// 引数：face     - キューブフェイスインデックス（0〜5: +X/-X/+Y/-Y/+Z/-Z）
//        u, v     - フェイス上の UV 座標（0〜1）
//        outX/Y/Z - 変換結果の正規化方向ベクトル
// ------------------------------------------------------------
void IBL_BAKER::FaceUvToDir(int face, float u, float v,
    float& outX, float& outY, float& outZ)
{
    // ─── UV を [-1, 1] に変換してフェイスごとに軸を割り当てる
    float lUc = 2.0f * u - 1.0f;
    float lVc = 2.0f * v - 1.0f;

    switch (face)
    {
    case 0: outX = 1.0f; outY = -lVc;  outZ = -lUc; break;  // +X
    case 1: outX = -1.0f; outY = -lVc;  outZ = lUc; break;  // -X
    case 2: outX = lUc;  outY = 1.0f; outZ = lVc; break;  // +Y
    case 3: outX = lUc;  outY = -1.0f; outZ = -lVc; break;  // -Y
    case 4: outX = lUc;  outY = -lVc;  outZ = 1.0f; break; // +Z
    case 5: outX = -lUc;  outY = -lVc;  outZ = -1.0f; break; // -Z
    }

    // ─── 正規化して単位ベクトルにする
    float lLen = sqrtf(outX * outX + outY * outY + outZ * outZ);
    outX /= lLen; outY /= lLen; outZ /= lLen;
}

// ------------------------------------------------------------
// DirToEquirect
// 3D 方向ベクトルを正距円筒投影（Equirectangular）の UV に変換する
// 引数：x, y, z  - 正規化済み方向ベクトル
//        outU/V   - 変換結果の UV 座標（0〜1）
// ------------------------------------------------------------
void IBL_BAKER::DirToEquirect(float x, float y, float z,
    float& outU, float& outV)
{
    // ─── 方位角（θ）と仰角（φ）から UV を計算する
    float lTheta = atan2f(z, x);
    float lPhi = asinf(y);

    outU = (lTheta / (2.0f * IBL_PI)) + 0.5f;
    outV = (lPhi / IBL_PI) + 0.5f;
}

// ------------------------------------------------------------
// SampleHdr
// 指定方向の HDR 環境マップをバイリニア補間でサンプリングする
// 引数：x, y, z       - サンプリング方向（正規化済み）
//        outR/G/B     - サンプリング結果の RGB 値
// ------------------------------------------------------------
void IBL_BAKER::SampleHdr(float x, float y, float z,
    float& outR, float& outG, float& outB)
{
    // ─── 方向を Equirectangular UV に変換しピクセル座標を計算
    float lU, lV;
    DirToEquirect(x, y, z, lU, lV);

    float lFx = lU * static_cast<float>(m_hdrWidth - 1);
    float lFy = lV * static_cast<float>(m_hdrHeight - 1);

    int lX0 = static_cast<int>(lFx);
    int lY0 = static_cast<int>(lFy);
    int lX1 = (lX0 + 1) % m_hdrWidth;
    int lY1 = min(lY0 + 1, m_hdrHeight - 1);

    float lTx = lFx - static_cast<float>(lX0);
    float lTy = lFy - static_cast<float>(lY0);

    // ─── 4 テクセルをバイリニア補間して結果を得る
    auto lSample = [&](int px, int py, float& r, float& g, float& b)
        {
            int lIdx = (py * m_hdrWidth + px) * 3;
            r = m_pHdrData[lIdx + 0];
            g = m_pHdrData[lIdx + 1];
            b = m_pHdrData[lIdx + 2];
        };

    float lR00, lG00, lB00, lR10, lG10, lB10;
    float lR01, lG01, lB01, lR11, lG11, lB11;

    lSample(lX0, lY0, lR00, lG00, lB00);
    lSample(lX1, lY0, lR10, lG10, lB10);
    lSample(lX0, lY1, lR01, lG01, lB01);
    lSample(lX1, lY1, lR11, lG11, lB11);

    outR = (lR00 * (1 - lTx) + lR10 * lTx) * (1 - lTy) + (lR01 * (1 - lTx) + lR11 * lTx) * lTy;
    outG = (lG00 * (1 - lTx) + lG10 * lTx) * (1 - lTy) + (lG01 * (1 - lTx) + lG11 * lTx) * lTy;
    outB = (lB00 * (1 - lTx) + lB10 * lTx) * (1 - lTy) + (lB01 * (1 - lTx) + lB11 * lTx) * lTy;
}

// ------------------------------------------------------------
// ImportanceSampleGgx
// GGX 分布に基づく重点サンプリングでハーフベクトルを生成する
// 引数：xi1/xi2        - 一様乱数（Halton 列など）
//        roughness      - 表面粗さ（0=滑らか、1=粗い）
//        nx/ny/nz       - 法線ベクトル（正規化済み）
//        outHx/Hy/Hz    - 生成されたハーフベクトル（正規化済み）
// ------------------------------------------------------------
void IBL_BAKER::ImportanceSampleGgx(
    float xi1, float xi2, float roughness,
    float nx, float ny, float nz,
    float& outHx, float& outHy, float& outHz)
{
    float lA = roughness * roughness;

    // ─── 球面座標で GGX 重点サンプリング方向を計算
    float lPhi = 2.0f * IBL_PI * xi1;
    float lCosTheta = sqrtf((1.0f - xi2) / (1.0f + (lA * lA - 1.0f) * xi2));
    float lSinTheta = sqrtf(max(0.0f, 1.0f - lCosTheta * lCosTheta));

    float lHx = lSinTheta * cosf(lPhi);
    float lHy = lSinTheta * sinf(lPhi);
    float lHz = lCosTheta;

    // ─── 法線を基底とする接線空間を構築（TBN 行列）
    float lUpX = (fabsf(nz) < 0.999f) ? 0.0f : 1.0f;
    float lUpY = 0.0f;
    float lUpZ = (fabsf(nz) < 0.999f) ? 1.0f : 0.0f;

    float lTx = lUpY * nz - lUpZ * ny;
    float lTy = lUpZ * nx - lUpX * nz;
    float lTz = lUpX * ny - lUpY * nx;
    float lTLen = sqrtf(lTx * lTx + lTy * lTy + lTz * lTz);
    lTx /= lTLen; lTy /= lTLen; lTz /= lTLen;

    float lBx = ny * lTz - nz * lTy;
    float lBy = nz * lTx - nx * lTz;
    float lBz = nx * lTy - ny * lTx;

    // ─── タンジェント空間のサンプルをワールド空間に変換
    outHx = lTx * lHx + lBx * lHy + nx * lHz;
    outHy = lTy * lHx + lBy * lHy + ny * lHz;
    outHz = lTz * lHx + lBz * lHy + nz * lHz;

    float lLen = sqrtf(outHx * outHx + outHy * outHy + outHz * outHz);
    outHx /= lLen; outHy /= lLen; outHz /= lLen;
}

// ------------------------------------------------------------
// LoadHdr
// stb_image で HDR ファイルを読み込み D3D11 テクスチャを生成する
// 引数：hdrPath - HDR ファイルパス
// ------------------------------------------------------------
bool IBL_BAKER::LoadHdr(const std::string& hdrPath)
{
    // ─── stb_image で HDR ファイルを浮動小数点として読み込む
    stbi_set_flip_vertically_on_load(true);
    int lChannels = 0;
    m_pHdrData = stbi_loadf(
        hdrPath.c_str(),
        &m_hdrWidth, &m_hdrHeight,
        &lChannels, 3);

    if (!m_pHdrData)
    {
        OutputDebugStringA(("[IBL] HDR読み込み失敗: " + hdrPath + "\n").c_str());
        return false;
    }
    OutputDebugStringA(("[IBL] HDR読み込み成功: " + hdrPath + "\n").c_str());

    // ─── RGB から RGBA（アルファ=1.0）に変換して D3D11 テクスチャを生成
    std::vector<float> lRgba(m_hdrWidth * m_hdrHeight * 4);
    for (int lI = 0; lI < m_hdrWidth * m_hdrHeight; lI++)
    {
        lRgba[lI * 4 + 0] = m_pHdrData[lI * 3 + 0];
        lRgba[lI * 4 + 1] = m_pHdrData[lI * 3 + 1];
        lRgba[lI * 4 + 2] = m_pHdrData[lI * 3 + 2];
        lRgba[lI * 4 + 3] = 1.0f;
    }

    D3D11_TEXTURE2D_DESC lHdrDesc = {};
    lHdrDesc.Width = m_hdrWidth;
    lHdrDesc.Height = m_hdrHeight;
    lHdrDesc.MipLevels = 1;
    lHdrDesc.ArraySize = 1;
    lHdrDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    lHdrDesc.SampleDesc = { 1, 0 };
    lHdrDesc.Usage = D3D11_USAGE_DEFAULT;
    lHdrDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA lHdrInitData = {};
    lHdrInitData.pSysMem = lRgba.data();
    lHdrInitData.SysMemPitch = m_hdrWidth * 4 * sizeof(float);

    if (FAILED(m_pDevice->CreateTexture2D(&lHdrDesc, &lHdrInitData, &m_pHdrTex)))
    {
        OutputDebugStringA("[IBL] HDRテクスチャ作成失敗\n");
        return false;
    }

    // ─── SRV を生成してシェーダーから参照できるようにする
    D3D11_SHADER_RESOURCE_VIEW_DESC lHdrSrvDesc = {};
    lHdrSrvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    lHdrSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    lHdrSrvDesc.Texture2D.MipLevels = 1;

    if (FAILED(m_pDevice->CreateShaderResourceView(
        m_pHdrTex.Get(), &lHdrSrvDesc, &m_pHdrSrv)))
    {
        OutputDebugStringA("[IBL] HDR SRV作成失敗\n");
        return false;
    }

    OutputDebugStringA("[IBL] HDRテクスチャ作成成功\n");
    return true;
}

// ------------------------------------------------------------
// BakeDiffuse
// コサイン重み付きモンテカルロ積分で Diffuse Irradiance キューブマップを生成する
// 引数：なし
// ------------------------------------------------------------
bool IBL_BAKER::BakeDiffuse()
{
    const int lSize = DIFFUSE_SIZE;

    // ─── 6 フェイス分のピクセルバッファを確保
    std::vector<float> lPixels(lSize * lSize * 4 * 6, 0.0f);

    for (int lFace = 0; lFace < 6; lFace++)
    {
        for (int lY = 0; lY < lSize; lY++)
        {
            for (int lX = 0; lX < lSize; lX++)
            {
                float lU = (static_cast<float>(lX) + 0.5f) / static_cast<float>(lSize);
                float lV = (static_cast<float>(lY) + 0.5f) / static_cast<float>(lSize);

                // ─── フェイス UV からワールド法線を取得
                float lNx, lNy, lNz;
                FaceUvToDir(lFace, lU, lV, lNx, lNy, lNz);

                float lSumR = 0.0f, lSumG = 0.0f, lSumB = 0.0f;
                float lSumW = 0.0f;

                // ─── コサイン重み付き半球サンプリング（DIFFUSE_SAMPLES 回）
                for (int lS = 0; lS < DIFFUSE_SAMPLES; lS++)
                {
                    float lXi1 = Halton(lS, 2);
                    float lXi2 = Halton(lS, 3);

                    float lPhi = 2.0f * IBL_PI * lXi1;
                    float lCosTheta = sqrtf(lXi2);
                    float lSinTheta = sqrtf(1.0f - lXi2);

                    float lLx = lSinTheta * cosf(lPhi);
                    float lLy = lSinTheta * sinf(lPhi);
                    float lLz = lCosTheta;

                    // ─── タンジェント空間からワールド空間へ変換
                    float lUpX = (fabsf(lNz) < 0.999f) ? 0.0f : 1.0f;
                    float lUpY = 0.0f;
                    float lUpZ = (fabsf(lNz) < 0.999f) ? 1.0f : 0.0f;

                    float lTx = lUpY * lNz - lUpZ * lNy;
                    float lTy = lUpZ * lNx - lUpX * lNz;
                    float lTz = lUpX * lNy - lUpY * lNx;
                    float lTLen = sqrtf(lTx * lTx + lTy * lTy + lTz * lTz);
                    lTx /= lTLen; lTy /= lTLen; lTz /= lTLen;

                    float lBx = lNy * lTz - lNz * lTy;
                    float lBy = lNz * lTx - lNx * lTz;
                    float lBz = lNx * lTy - lNy * lTx;

                    float lWx = lTx * lLx + lBx * lLy + lNx * lLz;
                    float lWy = lTy * lLx + lBy * lLy + lNy * lLz;
                    float lWz = lTz * lLx + lBz * lLy + lNz * lLz;

                    // ─── ワールド方向で HDR をサンプリングして積算
                    float lR, lG, lB;
                    SampleHdr(lWx, lWy, lWz, lR, lG, lB);

                    lSumR += lR;
                    lSumG += lG;
                    lSumB += lB;
                    lSumW += 1.0f;
                }

                int lIdx = (lFace * lSize * lSize + lY * lSize + lX) * 4;
                lPixels[lIdx + 0] = lSumR / lSumW;
                lPixels[lIdx + 1] = lSumG / lSumW;
                lPixels[lIdx + 2] = lSumB / lSumW;
                lPixels[lIdx + 3] = 1.0f;
            }
        }
    }

    // ─── キューブマップテクスチャを生成して初期データを渡す
    D3D11_TEXTURE2D_DESC lDesc = {};
    lDesc.Width = lSize;
    lDesc.Height = lSize;
    lDesc.MipLevels = 1;
    lDesc.ArraySize = 6;
    lDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    lDesc.SampleDesc = { 1, 0 };
    lDesc.Usage = D3D11_USAGE_DEFAULT;
    lDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    lDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    D3D11_SUBRESOURCE_DATA lInitData[6] = {};
    for (int lF = 0; lF < 6; lF++)
    {
        lInitData[lF].pSysMem = lPixels.data() + lF * lSize * lSize * 4;
        lInitData[lF].SysMemPitch = lSize * 4 * sizeof(float);
    }

    if (FAILED(m_pDevice->CreateTexture2D(&lDesc, lInitData, &m_pDiffuseTex)))
    {
        OutputDebugStringA("[IBL] Diffuseテクスチャ作成失敗\n");
        return false;
    }

    // ─── SRV を生成する（キューブテクスチャとして）
    D3D11_SHADER_RESOURCE_VIEW_DESC lSrvDesc = {};
    lSrvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    lSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    lSrvDesc.TextureCube.MostDetailedMip = 0;
    lSrvDesc.TextureCube.MipLevels = 1;

    if (FAILED(m_pDevice->CreateShaderResourceView(
        m_pDiffuseTex.Get(), &lSrvDesc, &m_pDiffuseSrv)))
    {
        OutputDebugStringA("[IBL] Diffuse SRV作成失敗\n");
        return false;
    }

    OutputDebugStringA("[IBL] Diffuse Irradianceベイク完了\n");
    return true;
}

// ------------------------------------------------------------
// BakeSpecular
// GGX 重点サンプリングで Specular Prefiltered キューブマップを生成する
// Mip レベルごとにラフネスを変えてフィルタリングする
// 引数：なし
// ------------------------------------------------------------
bool IBL_BAKER::BakeSpecular()
{
    const int lMips = SPECULAR_MIPS;
    const int lBaseSize = SPECULAR_SIZE;

    // ─── ミップマップ付きキューブマップテクスチャを生成
    D3D11_TEXTURE2D_DESC lDesc = {};
    lDesc.Width = lBaseSize;
    lDesc.Height = lBaseSize;
    lDesc.MipLevels = lMips;
    lDesc.ArraySize = 6;
    lDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    lDesc.SampleDesc = { 1, 0 };
    lDesc.Usage = D3D11_USAGE_DEFAULT;
    lDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    lDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    if (FAILED(m_pDevice->CreateTexture2D(&lDesc, nullptr, &m_pSpecularTex)))
    {
        OutputDebugStringA("[IBL] Specularテクスチャ作成失敗\n");
        return false;
    }

    // ─── Mip レベルごとにラフネスを上げながら GGX 重点サンプリングを実行
    for (int lMip = 0; lMip < lMips; lMip++)
    {
        int   lSize = max(1, lBaseSize >> lMip);
        float lRoughness = static_cast<float>(lMip) / static_cast<float>(lMips - 1);

        std::vector<float> lPixels(lSize * lSize * 4 * 6, 0.0f);

        for (int lFace = 0; lFace < 6; lFace++)
        {
            for (int lY = 0; lY < lSize; lY++)
            {
                for (int lX = 0; lX < lSize; lX++)
                {
                    float lU = (static_cast<float>(lX) + 0.5f) / static_cast<float>(lSize);
                    float lV = (static_cast<float>(lY) + 0.5f) / static_cast<float>(lSize);

                    float lNx, lNy, lNz;
                    FaceUvToDir(lFace, lU, lV, lNx, lNy, lNz);

                    float lVx = lNx, lVy = lNy, lVz = lNz;
                    float lSumR = 0.0f, lSumG = 0.0f, lSumB = 0.0f;
                    float lSumW = 0.0f;

                    // ─── SPECULAR_SAMPLES 回の GGX 重点サンプリング
                    for (int lS = 0; lS < SPECULAR_SAMPLES; lS++)
                    {
                        float lXi1 = Halton(lS, 2);
                        float lXi2 = Halton(lS, 3);

                        float lHx, lHy, lHz;
                        ImportanceSampleGgx(
                            lXi1, lXi2, lRoughness,
                            lNx, lNy, lNz,
                            lHx, lHy, lHz);

                        float lVdotH = lVx * lHx + lVy * lHy + lVz * lHz;
                        float lLx = 2.0f * lVdotH * lHx - lVx;
                        float lLy = 2.0f * lVdotH * lHy - lVy;
                        float lLz = 2.0f * lVdotH * lHz - lVz;

                        float lNdotL = lNx * lLx + lNy * lLy + lNz * lLz;
                        if (lNdotL > 0.0f)
                        {
                            float lR, lG, lB;
                            SampleHdr(lLx, lLy, lLz, lR, lG, lB);

                            lSumR += lR * lNdotL;
                            lSumG += lG * lNdotL;
                            lSumB += lB * lNdotL;
                            lSumW += lNdotL;
                        }
                    }

                    float lInvW = (lSumW > 0.0f) ? 1.0f / lSumW : 0.0f;
                    int   lIdx = (lFace * lSize * lSize + lY * lSize + lX) * 4;
                    lPixels[lIdx + 0] = lSumR * lInvW;
                    lPixels[lIdx + 1] = lSumG * lInvW;
                    lPixels[lIdx + 2] = lSumB * lInvW;
                    lPixels[lIdx + 3] = 1.0f;
                }
            }

            // ─── 各フェイスのピクセルデータを GPU テクスチャのサブリソースに転送
            UINT lSubresource = D3D11CalcSubresource(lMip, lFace, lMips);
            m_pContext->UpdateSubresource(
                m_pSpecularTex.Get(),
                lSubresource,
                nullptr,
                lPixels.data() + lFace * lSize * lSize * 4,
                lSize * 4 * sizeof(float),
                0);
        }

        OutputDebugStringA(("[IBL] Specular Mip" + std::to_string(lMip) +
            " (roughness=" + std::to_string(lRoughness) + ") 完了\n").c_str());
    }

    // ─── SRV を生成する（全 Mip を含むキューブテクスチャとして）
    D3D11_SHADER_RESOURCE_VIEW_DESC lSrvDesc = {};
    lSrvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    lSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    lSrvDesc.TextureCube.MostDetailedMip = 0;
    lSrvDesc.TextureCube.MipLevels = lMips;

    if (FAILED(m_pDevice->CreateShaderResourceView(
        m_pSpecularTex.Get(), &lSrvDesc, &m_pSpecularSrv)))
    {
        OutputDebugStringA("[IBL] Specular SRV作成失敗\n");
        return false;
    }

    OutputDebugStringA("[IBL] Prefiltered Specularベイク完了\n");
    return true;
}

// ------------------------------------------------------------
// BakeBrdfLut
// Split-sum BRDF 積分のルックアップテクスチャを生成する
// UV の X 軸が NdotV、Y 軸がラフネスに対応する 2 チャンネルテクスチャ
// 引数：なし
// ------------------------------------------------------------
bool IBL_BAKER::BakeBrdfLut()
{
    const int lSize = LUT_SIZE;

    // ─── RG32F フォーマットで LUT ピクセルを計算する
    std::vector<float> lPixels(lSize * lSize * 2, 0.0f);

    for (int lY = 0; lY < lSize; lY++)
    {
        float lRoughness = (static_cast<float>(lY) + 0.5f) / static_cast<float>(lSize);

        for (int lX = 0; lX < lSize; lX++)
        {
            float lNdotV = (static_cast<float>(lX) + 0.5f) / static_cast<float>(lSize);

            float lVx = sqrtf(max(0.0f, 1.0f - lNdotV * lNdotV));
            float lVy = 0.0f;
            float lVz = lNdotV;

            float lA = 0.0f;
            float lB = 0.0f;

            // ─── 1024 サンプルで BRDF 積分を計算（G * F の分解）
            for (int lS = 0; lS < 1024; lS++)
            {
                float lXi1 = Halton(lS, 2);
                float lXi2 = Halton(lS, 3);

                float lHx, lHy, lHz;
                ImportanceSampleGgx(
                    lXi1, lXi2, lRoughness,
                    0.0f, 0.0f, 1.0f,
                    lHx, lHy, lHz);

                float lVdotH = lVx * lHx + lVy * lHy + lVz * lHz;
                float lLx = 2.0f * lVdotH * lHx - lVx;
                float lLy = 2.0f * lVdotH * lHy - lVy;
                float lLz = 2.0f * lVdotH * lHz - lVz;

                float lNdotL = max(0.0f, lLz);
                float lNdotH = max(0.0f, lHz);
                lVdotH = max(0.0f, lVdotH);

                if (lNdotL > 0.0f)
                {
                    float lAlpha = lRoughness * lRoughness;
                    float lK = lAlpha / 2.0f;

                    auto lG1 = [&](float lNdotX) -> float
                        {
                            return lNdotX / (lNdotX * (1.0f - lK) + lK);
                        };

                    float lG = lG1(lNdotV) * lG1(lNdotL);
                    float lGVis = (lG * lVdotH) / (lNdotH * lNdotV + 0.0001f);

                    // ─── フレネル係数の定数項と非定数項を分離して積算
                    float lFc = pow(1.0f - lVdotH, 5.0f);
                    lA += lGVis * (1.0f - lFc);
                    lB += lGVis * lFc;
                }
            }

            int lIdx = (lY * lSize + lX) * 2;
            lPixels[lIdx + 0] = lA / 1024.0f;
            lPixels[lIdx + 1] = lB / 1024.0f;
        }
    }

    // ─── RG32F テクスチャを生成してデータを転送
    D3D11_TEXTURE2D_DESC lDesc = {};
    lDesc.Width = lSize;
    lDesc.Height = lSize;
    lDesc.MipLevels = 1;
    lDesc.ArraySize = 1;
    lDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    lDesc.SampleDesc = { 1, 0 };
    lDesc.Usage = D3D11_USAGE_DEFAULT;
    lDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA lInitData = {};
    lInitData.pSysMem = lPixels.data();
    lInitData.SysMemPitch = lSize * 2 * sizeof(float);

    if (FAILED(m_pDevice->CreateTexture2D(&lDesc, &lInitData, &m_pBrdfLutTex)))
    {
        OutputDebugStringA("[IBL] BRDF LUTテクスチャ作成失敗\n");
        return false;
    }

    // ─── SRV を生成する
    D3D11_SHADER_RESOURCE_VIEW_DESC lSrvDesc = {};
    lSrvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    lSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    lSrvDesc.Texture2D.MipLevels = 1;

    if (FAILED(m_pDevice->CreateShaderResourceView(
        m_pBrdfLutTex.Get(), &lSrvDesc, &m_pBrdfLutSrv)))
    {
        OutputDebugStringA("[IBL] BRDF LUT SRV作成失敗\n");
        return false;
    }

    OutputDebugStringA("[IBL] BRDF LUTベイク完了\n");
    return true;
}

// ------------------------------------------------------------
// BakeAll
// HDR 読み込みから全テクスチャのベイクまでを順番に実行する
// 引数：hdrPath - HDR 環境マップファイルのパス
// ------------------------------------------------------------
bool IBL_BAKER::BakeAll(const std::string& hdrPath)
{
    OutputDebugStringA("[IBL] ベイク開始...\n");

    // ─── HDR → Diffuse → Specular → BRDF LUT の順でベイク
    if (!LoadHdr(hdrPath))   return false;
    if (!BakeDiffuse())      return false;
    if (!BakeSpecular())     return false;
    if (!BakeBrdfLut())      return false;

    // ─── CPU 側の HDR データを解放（GPU 転送済みのため不要）
    stbi_image_free(m_pHdrData);
    m_pHdrData = nullptr;

    m_bReady = true;
    OutputDebugStringA("[IBL] 全ベイク完了\n");
    return true;
}