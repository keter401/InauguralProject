// ============================================================
// App.cpp
// アプリケーション全体を統括するクラスの実装
// ウィンドウ・DirectX 初期化、メインループの更新と描画を行う
// 制作者：Chan WingChung
// ============================================================
#include "App.h"
#include "ImGuiManager.h"
#include "Imgui/imgui.h"
#include <d3dcompiler.h>
#include <cassert>

// ─── D3D 生成失敗時に false を返すヘルパーマクロ
#define HR_RETURN_FALSE(expr) do { if (FAILED(expr)) return false; } while (0)

// ------------------------------------------------------------
// Initialize
// アプリケーション全体を初期化する（ウィンドウ・DX・マネージャー）
// 引数：hInstance  - アプリケーションインスタンスハンドル
//        nCmdShow   - ウィンドウ表示状態
// ------------------------------------------------------------
bool APP::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (!InitWindow(hInstance, nCmdShow)) return false;
    if (!InitDirectX())                   return false;
    if (!InitManagers())                  return false;

    // ─── 高分解能タイマーを初期化する
    QueryPerformanceFrequency(&m_perfFreq);
    QueryPerformanceCounter(&m_prevCounter);

    return true;
}

// ------------------------------------------------------------
// InitWindow
// Win32 ウィンドウクラスを登録しウィンドウを生成する
// 引数：hInstance  - アプリケーションインスタンスハンドル
//        nCmdShow   - ウィンドウ表示状態
// ------------------------------------------------------------
bool APP::InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    // ─── ウィンドウクラスの登録
    WNDCLASSEX lWc = {};
    lWc.cbSize = sizeof(WNDCLASSEX);
    lWc.style = CS_HREDRAW | CS_VREDRAW;
    lWc.lpfnWndProc = WindowProc;
    lWc.hInstance = hInstance;
    lWc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    lWc.lpszClassName = L"ShaderProjectClass";
    RegisterClassEx(&lWc);

    // ─── クライアント領域が指定サイズになるようにウィンドウ矩形を調整
    RECT lRect = { 0, 0, m_width, m_height };
    AdjustWindowRect(&lRect, WS_OVERLAPPEDWINDOW, FALSE);

    // ─── ウィンドウ生成（APP インスタンスを lpCreateParams に渡す）
    m_hWnd = CreateWindowEx(
        0, L"ShaderProjectClass", L"ShaderProject - DirectX11",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        lRect.right - lRect.left, lRect.bottom - lRect.top,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hWnd) return false;
    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);
    return true;
}

