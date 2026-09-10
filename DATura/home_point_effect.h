#pragma once

#include <cstdint>
#include <d3d9.h>
#include <memory>
#include <vector>

namespace HomePoint
{
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
