// ============================================================
// CloudPS.hlsl
// Cloud pass pixel shader (PHASE 2: volumetric raymarching).
// Raymarches a horizontal cloud slab; fbm noise drives density and
// a short light-march toward the sun gives in-cloud shadowing
// (Beer + Henyey-Greenstein). Alpha-composited over the frame;
// scene geometry occludes it via the depth test set on the C++ side.
// Author: Chan WingChung
// ============================================================

cbuffer CB_CLOUD : register(b0)
{
    float4x4 invViewProj;
    float3   camPos;
    float    time;
    float3   sunDir;
    float    coverage;
    float    cloudBottom;
    float    cloudTop;
    int      marchSteps;
    int      lightSteps;
    float    densityMult;
    float    wind;
    float    _pad0, _pad1;
};

struct VS_OUTPUT_CLOUD
{
    float4 svPosition : SV_POSITION;
    float2 clip : TEXCOORD0;
};

// --- tunables (phase 3 moves these into the constant buffer + ImGui)
static const float DENSITY_SCALE = 0.05f;
static const float3 SUN_COLOR = float3(1.00f, 0.95f, 0.85f);
static const float3 SKY_AMBIENT = float3(0.50f, 0.60f, 0.75f);

// ------------------------------------------------------------
// hash / value noise / fbm (self-contained 3D noise)
// ------------------------------------------------------------
float Hash(float3 p)
{
    p = frac(p * 0.3183099f + 0.1f);
    p *= 17.0f;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float ValueNoise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f); // smoothstep interpolation

    // --- trilinear blend of the 8 corner hashes
    float n000 = Hash(i + float3(0, 0, 0));
    float n100 = Hash(i + float3(1, 0, 0));
    float n010 = Hash(i + float3(0, 1, 0));
    float n110 = Hash(i + float3(1, 1, 0));
    float n001 = Hash(i + float3(0, 0, 1));
    float n101 = Hash(i + float3(1, 0, 1));
    float n011 = Hash(i + float3(0, 1, 1));
    float n111 = Hash(i + float3(1, 1, 1));

    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);
    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);
    return lerp(nxy0, nxy1, f.z);
}

float Fbm(float3 p)
{
    float lSum = 0.0f;
    float lAmp = 0.5f;
    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        lSum += lAmp * ValueNoise(p);
        p *= 2.02f;
        lAmp *= 0.5f;
    }
    return lSum;
}

// ------------------------------------------------------------
// SampleDensity : cloud density at a world position
// ------------------------------------------------------------
float SampleDensity(float3 worldPos)
{
    // --- scroll along X for wind
    float3 lSamplePos = worldPos;
    lSamplePos.x += time * wind;

    float lNoise = Fbm(lSamplePos * DENSITY_SCALE);

    // --- coverage carves away low-density noise
    float lBase = max(lNoise - (1.0f - coverage), 0.0f);

    // --- fade toward slab top/bottom for rounded edges
    float lH = saturate((worldPos.y - cloudBottom) / (cloudTop - cloudBottom));
    float lFade = lH * (1.0f - lH) * 4.0f; // parabola, peaks at center

    return lBase * lFade * densityMult;
}

// ------------------------------------------------------------
// LightMarch : Beer transmittance from a point toward the sun
// ------------------------------------------------------------
float LightMarch(float3 pos, float3 toSun)
{
    float lStep = (cloudTop - cloudBottom) / (float) lightSteps;
    float lSum = 0.0f;
    [loop]
    for (int i = 0; i < lightSteps; ++i)
    {
        pos += toSun * lStep;
        lSum += SampleDensity(pos);
    }
    return exp(-lSum * lStep);
}

// ------------------------------------------------------------
// HenyeyGreenstein : forward-scattering phase (silver lining)
// ------------------------------------------------------------
float HenyeyGreenstein(float cosAngle, float g)
{
    float g2 = g * g;
    return (1.0f - g2) / (4.0f * 3.14159265f * pow(1.0f + g2 - 2.0f * g * cosAngle, 1.5f));
}

// ------------------------------------------------------------
// main : ray-slab intersection + volumetric raymarch
// ------------------------------------------------------------
float4 main(VS_OUTPUT_CLOUD input) : SV_TARGET
{
    // --- rebuild the world-space view ray
    float4 lWorldH = mul(float4(input.clip, 1.0f, 1.0f), invViewProj);
    float3 lWorldFar = lWorldH.xyz / lWorldH.w;
    float3 lRayDir = normalize(lWorldFar - camPos);

    // --- skip near-horizontal rays (avoid div by zero on the slab)
    if (abs(lRayDir.y) < 1e-4f)
        return float4(0, 0, 0, 0);

    // --- intersect ray with the horizontal slab
    float lT0 = (cloudBottom - camPos.y) / lRayDir.y;
    float lT1 = (cloudTop - camPos.y) / lRayDir.y;
    float lEnter = max(min(lT0, lT1), 0.0f);
    float lExit = max(lT0, lT1);
    if (lExit <= lEnter)
        return float4(0, 0, 0, 0);

    // --- march the slab segment
    float3 lToSun = normalize(sunDir);
    float lPhase = HenyeyGreenstein(dot(lRayDir, lToSun), 0.6f);

    float lStep = (lExit - lEnter) / (float) marchSteps;
    float3 lPos = camPos + lRayDir * lEnter;

    float lTransmittance = 1.0f;
    float3 lScatter = float3(0, 0, 0);

    [loop]
    for (int i = 0; i < marchSteps; ++i)
    {
        float lDensity = SampleDensity(lPos);
        if (lDensity > 0.0f)
        {
            float lLight = LightMarch(lPos, lToSun);
            float3 lLit = SUN_COLOR * lLight * lPhase + SKY_AMBIENT;

            lScatter += lLit * lDensity * lStep * lTransmittance;
            lTransmittance *= exp(-lDensity * lStep);

            if (lTransmittance < 0.01f)   // basically opaque -> stop
                break;
        }
        lPos += lRayDir * lStep;
    }

    return float4(lScatter, saturate(1.0f - lTransmittance));
}