// ------------------------------------------------------------
// InitDirectX
// D3D11 デバイス・スワップチェーン・RTV・DSV を生成し描画環境を整える
// 引数：なし
// ------------------------------------------------------------
bool APP::InitDirectX()
{
    // ─── スワップチェーンの設定と D3D11 デバイス生成
    DXGI_SWAP_CHAIN_DESC lScDesc = {};
    lScDesc.BufferCount = 2;
    lScDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    lScDesc.BufferDesc.Width = m_width;
    lScDesc.BufferDesc.Height = m_height;
    lScDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    lScDesc.BufferDesc.RefreshRate = { 60, 1 };
    lScDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    lScDesc.OutputWindow = m_hWnd;
    lScDesc.SampleDesc = { 1, 0 };
    lScDesc.Windowed = TRUE;

    D3D_FEATURE_LEVEL lFeatureLevel;
    HRESULT lHr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &lScDesc,
        &m_swapChain, &m_device, &lFeatureLevel, &m_context
    );
    if (FAILED(lHr)) return false;

    // ─── バックバッファから RTV（レンダーターゲットビュー）を生成
    ComPtr<ID3D11Texture2D> lBackBuffer;
    HR_RETURN_FALSE(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&lBackBuffer)));
    HR_RETURN_FALSE(m_device->CreateRenderTargetView(lBackBuffer.Get(), nullptr, &m_renderTargetView));

    // ─── 深度ステンシルテクスチャと DSV を生成
    D3D11_TEXTURE2D_DESC lDepthDesc = {};
    lDepthDesc.Width = m_width;
    lDepthDesc.Height = m_height;
    lDepthDesc.MipLevels = 1;
    lDepthDesc.ArraySize = 1;
    lDepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    lDepthDesc.SampleDesc = { 1, 0 };
    lDepthDesc.Usage = D3D11_USAGE_DEFAULT;
    lDepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> lDepthTex;
    HR_RETURN_FALSE(m_device->CreateTexture2D(&lDepthDesc, nullptr, &lDepthTex));
    HR_RETURN_FALSE(m_device->CreateDepthStencilView(lDepthTex.Get(), nullptr, &m_depthStencilView));

    // ─── ラスタライザーステートの設定（背面カリング有効）
    D3D11_RASTERIZER_DESC lRasterDesc = {};
    lRasterDesc.FillMode = D3D11_FILL_SOLID;
    lRasterDesc.CullMode = D3D11_CULL_BACK;

    ComPtr<ID3D11RasterizerState> lRasterState;
    HR_RETURN_FALSE(m_device->CreateRasterizerState(&lRasterDesc, &lRasterState));
    m_context->RSSetState(lRasterState.Get());

    // ─── 出力マネージャーに RTV と DSV をセット
    ID3D11RenderTargetView* lRtvs[] = { m_renderTargetView.Get() };
    m_context->OMSetRenderTargets(1, lRtvs, m_depthStencilView.Get());

    // ─── ビューポートを画面全体に設定
    D3D11_VIEWPORT lVp = {};
    lVp.Width = static_cast<float>(m_width);
    lVp.Height = static_cast<float>(m_height);
    lVp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &lVp);

    return true;
}

// ------------------------------------------------------------
// InitManagers
// 各マネージャーを初期化し IBL ベイクとシェーダー割り当てを行う
// 引数：なし
// ------------------------------------------------------------
bool APP::InitManagers()
{
    // ─── 各マネージャーの初期化（失敗時は即 false を返す）
    if (!m_shaderManager.Initialize(m_device.Get(), m_context.Get()))         return false;
    if (!m_shadowManager.Initialize(m_device.Get(), m_context.Get()))         return false;
    if (!m_gameObjectManager.Initialize(m_device.Get(), m_context.Get()))     return false;
    if (!m_imguiManager.Initialize(m_hWnd, m_device.Get(), m_context.Get()))  return false;

    // ─── HDR 環境マップをベイクして IBL SRV を生成
    if (!m_iblBaker.Initialize(m_device.Get(), m_context.Get()))              return false;
    if (!m_iblBaker.BakeAll(CONFIG::IBL_HDR_PATH))                            return false;

    // ─── ゲームオブジェクトマネージャーへ IBL SRV を渡す
    m_gameObjectManager.SetIblSrvs(
        m_iblBaker.GetDiffuseSrv(),
        m_iblBaker.GetSpecularSrv(),
        m_iblBaker.GetBrdfLutSrv()
    );
    m_gameObjectManager.SetHdrSrv(m_iblBaker.GetHdrSrv());

    // ─── シェーダーをゲームオブジェクトに割り当てる
    m_gameObjectManager.AssignShaders(m_shaderManager);

    // ─── シェーダーマネージャーにモデルリストを渡す（ImGui 用）
    m_shaderManager.SetModelList(m_gameObjectManager.GetModels());

    return true;
}

// ------------------------------------------------------------
// Update
// 毎フレーム呼ばれるロジック更新（ゲームオブジェクト全体を更新）
// 引数：なし
// ------------------------------------------------------------
void APP::Update()
{
    // ─── 前フレームからの経過秒を計測する
    LARGE_INTEGER lNow;
    QueryPerformanceCounter(&lNow);
    m_deltaSeconds = static_cast<float>(lNow.QuadPart - m_prevCounter.QuadPart)
        / static_cast<float>(m_perfFreq.QuadPart);
    m_prevCounter = lNow;

    // ─── 異常値（ブレーク後の巨大 dt など）をクランプする
    if (m_deltaSeconds > 0.1f) m_deltaSeconds = 0.1f;

    m_gameObjectManager.Update(m_deltaSeconds);
}

