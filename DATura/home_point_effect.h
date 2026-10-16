#pragma once

#include <cstdint>
#include <d3d9.h>
#include <memory>
#include <vector>
#include "d3d_math.h"
#include "home_point_data.h"

namespace HomePoint
{
inline D3DMATRIX ApplyMeshRotation(D3DMATRIX local, const Vec& rotation,
                                   const Vec& spin, bool localSpin)
{
    if (localSpin)
    {
        local = D3DMath::Multiply(local, D3DMath::BuildRotationX(spin[0]));
        local = D3DMath::Multiply(local, D3DMath::BuildRotationY(spin[1]));
        local = D3DMath::Multiply(local, D3DMath::BuildRotationZ(spin[2]));
    }
    local = D3DMath::Multiply(local, D3DMath::BuildRotationX(rotation[0]));
    local = D3DMath::Multiply(local, D3DMath::BuildRotationY(rotation[1]));
    return D3DMath::Multiply(local, D3DMath::BuildRotationZ(rotation[2]));
}
inline constexpr unsigned int ModelId = 0x33;
inline constexpr float NameplateY = -2.7f;
inline constexpr float InteractionDistance = 6.0f;

struct Instance
{
    uint32_t id = 0;
    float position[3] = {};
    double activatedAt = -1000.0;
};

class Effect
{
  public:
    Effect();
    ~Effect();
    Effect(const Effect &) = delete;
    Effect &operator=(const Effect &) = delete;
    bool Load(IDirect3DDevice9 *device, const char *installation, bool compression);
    // Tower entrance/exit rings and player-attached effect-1 activation assets.
    bool LoadTowerPortal(IDirect3DDevice9 *device, const char *installation, int zone,
                         bool compression, bool upperFloor = false);
    void DrawActivation(IDirect3DDevice9 *device, const D3DMATRIX &world, const float *eye,
                        double seconds, double activatedAt, bool mipMapping);
    void Draw(IDirect3DDevice9 *device, const D3DMATRIX &world, const float *eye, double seconds,
              double activatedAt, bool mipMapping);
    // Audio is separate from PlaySound/BGM and belongs to the loaded zone.
    void UpdateSound(const std::vector<Instance> &instances, const float *listener, const float *right,
                     bool allowed, int maxSounds);
    void ActivateSound(const Instance &instance, const float *listener, const float *right);
    void StopSound();
    size_t LayerCount() const;
    size_t TriangleCount() const;
    size_t ActiveParticleCount(double seconds, double activatedAt) const;
    bool HasSounds() const;
    size_t SoundVoiceCount() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace HomePoint
