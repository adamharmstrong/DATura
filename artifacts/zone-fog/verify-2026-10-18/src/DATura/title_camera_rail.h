#pragma once

#include <array>
#include <memory>

namespace TitleCameraRail
{
struct Pose
{
    std::array<float, 3> target = {};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 0.0f;
};

class Controller
{
public:
    Controller();
    ~Controller();

    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    void Reset();
    static Pose KonschtatOpeningPose();
    void SetFallbackBase(const Pose& pose);
    bool LoadRetailSchedule(const char* installRoot, const char* scheduleName,
                            bool mirrorX);
    void Advance(float deltaSeconds);
    bool SampleRetail(Pose& pose) const;
    void SampleFallback(Pose& pose) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
