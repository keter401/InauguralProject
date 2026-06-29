// ============================================================
// Field.h
// 床面ポリゴンクラスの宣言
// POLYGON を継承し Y=0 の水平クワッドをフィールドとして描画する
// 制作者：Chan WingChung
// ============================================================

#pragma once
#include "Polygon.h"

class FIELD : public POLYGON
{
private:
    // ─── 内部ヘルパー
    void BuildMesh();

public:
    // ─── ライフサイクル
    bool Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) override;
    void Update(float deltaSeconds = 0) override {}
    void DrawImGui() override;
};