// ------------------------------------------------------------
// Draw
// 毎フレームの描画処理（シャドウ→ミラー→メイン→ImGui の順で実行）
// 引数：なし
// ------------------------------------------------------------
void APP::Draw()
{
    // ─── シャドウマップ生成（コンテキスト構築はマネージャーに委譲）
    SHADOW_PASS_CONTEXT lShadowContext = m_gameObjectManager.BuildShadowContext(
        m_shaderManager.GetByName("Shadow"),
        m_shaderManager.GetByName("ShadowSkinned"),
        m_renderTargetView.Get(),
        m_depthStencilView.Get(),
        static_cast<UINT>(m_width),
        static_cast<UINT>(m_height));

    m_shadowManager.Execute(lShadowContext);

    // ─── 反射パス：MIRROR は自前でライトを bind しないため、ここで影＋ライトを bind してから実行
    m_shadowManager.BindForLighting();
    m_gameObjectManager.GetLightManager().BindAll(1);

    m_gameObjectManager.ExecuteMirrorPass(
        m_renderTargetView.Get(),
        m_depthStencilView.Get(),
        static_cast<UINT>(m_width),
        static_cast<UINT>(m_height));

    // ─── メインバッファをクリア
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), m_clearColor);
    m_context->ClearDepthStencilView(
        m_depthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // ─── メイン描画：影 SRV を再バインド（ライトは GOM::Draw 内で bind される）
    m_shadowManager.BindForLighting();
    m_gameObjectManager.Draw();

    m_gameObjectManager.DrawImGui();
    m_shaderManager.DrawImGui();

    m_imguiManager.EndFrame();

    // ─── フレームを画面に表示（垂直同期あり）
    m_swapChain->Present(1, 0);
}

// ------------------------------------------------------------
// Finalize
// 全マネージャーを解放し COM を終了する
// 引数：なし
// ------------------------------------------------------------
void APP::Finalize()
{
    // ─── 各マネージャーを生成逆順で解放する
    m_iblBaker.Finalize();
    m_gameObjectManager.Finalize();
    m_shaderManager.Finalize();
    m_shadowManager.Finalize();
    m_imguiManager.Finalize();

    // ─── GPU コマンドをフラッシュしてから COM を終了
    if (m_context)
        m_context->ClearState();

    CoUninitialize();
}

// ------------------------------------------------------------
// WindowProc
// Win32 ウィンドウメッセージを処理する静的コールバック関数
// 引数：hWnd    - メッセージ対象のウィンドウハンドル
//        msg     - メッセージ識別子
//        wParam  - メッセージ追加パラメータ
//        lParam  - メッセージ追加パラメータ
// ------------------------------------------------------------
LRESULT CALLBACK APP::WindowProc(HWND hWnd, UINT msg,
    WPARAM wParam, LPARAM lParam)
{
    // ─── ImGui にメッセージを先に渡す（UI がマウス・キーを吸収する場合がある）
    if (IMGUI_MANAGER::HandleWindowMessage(hWnd, msg, wParam, lParam))
        return true;

    auto* lApp = reinterpret_cast<APP*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_CREATE:
    {
        // ─── ウィンドウ生成時に APP インスタンスポインタを USERDATA に保存
        auto* lCs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(lCs->lpCreateParams));
        return 0;
    }
    case WM_DESTROY:
        // ─── ウィンドウ破棄時にループを終了させる
        if (lApp) lApp->m_bActive = false;
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        // ─── ESC キーでアプリを終了する
        if (wParam == VK_ESCAPE && lApp)
            lApp->m_bActive = false;
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}