// ============================================================
// Field.cpp
// 床面ポリゴンクラスの実装
// Y=0 の水平クワッドをメッシュとして設定して POLYGON に委譲する
// 制作者：Chan WingChung
// ============================================================

#include "Field.h"
#include "Imgui/imgui.h"

// ------------------------------------------------------------
// Initialize
// メッシュを構築してから POLYGON の初期化を行う
// 引数：pDevice  - D3D11 デバイスポインタ
//        pContext - D3D11 デバイスコンテキストポインタ
// ------------------------------------------------------------
bool FIELD::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    // ─── 頂点・インデックスデータを設定してから GPU バッファを生成する
    BuildMesh();
    return POLYGON::Initialize(pDevice, pContext);
}

// ------------------------------------------------------------
// DrawImGui
// フィールドの位置とスケールを操作する ImGui ウィンドウを描画する
// 引数：なし
// ------------------------------------------------------------
void FIELD::DrawImGui()
{
    if (ImGui::Begin("Field"))
    {
        // ─── 位置のドラッグ編集
        VECTOR3 lPos = GetPosition();
        if (ImGui::DragFloat3("Position", &lPos.x, 0.1f))
            SetPosition(lPos);

        // ─── スケールのドラッグ編集
        VECTOR3 lScale = m_scale;
        if (ImGui::DragFloat3("Scale", &lScale.x, 0.1f, 0.01f, 1000.0f))
            SetScale(lScale);
    }
    ImGui::End();
}

// ------------------------------------------------------------
// BuildMesh
// Y=0 の水平クワッド（4 頂点・2 三角形）を定義して SetMeshData に渡す
// 引数：なし
// ------------------------------------------------------------
void FIELD::BuildMesh()
{
    // ─── 単位サイズの水平平面を 4 頂点で定義する（法線は上向き +Y）
    std::vector<VERTEX_3D> lVertices =
    {
        { -0.5f, 0.0f, -0.5f,    0.0f, 1.0f, 0.0f,    0.0f, 0.0f,    0,0,0,  0,0,0 },
        {  0.5f, 0.0f, -0.5f,    0.0f, 1.0f, 0.0f,    1.0f, 0.0f,    0,0,0,  0,0,0 },
        { -0.5f, 0.0f,  0.5f,    0.0f, 1.0f, 0.0f,    0.0f, 1.0f,    0,0,0,  0,0,0 },
        {  0.5f, 0.0f,  0.5f,    0.0f, 1.0f, 0.0f,    1.0f, 1.0f,    0,0,0,  0,0,0 },
    };

    // ─── 2 三角形分のインデックスを定義する（時計回り）
    std::vector<UINT> lIndices = { 0, 2, 1,  1, 2, 3 };
    SetMeshData(lVertices, lIndices);
}