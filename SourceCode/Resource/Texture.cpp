// ============================================================
// Texture.cpp
// 2D テクスチャのロードと GPU バインドを管理するクラスの実装
// 拡張子により WIC または TGA ローダーを自動選択する
// 制作者：Chan WingChung
// ============================================================

#include "Texture.h"

#include <wincodec.h>
#include <fstream>
#include <vector>
#include <algorithm>

#pragma comment(lib, "windowscodecs.lib")

// ------------------------------------------------------------
// Initialize
// ファイルパスからテクスチャを読み込み GPU リソースを生成する
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
//        filePath - テクスチャファイルのパス
//        bSrgb    - sRGB フォーマットで読み込む場合は true（デフォルト true）
// ------------------------------------------------------------
bool TEXTURE::Initialize(
    ID3D11Device* pDevice,
    ID3D11DeviceContext* pContext,
    const std::string& filePath,
    bool                 bSrgb)
{
    m_pDevice = pDevice;
    m_pContext = pContext;
    m_filePath = filePath;

    // ─── 拡張子を小文字に変換してフォーマットを判定する
    std::string lExt = filePath.substr(filePath.rfind('.') + 1);
    std::transform(lExt.begin(), lExt.end(), lExt.begin(), ::tolower);

    bool lResult = false;

    // ─── TGA は独自ローダー、それ以外は WIC で読み込む
    if (lExt == "tga")
    {
        lResult = LoadTGA(filePath, bSrgb);
    }
    else
    {
        std::wstring lWPath(filePath.begin(), filePath.end());
        lResult = LoadWIC(lWPath, bSrgb);
    }

    m_bLoaded = lResult;
    return lResult;
}

// ------------------------------------------------------------
// Finalize
// GPU リソース（SRV）を解放する
// 引数：なし
// ------------------------------------------------------------
void TEXTURE::Finalize()
{
    // ─── SRV を解放してロード状態をリセットする
    m_srv.Reset();
    m_bLoaded = false;
}

// ------------------------------------------------------------
// Bind
// SRV をピクセルシェーダーの指定スロットにバインドする
// 引数：slot - PS テクスチャスロット番号（デフォルト 0）
// ------------------------------------------------------------
void TEXTURE::Bind(UINT slot) const
{
    if (!m_bLoaded) return;
    // ─── PS にテクスチャをバインドする
    ID3D11ShaderResourceView* lSrv = m_srv.Get();
    m_pContext->PSSetShaderResources(slot, 1, &lSrv);
}

// ------------------------------------------------------------
// BindVS
// SRV を頂点シェーダーの指定スロットにバインドする
// 引数：slot - VS テクスチャスロット番号
// ------------------------------------------------------------
void TEXTURE::BindVS(UINT slot) const
{
    if (!m_bLoaded) return;
    // ─── VS にテクスチャをバインドする（アウトライン VTF 等で使用）
    ID3D11ShaderResourceView* lSrv = m_srv.Get();
    m_pContext->VSSetShaderResources(slot, 1, &lSrv);
}

// ------------------------------------------------------------
// LoadWIC
// Windows Imaging Component を使って画像を RGBA ピクセルに展開する
// 引数：wPath - ワイド文字列のファイルパス
//        bSrgb - sRGB フォーマットを使う場合は true
// ------------------------------------------------------------
bool TEXTURE::LoadWIC(const std::wstring& wPath, bool bSrgb)
{
    // ─── WIC ファクトリーを生成して画像のデコーダーを取得する
    ComPtr<IWICImagingFactory> lFactory;
    HRESULT lHr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&lFactory));
    if (FAILED(lHr)) return false;

    ComPtr<IWICBitmapDecoder> lDecoder;
    lHr = lFactory->CreateDecoderFromFilename(
        wPath.c_str(), nullptr,
        GENERIC_READ, WICDecodeMetadataCacheOnLoad,
        &lDecoder);
    if (FAILED(lHr)) return false;

    ComPtr<IWICBitmapFrameDecode> lFrame;
    if (FAILED(lDecoder->GetFrame(0, &lFrame))) return false;

    // ─── 任意フォーマットを 32bpp RGBA に変換するコンバーターを使用する
    ComPtr<IWICFormatConverter> lConverter;
    if (FAILED(lFactory->CreateFormatConverter(&lConverter))) return false;
    if (FAILED(lConverter->Initialize(
        lFrame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeMedianCut))) return false;

    // ─── ピクセルデータをシステムメモリにコピーする
    UINT lWidth = 0, lHeight = 0;
    lConverter->GetSize(&lWidth, &lHeight);

    std::vector<BYTE> lPixels(lWidth * lHeight * 4);
    if (FAILED(lConverter->CopyPixels(
        nullptr, lWidth * 4,
        static_cast<UINT>(lPixels.size()),
        lPixels.data()))) return false;

    return CreateSRV(lPixels.data(), lWidth, lHeight, bSrgb);
}

