// ============================================================
// Vector3.h
// 3次元ベクトルクラスの定義
// 算術演算・正規化・内積・外積などの基本操作を提供する
// 制作者：Chan WingChung
// ============================================================

#pragma once

#include <math.h>

class VECTOR3
{
public:
    // ─── 成分
    float x, y, z;

    // ─── コンストラクター
    VECTOR3() : x(0.0f), y(0.0f), z(0.0f) {}
    VECTOR3(const VECTOR3& a) : x(a.x), y(a.y), z(a.z) {}
    VECTOR3(float nx, float ny, float nz) : x(nx), y(ny), z(nz) {}

    // ─── 代入・比較演算子
    VECTOR3& operator =(const VECTOR3& a)
    {
        x = a.x;
        y = a.y;
        z = a.z;
        return *this;
    }

    bool operator ==(const VECTOR3& a) const { return x == a.x && y == a.y && z == a.z; }
    bool operator !=(const VECTOR3& a) const { return x != a.x || y != a.y || z != a.z; }

    // ─── ベクトル初期化
    void Zero() { x = y = z = 0.0f; }

    // ─── 単項演算子
    VECTOR3 operator -() const { return VECTOR3(-x, -y, -z); }

    // ─── 二項算術演算子
    VECTOR3 operator +(const VECTOR3& a) const { return VECTOR3(x + a.x, y + a.y, z + a.z); }
    VECTOR3 operator -(const VECTOR3& a) const { return VECTOR3(x - a.x, y - a.y, z - a.z); }

    VECTOR3 operator *(float a) const { return VECTOR3(x * a, y * a, z * a); }
    VECTOR3 operator /(float a) const
    {
        float lOneOverA = 1.0f / a;
        return VECTOR3(x * lOneOverA, y * lOneOverA, z * lOneOverA);
    }

    // ─── 複合代入演算子
    VECTOR3& operator +=(const VECTOR3& a)
    {
        x += a.x; y += a.y; z += a.z;
        return *this;
    }
    VECTOR3& operator -=(const VECTOR3& a)
    {
        x -= a.x; y -= a.y; z -= a.z;
        return *this;
    }
    VECTOR3& operator *=(float a)
    {
        x *= a; y *= a; z *= a;
        return *this;
    }
    VECTOR3& operator /=(float a)
    {
        float lOneOverA = 1.0f / a;
        x *= lOneOverA; y *= lOneOverA; z *= lOneOverA;
        return *this;
    }

    // ─── ベクトル操作
    VECTOR3 Normalize()
    {
        float lMagSq = x * x + y * y + z * z;
        if (lMagSq > 0.0f)
        {
            float lOneOverMag = 1.0f / sqrtf(lMagSq);
            x *= lOneOverMag;
            y *= lOneOverMag;
            z *= lOneOverMag;
        }

        return VECTOR3{ x, y, z };
    }

    float Length() const { return sqrtf(x * x + y * y + z * z); }

    // ─── 静的演算関数
    static float Dot(const VECTOR3& a, const VECTOR3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static VECTOR3 Cross(const VECTOR3& a, const VECTOR3& b)
    {
        return VECTOR3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }
};

// ─── グローバル演算子・ユーティリティ
inline VECTOR3 operator *(float k, const VECTOR3& v)
{
    return VECTOR3(k * v.x, k * v.y, k * v.z);
}

inline float Distance(const VECTOR3& a, const VECTOR3& b)
{
    float lDx = a.x - b.x;
    float lDy = a.y - b.y;
    float lDz = a.z - b.z;
    return sqrtf(lDx * lDx + lDy * lDy + lDz * lDz);
}