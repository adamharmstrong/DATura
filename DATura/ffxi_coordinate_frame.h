#pragma once

#include <array>
#include <cmath>
#include <utility>

// DATura's parser emits native DAT positions, including the complete MZB or
// generator placement. Room geometry and catalog NPC positions use this same
// frame. The scene keeps native Y (positive downward) and Z; normal zone view
// reflects X. The UI's "Mirror World Zones" option exposes the native frame,
// so callers pass !mirrorWorldZones here: mirrorX means APPLY the reflection.
//
// This contract is specific to DATura's D3D9 scene. An upstream renderer's
// (x, -y, -z) convention must not be applied to these already placed vertices.
// Parser visibility, LOD and object metadata stay native; convert query points
// back instead of modifying that metadata. Reflections act equally on points,
// displacement vectors and normals, but reverse triangle winding as well.
namespace FFXICoordinateFrame
{
using Vector = std::array<float, 3>;

inline Vector NativeDatToScene(const Vector& native, const bool mirrorX)
{
    return { mirrorX ? -native[0] : native[0], native[1], native[2] };
}

inline Vector SceneToNativeDat(const Vector& scene, const bool mirrorX)
{
    return NativeDatToScene(scene, mirrorX); // An X reflection is its own inverse.
}

// The pointer overloads accept the float[3] fields used by parser and renderer
// records. Input and output may be the same array.
inline void NativeDatToScene(const float native[3], const bool mirrorX, float scene[3])
{
    const Vector result = NativeDatToScene({ native[0], native[1], native[2] }, mirrorX);
    for (int axis = 0; axis < 3; ++axis) scene[axis] = result[axis];
}

inline void SceneToNativeDat(const float scene[3], const bool mirrorX, float native[3])
{
    NativeDatToScene(scene, mirrorX, native);
}

template<class T>
inline void ReverseTriangleWindingIfReflected(T& second, T& third, const bool mirrorX)
{
    if (mirrorX) std::swap(second, third);
}

// Catalog headings and character-local forward use +X at zero and -Z at pi/2.
// Under an X reflection, (cos(h), -sin(h)) becomes (-cos(h), -sin(h)).
// This is NOT the camera-relative yaw used by the player controller.
inline float NativeDatHeadingToScene(const float heading, const bool mirrorX)
{
    if (!mirrorX) return heading;
    constexpr float pi = 3.14159265358979323846f;
    constexpr float twoPi = 2.0f * pi;
    const float reflected = std::fmod(pi - heading, twoPi);
    return reflected < 0.0f ? reflected + twoPi : reflected;
}

inline float SceneHeadingToNativeDat(const float heading, const bool mirrorX)
{
    return NativeDatHeadingToScene(heading, mirrorX);
}
}