// ------------------------------------------------------------
// LoadTGA
// TGA ファイルを手動でパースして RGBA ピクセルに展開する
// 引数：path  - TGA ファイルのパス
//        bSrgb - sRGB フォーマットを使う場合は true
// ------------------------------------------------------------
bool TEXTURE::LoadTGA(const std::string& path, bool bSrgb)
{
    // ─── バイナリでファイルを開いてヘッダーを読み取る
    std::ifstream lFile(path, std::ios::binary);
    if (!lFile) return false;

    BYTE lHeader[18] = {};
    lFile.read(reinterpret_cast<char*>(lHeader), 18);

    const BYTE  lIdLength = lHeader[0];
    const BYTE  lImageType = lHeader[2];
    const UINT  lWidth = lHeader[12] | (lHeader[13] << 8);
    const UINT  lHeight = lHeader[14] | (lHeader[15] << 8);
    const BYTE  lBpp = lHeader[16];
    const BYTE  lDescriptor = lHeader[17];

    // ─── 非圧縮 RGB / グレースケールかつ 24/32bpp のみ対応する
    if ((lImageType != 2 && lImageType != 3) ||
        (lBpp != 24 && lBpp != 32))
    {
        OutputDebugStringA("[Texture] 非対応のTGAフォーマットです\n");
        return false;
    }

    if (lIdLength > 0)
        lFile.seekg(lIdLength, std::ios::cur);

    // ─── ピクセルデータを読み込み BGR → RGBA に変換する
    const UINT  lBytesPP = lBpp / 8;
    const UINT  lPixelNum = lWidth * lHeight;
    std::vector<BYTE> lRawPixels(lPixelNum * lBytesPP);
    lFile.read(reinterpret_cast<char*>(lRawPixels.data()),
        static_cast<std::streamsize>(lRawPixels.size()));

    std::vector<BYTE> lPixels(lPixelNum * 4);
    for (UINT lIdx = 0; lIdx < lPixelNum; lIdx++)
    {
        const BYTE* lSrc = &lRawPixels[lIdx * lBytesPP];
        BYTE* lDst = &lPixels[lIdx * 4];

        lDst[0] = lSrc[2];
        lDst[1] = lSrc[1];
        lDst[2] = lSrc[0];
        lDst[3] = (lBytesPP == 4) ? lSrc[3] : 0xFF;
    }

    // ─── Y 反転フラグが立っていない場合は上下を反転する
    const bool lBFlipY = ((lDescriptor & 0x20) == 0);
    if (lBFlipY)
    {
        for (UINT lRow = 0; lRow < lHeight / 2; lRow++)
        {
            BYTE* lTop = &lPixels[lRow * lWidth * 4];
            BYTE* lBot = &lPixels[(lHeight - 1 - lRow) * lWidth * 4];
            for (UINT lCol = 0; lCol < lWidth * 4; lCol++)
                std::swap(lTop[lCol], lBot[lCol]);
        }
    }

    return CreateSRV(lPixels.data(), lWidth, lHeight, bSrgb);
}

// ------------------------------------------------------------
// CreateSRV
// ピクセルデータから D3D11 テクスチャと SRV を生成する
// 引数：pixels - RGBA ピクセルデータポインタ
//        width  - テクスチャの横幅
//        height - テクスチャの縦幅
//        bSrgb  - sRGB フォーマットを使う場合は true
// ------------------------------------------------------------
bool TEXTURE::CreateSRV(
    const BYTE* pixels,
    UINT        width,
    UINT        height,
    bool        bSrgb)
{
    // ─── sRGB フラグに応じてフォーマットを選択する
    const DXGI_FORMAT lFormat = bSrgb
        ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
        : DXGI_FORMAT_R8G8B8A8_UNORM;

    // ─── イミュータブルテクスチャとして D3D11 テクスチャを生成する
    D3D11_TEXTURE2D_DESC lTexDesc = {};
    lTexDesc.Width = width;
    lTexDesc.Height = height;
    lTexDesc.MipLevels = 1;
    lTexDesc.ArraySize = 1;
    lTexDesc.Format = lFormat;
    lTexDesc.SampleDesc.Count = 1;
    lTexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    lTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA lInitData = {};
    lInitData.pSysMem = pixels;
    lInitData.SysMemPitch = width * 4;

    ComPtr<ID3D11Texture2D> lTex;
    if (FAILED(m_pDevice->CreateTexture2D(&lTexDesc, &lInitData, &lTex)))
    {
        OutputDebugStringA("[Texture] Texture2D の作成に失敗\n");
        return false;
    }

    // ─── テクスチャから SRV を生成してシェーダーから参照できるようにする
    D3D11_SHADER_RESOURCE_VIEW_DESC lSrvDesc = {};
    lSrvDesc.Format = lFormat;
    lSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    lSrvDesc.Texture2D.MipLevels = 1;
    lSrvDesc.Texture2D.MostDetailedMip = 0;

    if (FAILED(m_pDevice->CreateShaderResourceView(lTex.Get(), &lSrvDesc, &m_srv)))
    {
        OutputDebugStringA("[Texture] SRV の作成に失敗\n");
        return false;
    }

    return true;
}