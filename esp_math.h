#pragma once
#include <cmath>

// ============================================================
//  ESP_MATH
//  Vector classes and WorldToScreen projection formula, ported
//  verbatim from the internal project except WorldToScreen: there's
//  no ImGui IO struct here, so screen size is an explicit parameter
//  instead of ImGui::GetIO().DisplaySize.
// ============================================================

struct Vector2 {
    float x, y;
};

struct Vector3 {
    float x, y, z;

    Vector3() : x(0.f), y(0.f), z(0.f) {}
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vector3 operator-(const Vector3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vector3 operator+(const Vector3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vector3 operator*(float s)          const { return { x * s,   y * s,   z * s   }; }
    float   Length()     const { return std::sqrt(x * x + y * y + z * z); }
    float   LengthSqr()  const { return x * x + y * y + z * z; }
    float   Dot(const Vector3& o) const { return x * o.x + y * o.y + z * o.z; }
};

struct Vector4 {
    float x, y, z, w;
};

namespace Math {

    constexpr float PI = 3.14159265358979323846f;

    inline void ClampAngles(Vector2& a) {
        if (a.x >  89.f) a.x =  89.f;
        if (a.x < -89.f) a.x = -89.f;
        while (a.y >  180.f) a.y -= 360.f;
        while (a.y < -180.f) a.y += 360.f;
    }

    // Projects a 3D world space coordinate into 2D screen space using a
    // row-major view-matrix. Returns false if the coordinate is behind the
    // camera (w <= 0). screenW/screenH are the overlay's tracked size
    // (the game window's client rect), replacing ImGui::GetIO().DisplaySize.
    inline bool WorldToScreen(const Vector3& world, Vector2& screen, const float view[16],
                               float screenW, float screenH) {
        // Map 1D float[16] array to 2D float[4][4] row-major view matrix
        const float (*m)[4] = reinterpret_cast<const float (*)[4]>(view);

        float w = m[3][0] * world.x + m[3][1] * world.y + m[3][2] * world.z + m[3][3];
        if (w < 0.01f) {
            return false;
        }

        float invW = 1.0f / w;
        float x = (m[0][0] * world.x + m[0][1] * world.y + m[0][2] * world.z + m[0][3]) * invW;
        float y = (m[1][0] * world.x + m[1][1] * world.y + m[1][2] * world.z + m[1][3]) * invW;

        screen.x = (screenW * 0.5f) + (x * screenW * 0.5f);
        screen.y = (screenH * 0.5f) - (y * screenH * 0.5f);

        return true;
    }

}
