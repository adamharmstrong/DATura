#include "stdafx.h"
#include "title_camera_rail.h"

#include "ffxi_coordinate_frame.h"
#include "ffxi_file_io.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace TitleCameraRail
{
namespace
{
constexpr float kOpeningTarget[3] = { -284.089f, -41.906f, 328.520f };
constexpr float kOpeningYaw = 10.1700f;
constexpr float kOpeningPitch = 0.1900f;
constexpr float kOpeningDistance = 260.0f;
constexpr float kCustomRailDurationSeconds = 48.0f;

struct RailKey
{
    float targetOffset[3];
    float yawOffset;
    float pitch;
    float distance;
};

// DATura's Konschtat title camera rail and fallback for incomplete retail data.
constexpr RailKey kCustomRail[] =
{
    { {  0.0f,  0.0f,   0.0f }, -0.035f, 0.185f, 266.0f },
    { { -5.0f,  1.5f,   7.0f }, -0.010f, 0.178f, 258.0f },
    { { -9.0f,  2.5f,  12.0f },  0.025f, 0.172f, 250.0f },
    { { -4.0f,  1.0f,   6.0f },  0.055f, 0.182f, 255.0f },
    { {  4.0f, -1.0f,  -5.0f },  0.030f, 0.195f, 265.0f },
    { {  6.0f, -1.5f, -10.0f }, -0.010f, 0.198f, 272.0f },
};

struct RetailKey
{
    std::array<float, 3> eye = {};
    std::array<float, 3> at = {};
};

struct RetailCamera
{
    unsigned int easing = 0;
    std::vector<RetailKey> keys;
};

struct RetailShot
{
    const RetailCamera* camera = nullptr;
    float startSeconds = 0.0f;
    float durationSeconds = 0.0f;
};

struct RetailPlayer
{
    std::map<std::string, RetailCamera> cameras;
    std::vector<RetailShot> shots;
    float durationSeconds = 0.0f;
    bool active = false;
};

template<typename T>
bool ReadValue(const BYTE* data, const size_t size, const size_t offset, T* value)
{
    if (!data || !value || offset > size || sizeof(T) > size - offset)
        return false;
    memcpy(value, data + offset, sizeof(T));
    return true;
}

std::string ReadTag(const BYTE* data, const size_t size, const size_t offset)
{
    if (!data || offset > size || 4 > size - offset)
        return {};
    char tag[5] = {};
    memcpy(tag, data + offset, 4);
    return std::string(tag, strnlen_s(tag, 4));
}

float CatmullRom(const float p0, const float p1, const float p2,
                 const float p3, const float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

float ApplyEasing(float t, const unsigned int mode)
{
    constexpr float halfPi = 1.5707963267948966f;
    constexpr float pi = 3.1415926535897932f;
    t = std::clamp(t, 0.0f, 1.0f);
    switch (mode)
    {
    case 1: return std::sin(t * halfPi);
    case 2: return 1.0f - std::cos(t * halfPi);
    case 3:
    case 4: return 0.5f - 0.5f * std::cos(t * pi);
    default: return t;
    }
}

std::array<float, 3> SampleSpline(
    const RetailCamera& camera, const bool eye, const float normalizedTime)
{
    const size_t count = camera.keys.size();
    if (count == 0)
        return {};
    const auto point = [&](const size_t index) -> const std::array<float, 3>&
    {
        return eye ? camera.keys[index].eye : camera.keys[index].at;
    };
    if (count == 1)
        return point(0);

    // Match YmSpline::MakeTable2's chord-length parameterization, then sample
    // a natural cubic spline independently for each coordinate.
    std::vector<float> parameter(count, 0.0f);
    for (size_t i = 1; i < count; ++i)
    {
        float lengthSquared = 0.0f;
        for (int axis = 0; axis < 3; ++axis)
        {
            const float difference = point(i)[axis] - point(i - 1)[axis];
            lengthSquared += difference * difference;
        }
        parameter[i] = parameter[i - 1] + std::sqrt(lengthSquared);
    }
    const float totalLength = parameter.back();
    if (totalLength <= 0.000001f)
        return point(0);
    for (float& value : parameter)
        value /= totalLength;

    const float t = std::clamp(normalizedTime, 0.0f, 1.0f);
    size_t segment = 0;
    while (segment + 2 < count && parameter[segment + 1] < t)
        ++segment;
    const float h = (std::max)(0.000001f,
        parameter[segment + 1] - parameter[segment]);
    const float a = (parameter[segment + 1] - t) / h;
    const float b = (t - parameter[segment]) / h;

    std::array<float, 3> result = {};
    for (int axis = 0; axis < 3; ++axis)
    {
        std::vector<float> second(count, 0.0f);
        if (count > 2)
        {
            std::vector<float> lower(count, 0.0f), diagonal(count, 0.0f);
            std::vector<float> upper(count, 0.0f), rhs(count, 0.0f);
            diagonal[0] = diagonal[count - 1] = 1.0f;
            for (size_t i = 1; i + 1 < count; ++i)
            {
                const float left = parameter[i] - parameter[i - 1];
                const float right = parameter[i + 1] - parameter[i];
                lower[i] = left;
                diagonal[i] = 2.0f * (left + right);
                upper[i] = right;
                rhs[i] = 6.0f * ((point(i + 1)[axis] - point(i)[axis]) / right -
                    (point(i)[axis] - point(i - 1)[axis]) / left);
            }
            for (size_t i = 1; i < count; ++i)
            {
                const float factor = lower[i] / diagonal[i - 1];
                diagonal[i] -= factor * upper[i - 1];
                rhs[i] -= factor * rhs[i - 1];
            }
            for (size_t i = count; i-- > 0;)
            {
                second[i] = (rhs[i] -
                    (i + 1 < count ? upper[i] * second[i + 1] : 0.0f)) /
                    diagonal[i];
            }
        }
        result[axis] = a * point(segment)[axis] + b * point(segment + 1)[axis] +
            ((a * a * a - a) * second[segment] +
             (b * b * b - b) * second[segment + 1]) * h * h / 6.0f;
    }
    return result;
}
}

struct Controller::Impl
{
    RetailPlayer retail;
    float elapsedSeconds = 0.0f;
    Pose fallbackBase;
    bool mirrorX = true;
};

Controller::Controller() : impl_(std::make_unique<Impl>())
{
    Reset();
}

Controller::~Controller() = default;

void Controller::Reset()
{
    impl_->retail = {};
    impl_->elapsedSeconds = 0.0f;
    impl_->fallbackBase = KonschtatOpeningPose();
    impl_->mirrorX = true;
}

Pose Controller::KonschtatOpeningPose()
{
    Pose pose;
    pose.target = { kOpeningTarget[0], kOpeningTarget[1], kOpeningTarget[2] };
    pose.yaw = kOpeningYaw;
    pose.pitch = kOpeningPitch;
    pose.distance = kOpeningDistance;
    return pose;
}

void Controller::SetFallbackBase(const Pose& pose)
{
    impl_->fallbackBase = pose;
}

bool Controller::LoadRetailSchedule(const char* installRoot,
                                    const char* scheduleName,
                                    const bool mirrorX)
{
    impl_->retail = {};
    impl_->mirrorX = mirrorX;
    if (!installRoot || !installRoot[0] || !scheduleName || !scheduleName[0])
        return false;

    char path[MAX_PATH] = {};
    strcpy_s(path, installRoot);
    const size_t length = strlen(path);
    if (length && path[length - 1] != '\\' && path[length - 1] != '/')
        strcat_s(path, "\\");
    strcat_s(path, "ROM\\0\\23.DAT");

    BYTE* bytes = nullptr;
    DWORD byteCount = 0;
    if (!FFXIFileIO::ReadWholeFile(path, &bytes, &byteCount))
        return false;
    std::unique_ptr<BYTE[]> owned(bytes);

    struct SchedulePayload { const BYTE* data = nullptr; size_t size = 0; } schedule;
    for (size_t offset = 0; offset + 16 <= byteCount;)
    {
        unsigned int info = 0;
        if (!ReadValue(bytes, byteCount, offset + 4, &info))
            return false;
        const size_t chunkSize = (info >> 3) & 0x7ffff0;
        const unsigned int type = info & 0x7f;
        if (chunkSize < 16 || chunkSize > byteCount - offset)
            return false;
        const std::string name = ReadTag(bytes, byteCount, offset);
        const BYTE* payload = bytes + offset + 16;
        const size_t payloadSize = chunkSize - 16;

        if (type == 6 && payloadSize >= 32)
        {
            unsigned int mode = 0;
            ReadValue(payload, payloadSize, 16, &mode);
            const unsigned int keyCount = mode & 0xff;
            if (keyCount > 0 && keyCount <= 64 &&
                32 + static_cast<size_t>(keyCount) * 48 <= payloadSize)
            {
                RetailCamera camera;
                ReadValue(payload, payloadSize, 20, &camera.easing);
                for (unsigned int key = 0; key < keyCount; ++key)
                {
                    const size_t keyOffset = 32 + static_cast<size_t>(key) * 48;
                    RetailKey parsed;
                    memcpy(parsed.eye.data(), payload + keyOffset, sizeof(float) * 3);
                    memcpy(parsed.at.data(), payload + keyOffset + 16, sizeof(float) * 3);
                    camera.keys.push_back(parsed);
                }
                impl_->retail.cameras[name] = std::move(camera);
            }
        }
        else if (type == 7 && name == scheduleName)
        {
            schedule = { payload, payloadSize };
        }
        offset += chunkSize;
    }

    if (!schedule.data || schedule.size < 0x20)
        return false;
    unsigned int begin = 0, end = 0;
    if (!ReadValue(schedule.data, schedule.size, 0x14, &begin) ||
        !ReadValue(schedule.data, schedule.size, 0x18, &end) ||
        begin < 16 || end < begin || end - 16 > schedule.size)
        return false;

    float timelineFrames = 0.0f;
    for (size_t cursor = begin - 16; cursor + 4 <= end - 16;)
    {
        const BYTE opcode = schedule.data[cursor];
        const size_t commandSize = (std::max)(1,
            static_cast<int>(schedule.data[cursor + 1])) * 4;
        if (commandSize > end - 16 - cursor)
            return false;
        unsigned short waitFrames = 0;
        ReadValue(schedule.data, schedule.size, cursor + 4, &waitFrames);
        if (opcode == 4 && commandSize >= 12)
        {
            unsigned short durationFrames = 0;
            ReadValue(schedule.data, schedule.size, cursor + 6, &durationFrames);
            const std::string cameraName = ReadTag(
                schedule.data, schedule.size, cursor + 8);
            const auto found = impl_->retail.cameras.find(cameraName);
            if (found != impl_->retail.cameras.end() && durationFrames > 0)
            {
                impl_->retail.shots.push_back({
                    &found->second, timelineFrames / 60.0f,
                    static_cast<float>(durationFrames) / 60.0f });
            }
        }
        timelineFrames += static_cast<float>(waitFrames);
        cursor += commandSize;
    }
    impl_->retail.durationSeconds = timelineFrames / 60.0f;
    impl_->retail.active = !impl_->retail.shots.empty() &&
        impl_->retail.durationSeconds > 0.0f;
    return impl_->retail.active;
}

void Controller::Advance(const float deltaSeconds)
{
    if (deltaSeconds > 0.0f && std::isfinite(deltaSeconds))
        impl_->elapsedSeconds += deltaSeconds;
}

bool Controller::SampleRetail(Pose& pose) const
{
    const RetailPlayer& retail = impl_->retail;
    if (!retail.active || retail.shots.empty())
        return false;
    const float time = std::fmod((std::max)(0.0f, impl_->elapsedSeconds),
        retail.durationSeconds);
    const RetailShot* shot = &retail.shots.front();
    for (const RetailShot& candidate : retail.shots)
    {
        if (candidate.startSeconds > time)
            break;
        shot = &candidate;
    }
    const float progress = shot->durationSeconds > 0.0f
        ? std::clamp((time - shot->startSeconds) / shot->durationSeconds, 0.0f, 1.0f)
        : 1.0f;
    const float eased = ApplyEasing(progress, shot->camera->easing);
    auto eye = SampleSpline(*shot->camera, true, eased);
    auto at = SampleSpline(*shot->camera, false, eased);
    eye = FFXICoordinateFrame::NativeDatToScene(eye, impl_->mirrorX);
    at = FFXICoordinateFrame::NativeDatToScene(at, impl_->mirrorX);

    const float dx = eye[0] - at[0];
    const float dy = eye[1] - at[1];
    const float dz = eye[2] - at[2];
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(distance) || distance <= 0.0001f)
        return false;
    pose.target = at;
    pose.distance = distance;
    pose.yaw = std::atan2(dx, dz);
    pose.pitch = std::asin(std::clamp(dy / distance, -1.0f, 1.0f));
    return true;
}

void Controller::SampleFallback(Pose& pose) const
{
    const int count = static_cast<int>(std::size(kCustomRail));
    const float wrapped = std::fmod((std::max)(0.0f, impl_->elapsedSeconds),
        kCustomRailDurationSeconds);
    const float position = wrapped * static_cast<float>(count) /
        kCustomRailDurationSeconds;
    const int key1 = static_cast<int>(position) % count;
    const int key0 = (key1 + count - 1) % count;
    const int key2 = (key1 + 1) % count;
    const int key3 = (key1 + 2) % count;
    const float blend = position - std::floor(position);

    for (int axis = 0; axis < 3; ++axis)
    {
        pose.target[axis] = impl_->fallbackBase.target[axis] + CatmullRom(
            kCustomRail[key0].targetOffset[axis],
            kCustomRail[key1].targetOffset[axis],
            kCustomRail[key2].targetOffset[axis],
            kCustomRail[key3].targetOffset[axis], blend);
    }
    pose.yaw = impl_->fallbackBase.yaw + CatmullRom(
        kCustomRail[key0].yawOffset, kCustomRail[key1].yawOffset,
        kCustomRail[key2].yawOffset, kCustomRail[key3].yawOffset, blend);
    pose.pitch = impl_->fallbackBase.pitch - kOpeningPitch + CatmullRom(
        kCustomRail[key0].pitch, kCustomRail[key1].pitch,
        kCustomRail[key2].pitch, kCustomRail[key3].pitch, blend);
    pose.distance = impl_->fallbackBase.distance - kOpeningDistance + CatmullRom(
        kCustomRail[key0].distance, kCustomRail[key1].distance,
        kCustomRail[key2].distance, kCustomRail[key3].distance, blend);
}
}
