#include "stdafx.h"
#include "ffxi_sqle_model_animation.h"

#include "d3d_model_buffers.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
int ExpectedChannelCount(const noesisModel_t* model, int fileIndex)
{
    int count = 0;
    if (!model)
        return 0;
    for (const FFXISqleBoneInfo& bone : model->sqleBones)
    {
        if (bone.fileIndex != fileIndex)
            continue;
        for (int group = 0; group < 5; ++group)
            count += bone.channelCounts[group];
    }
    return count;
}

void ReadNormalizedQuaternion(const FFXISqle::MotionInfo& motion, int frameIndex,
                              int channelOffset, float outQuaternion[4])
{
    const size_t frameOffset = static_cast<size_t>(frameIndex) *
                               static_cast<size_t>(motion.channelCount);
    for (int component = 0; component < 4; ++component)
        outQuaternion[component] = motion.frameValues[frameOffset + channelOffset + component];
    FFXISqle::NormalizeQuaternion(outQuaternion);
}

void AlignQuaternionHemisphere(float quaternion[4], const float reference[4])
{
    const float dot = quaternion[0] * reference[0] + quaternion[1] * reference[1] +
                      quaternion[2] * reference[2] + quaternion[3] * reference[3];
    if (dot < 0.0f)
        for (int component = 0; component < 4; ++component)
            quaternion[component] = -quaternion[component];
}

float QuaternionAngularDistanceDegrees(const float a[4], const float b[4])
{
    const float dot = std::fabs(a[0] * b[0] + a[1] * b[1] +
                                a[2] * b[2] + a[3] * b[3]);
    const float clampedDot = std::min(1.0f, dot);
    return 2.0f * std::acos(clampedDot) * (180.0f / 3.14159265358979323846f);
}

void RepairPbQuaternionTracks(const noesisModel_t* model, int fileIndex,
                              FFXISqle::MotionInfo& motion)
{
    if (!model || !motion.pbChannel || motion.frameCount < 2 || motion.frameValues.empty())
        return;

    constexpr float kUnitLengthTolerance = 0.05f;
    int channelCursor = 0;
    for (const FFXISqleBoneInfo& bone : model->sqleBones)
    {
        if (bone.fileIndex != fileIndex)
            continue;

        int boneChannelCount = 0;
        for (int group = 0; group < 5; ++group)
            boneChannelCount += bone.channelCounts[group];

        const int quaternionOffset = channelCursor + bone.channelCounts[0];
        channelCursor += boneChannelCount;
        if (bone.channelCounts[1] < 4 || quaternionOffset + 3 >= motion.channelCount)
            continue;

        std::vector<unsigned char> badFrames(static_cast<size_t>(motion.frameCount), 0);
        for (int frameIndex = 0; frameIndex < motion.frameCount; ++frameIndex)
        {
            const size_t offset = static_cast<size_t>(frameIndex) *
                                  static_cast<size_t>(motion.channelCount) +
                                  static_cast<size_t>(quaternionOffset);
            const float x = motion.frameValues[offset + 0];
            const float y = motion.frameValues[offset + 1];
            const float z = motion.frameValues[offset + 2];
            const float w = motion.frameValues[offset + 3];
            const float length = std::sqrt(x * x + y * y + z * z + w * w);
            if (!std::isfinite(length) || std::fabs(1.0f - length) > kUnitLengthTolerance)
                badFrames[static_cast<size_t>(frameIndex)] = 1;
        }

        // Bad PB rotations commonly arrive in clusters separated by only one
        // or two apparently valid samples. Close those short holes so one
        // smooth bridge spans the entire corrupt interval.
        for (int frameIndex = 0; frameIndex < motion.frameCount;)
        {
            if (!badFrames[static_cast<size_t>(frameIndex)])
            {
                ++frameIndex;
                continue;
            }
            int badEnd = frameIndex;
            while (badEnd < motion.frameCount && badFrames[static_cast<size_t>(badEnd)])
                ++badEnd;
            int nextBad = badEnd;
            while (nextBad < motion.frameCount && !badFrames[static_cast<size_t>(nextBad)])
                ++nextBad;
            if (nextBad < motion.frameCount && nextBad - badEnd <= 2)
            {
                for (int fill = badEnd; fill < nextBad; ++fill)
                    badFrames[static_cast<size_t>(fill)] = 1;
                frameIndex = badEnd;
            }
            else
            {
                frameIndex = badEnd;
            }
        }

        for (int frameIndex = 0; frameIndex < motion.frameCount;)
        {
            if (!badFrames[static_cast<size_t>(frameIndex)])
            {
                ++frameIndex;
                continue;
            }

            int badEnd = frameIndex;
            while (badEnd < motion.frameCount && badFrames[static_cast<size_t>(badEnd)])
                ++badEnd;
            const int previousFrame = frameIndex - 1;
            const int nextFrame = badEnd < motion.frameCount ? badEnd : -1;
            if (previousFrame < 0 && nextFrame < 0)
            {
                frameIndex = badEnd;
                continue;
            }

            float startQuaternion[4] = {};
            float endQuaternion[4] = {};
            ReadNormalizedQuaternion(motion,
                previousFrame >= 0 ? previousFrame : nextFrame,
                quaternionOffset, startQuaternion);
            ReadNormalizedQuaternion(motion,
                nextFrame >= 0 ? nextFrame : previousFrame,
                quaternionOffset, endQuaternion);
            AlignQuaternionHemisphere(endQuaternion, startQuaternion);

            const int intervalLength = badEnd - previousFrame;
            float startTangent[4] = {};
            float endTangent[4] = {};
            if (previousFrame > 0 && !badFrames[static_cast<size_t>(previousFrame - 1)])
            {
                float before[4] = {};
                ReadNormalizedQuaternion(motion, previousFrame - 1, quaternionOffset, before);
                AlignQuaternionHemisphere(before, startQuaternion);
                for (int component = 0; component < 4; ++component)
                    startTangent[component] =
                        (startQuaternion[component] - before[component]) * intervalLength;
            }
            if (nextFrame >= 0 && nextFrame + 1 < motion.frameCount &&
                !badFrames[static_cast<size_t>(nextFrame + 1)])
            {
                float after[4] = {};
                ReadNormalizedQuaternion(motion, nextFrame + 1, quaternionOffset, after);
                AlignQuaternionHemisphere(after, endQuaternion);
                for (int component = 0; component < 4; ++component)
                    endTangent[component] =
                        (after[component] - endQuaternion[component]) * intervalLength;
            }

            for (int repairedFrame = frameIndex; repairedFrame < badEnd; ++repairedFrame)
            {
                const float t = static_cast<float>(repairedFrame - previousFrame) /
                                static_cast<float>(intervalLength);
                const float t2 = t * t;
                const float t3 = t2 * t;
                const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
                const float h10 = t3 - 2.0f * t2 + t;
                const float h01 = -2.0f * t3 + 3.0f * t2;
                const float h11 = t3 - t2;
                float repaired[4] = {};
                for (int component = 0; component < 4; ++component)
                {
                    repaired[component] = h00 * startQuaternion[component] +
                        h10 * startTangent[component] + h01 * endQuaternion[component] +
                        h11 * endTangent[component];
                }
                FFXISqle::NormalizeQuaternion(repaired);
                const size_t offset = static_cast<size_t>(repairedFrame) *
                                      static_cast<size_t>(motion.channelCount) +
                                      static_cast<size_t>(quaternionOffset);
                for (int component = 0; component < 4; ++component)
                    motion.frameValues[offset + component] = repaired[component];
            }
            frameIndex = badEnd;
        }

        // Remove isolated unit-length detours which jump out and immediately
        // return. Genuine fast rotations keep travelling in the same direction
        // and therefore have much larger start-to-end displacement.
        for (int frameIndex = 1; frameIndex + 1 < motion.frameCount; ++frameIndex)
        {
            float before[4] = {};
            float current[4] = {};
            float after[4] = {};
            ReadNormalizedQuaternion(motion, frameIndex - 1, quaternionOffset, before);
            ReadNormalizedQuaternion(motion, frameIndex, quaternionOffset, current);
            const float stepIn = QuaternionAngularDistanceDegrees(before, current);
            if (stepIn <= 30.0f)
                continue;
            ReadNormalizedQuaternion(motion, frameIndex + 1, quaternionOffset, after);
            const float stepOut = QuaternionAngularDistanceDegrees(current, after);
            const float netTravel = QuaternionAngularDistanceDegrees(before, after);
            if (netTravel >= 0.5f * (stepIn + stepOut) - 15.0f)
                continue;
            AlignQuaternionHemisphere(after, before);
            float repaired[4] = {};
            for (int component = 0; component < 4; ++component)
                repaired[component] = 0.5f * (before[component] + after[component]);
            FFXISqle::NormalizeQuaternion(repaired);
            const size_t offset = static_cast<size_t>(frameIndex) *
                                  static_cast<size_t>(motion.channelCount) +
                                  static_cast<size_t>(quaternionOffset);
            for (int component = 0; component < 4; ++component)
                motion.frameValues[offset + component] = repaired[component];
        }
    }
}

RichMat43 BuildLocalMatrix(const FFXISqleBoneInfo& bone, const FFXISqle::MotionInfo* motion,
                           int sourceFrame, int& channelCursor)
{
    const int boneChannelStart = channelCursor;
    float translation[3] = { bone.bindTranslation[0], bone.bindTranslation[1], bone.bindTranslation[2] };
    float quaternion[4] = { bone.bindQuaternion[0], bone.bindQuaternion[1], bone.bindQuaternion[2], bone.bindQuaternion[3] };
    float scale[3] = { bone.bindScale[0], bone.bindScale[1], bone.bindScale[2] };
    float* groups[3] = { translation, quaternion, scale };
    const int capacities[3] = { 3, 4, 3 };
    for (int group = 0; group < 5; ++group)
    {
        const int componentCount = bone.channelCounts[group];
        for (int component = 0; component < componentCount; ++component)
        {
            if (motion && group < 3 && component < capacities[group])
                groups[group][component] = FFXISqle::FrameValue(*motion, sourceFrame, channelCursor);
            ++channelCursor;
        }
    }
    FFXISqle::NormalizeQuaternion(quaternion);

    RichMat43 local = RichQuat(quaternion[0], quaternion[1], quaternion[2], quaternion[3]).ToMat43(false);
    local[0] = local[0] * scale[0];
    local[1] = local[1] * scale[1];
    local[2] = local[2] * scale[2];
    local[3] = RichVec3(translation);

    static const float kSigns[3] = { 1.0f, -1.0f, 1.0f };
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            local[row][column] *= kSigns[row] * kSigns[column];
    for (int axis = 0; axis < 3; ++axis)
        local[3][axis] *= kSigns[axis];
    if (bone.parentIndex < 0)
        for (int axis = 0; axis < 3; ++axis)
            local[3][axis] += bone.rootOffset[axis];
    return local;
}
}

namespace FFXISqleModelAnimation
{
float SuggestedPreviewStartTime(const FFXISqle::MotionInfo& motion)
{
    if (!motion.valid || !motion.pbChannel || motion.frameCount < 2 ||
        motion.channelCount <= 0 || motion.frameValues.empty())
    {
        return 0.0f;
    }

    // Average scalar-channel movement over half a second. Quantisation makes
    // an authored hold very slightly non-zero, so require sustained energy
    // well above that floor. Do not skip a short anticipation pose: this is
    // only a startup convenience for presentations with a multi-second hold.
    constexpr int kWindowFrames = 15;
    constexpr int kMinimumHoldFrames = 60;
    constexpr float kMeanChannelDeltaThreshold = 0.00005f;
    std::vector<float> frameEnergy(static_cast<size_t>(motion.frameCount), 0.0f);
    for (int frameIndex = 1; frameIndex < motion.frameCount; ++frameIndex)
    {
        const size_t current = static_cast<size_t>(frameIndex) *
                               static_cast<size_t>(motion.channelCount);
        const size_t previous = current - static_cast<size_t>(motion.channelCount);
        float totalDelta = 0.0f;
        for (int channelIndex = 0; channelIndex < motion.channelCount; ++channelIndex)
        {
            totalDelta += std::fabs(motion.frameValues[current + channelIndex] -
                                    motion.frameValues[previous + channelIndex]);
        }
        frameEnergy[static_cast<size_t>(frameIndex)] =
            totalDelta / static_cast<float>(motion.channelCount);
    }

    float windowEnergy = 0.0f;
    for (int frameIndex = 1; frameIndex < motion.frameCount; ++frameIndex)
    {
        windowEnergy += frameEnergy[static_cast<size_t>(frameIndex)];
        if (frameIndex >= kWindowFrames)
            windowEnergy -= frameEnergy[static_cast<size_t>(frameIndex - kWindowFrames)];
        if (frameIndex < kWindowFrames)
            continue;

        const int windowStart = frameIndex - kWindowFrames + 1;
        if (windowStart >= kMinimumHoldFrames &&
            windowEnergy / static_cast<float>(kWindowFrames) > kMeanChannelDeltaThreshold)
        {
            return static_cast<float>(windowStart) / 30.0f;
        }
    }
    return 0.0f;
}

bool SampleRootMotion(const noesisModel_t* model, float animationTime,
                      float outTranslation[3])
{
    if (outTranslation)
        outTranslation[0] = outTranslation[1] = outTranslation[2] = 0.0f;
    if (!model || !model->pAnim || !outTranslation || model->pAnim->frameCount <= 0 ||
        model->pAnim->frameRootMotion.size() !=
            static_cast<size_t>(model->pAnim->frameCount))
    {
        return false;
    }

    const noesisAnim_t& animation = *model->pAnim;
    const float framePosition = std::fmod(
        std::max(0.0f, animationTime) * animation.fps,
        static_cast<float>(animation.frameCount));
    const int frameIndex = static_cast<int>(framePosition);
    const int nextFrameIndex = (frameIndex + 1) % animation.frameCount;
    const float blend = framePosition - static_cast<float>(frameIndex);
    const RichVec3& current = animation.frameRootMotion[static_cast<size_t>(frameIndex)];
    const RichVec3& next = animation.frameRootMotion[static_cast<size_t>(nextFrameIndex)];
    for (int axis = 0; axis < 3; ++axis)
        outTranslation[axis] = current[axis] * (1.0f - blend) + next[axis] * blend;
    return true;
}

bool GetRootMotionOrigin(const noesisModel_t* model, float outTranslation[3])
{
    if (outTranslation)
        outTranslation[0] = outTranslation[1] = outTranslation[2] = 0.0f;
    if (!model || !model->pAnim || !outTranslation ||
        model->pAnim->frameRootMotion.empty())
    {
        return false;
    }

    for (int axis = 0; axis < 3; ++axis)
        outTranslation[axis] = model->pAnim->rootMotionOrigin[axis];
    return true;
}

void BuildSkeletalAnimation(noesisModel_t* model, const FFXISqle::MotionInfo& bodyMotion,
                            const FFXISqle::MotionInfo& headMotion)
{
    if (!model || model->sqleBones.empty())
        return;

    FFXISqle::MotionInfo repairedMotions[2] = { bodyMotion, headMotion };
    const FFXISqle::MotionInfo* motions[2] = { &repairedMotions[0], &repairedMotions[1] };
    bool compatible[2] = {};
    for (int fileIndex = 0; fileIndex < 2; ++fileIndex)
    {
        const int expected = ExpectedChannelCount(model, fileIndex);
        compatible[fileIndex] = expected > 0 && motions[fileIndex]->valid &&
            !motions[fileIndex]->frameValues.empty() && expected == motions[fileIndex]->channelCount;
    }
    if (!compatible[0] || !compatible[1])
        return;

    RepairPbQuaternionTracks(model, 0, repairedMotions[0]);
    RepairPbQuaternionTracks(model, 1, repairedMotions[1]);

    const FFXISqle::MotionInfo& timing = compatible[0] ? *motions[0] : *motions[1];
    const int frameCount = std::max(1, timing.frameCount);
    noesisAnim_t* animation = new noesisAnim_t();
    animation->frameCount = frameCount;
    // Both SQLE encodings are authored for flat 30 Hz playback. FrameChannel
    // header time is not a duration suitable for deriving FPS and otherwise
    // makes the short clips run at approximately double speed.
    animation->fps = 30.0f;
    animation->boneCount = (int)model->sqleBones.size();
    animation->allowExtendedPoseBounds = timing.pbChannel;
    animation->frameWorldMats.resize((size_t)animation->frameCount * (size_t)animation->boneCount);

    int bodyAttachmentBone = -1;
    int bodyRootBone = -1;
    int headAttachmentBone = -1;
    int headBoneStart = animation->boneCount;
    for (int boneIndex = 0; boneIndex < animation->boneCount; ++boneIndex)
    {
        const FFXISqleBoneInfo& bone = model->sqleBones[static_cast<size_t>(boneIndex)];
        if (bone.fileIndex == 0)
        {
            if (bone.parentIndex < 0 && bodyRootBone < 0)
                bodyRootBone = boneIndex;
            if (bone.sourceBoneIndex == 4)
                bodyAttachmentBone = boneIndex;
        }
        else if (bone.fileIndex == 1)
        {
            headBoneStart = std::min(headBoneStart, boneIndex);
            if (bone.sourceBoneIndex == 1)
                headAttachmentBone = boneIndex;
        }
    }

    RichVec3 bodyRootStart;
    const bool extractRootMotion = timing.pbChannel && bodyRootBone >= 0;
    if (extractRootMotion)
    {
        const FFXISqleBoneInfo& rootBone =
            model->sqleBones[static_cast<size_t>(bodyRootBone)];
        int rootCursor = 0;
        bodyRootStart = BuildLocalMatrix(rootBone, motions[0], 0, rootCursor)[3];
        animation->rootMotionOrigin = bodyRootStart;
        animation->frameRootMotion.resize(static_cast<size_t>(frameCount));
    }

    RichVec3 bindAttachmentDelta;
    bool haveHeadAttachment = bodyAttachmentBone >= 0 && headAttachmentBone >= 0 &&
                              headBoneStart < animation->boneCount;
    if (haveHeadAttachment)
    {
        std::vector<RichMat43> bindWorlds(static_cast<size_t>(animation->boneCount));
        int bindChannelCursors[8] = {};
        for (int boneIndex = 0; boneIndex < animation->boneCount; ++boneIndex)
        {
            const FFXISqleBoneInfo& bone = model->sqleBones[static_cast<size_t>(boneIndex)];
            const int fileIndex = bone.fileIndex;
            bindWorlds[static_cast<size_t>(boneIndex)] = BuildLocalMatrix(
                bone, nullptr, 0,
                bindChannelCursors[(fileIndex >= 0 && fileIndex < 8) ? fileIndex : 0]);
            if (bone.parentIndex >= 0 && bone.parentIndex < boneIndex)
                bindWorlds[static_cast<size_t>(boneIndex)] =
                    bindWorlds[static_cast<size_t>(boneIndex)] *
                    bindWorlds[static_cast<size_t>(bone.parentIndex)];
        }
        bindAttachmentDelta =
            bindWorlds[static_cast<size_t>(bodyAttachmentBone)][3] -
            bindWorlds[static_cast<size_t>(headAttachmentBone)][3];
    }

    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
    {
        int channelCursors[8] = {};
        RichMat43* worldMatrices = &animation->frameWorldMats[
            (size_t)frameIndex * (size_t)animation->boneCount];
        for (int boneIndex = 0; boneIndex < animation->boneCount; ++boneIndex)
        {
            const FFXISqleBoneInfo& bone = model->sqleBones[(size_t)boneIndex];
            const int fileIndex = bone.fileIndex;
            const FFXISqle::MotionInfo* motion = (fileIndex >= 0 && fileIndex < 2 && compatible[fileIndex]) ?
                motions[fileIndex] : nullptr;
            int sourceFrame = 0;
            if (motion)
            {
                const float phase = frameCount > 1 ? (float)frameIndex / (float)(frameCount - 1) : 0.0f;
                sourceFrame = (int)(phase * (float)std::max(0, motion->frameCount - 1));
            }
            worldMatrices[boneIndex] = BuildLocalMatrix(
                bone, motion, sourceFrame, channelCursors[(fileIndex >= 0 && fileIndex < 8) ? fileIndex : 0]);
            if (bone.parentIndex >= 0 && bone.parentIndex < boneIndex)
                worldMatrices[boneIndex] = worldMatrices[boneIndex] * worldMatrices[bone.parentIndex];
        }

        if (haveHeadAttachment)
        {
            const RichVec3 correction =
                worldMatrices[bodyAttachmentBone][3] - worldMatrices[headAttachmentBone][3] -
                bindAttachmentDelta;
            for (int boneIndex = headBoneStart; boneIndex < animation->boneCount; ++boneIndex)
            {
                if (model->sqleBones[static_cast<size_t>(boneIndex)].fileIndex == 1)
                    worldMatrices[boneIndex][3] += correction;
            }
        }

        // PB translation is an authored actor trajectory, not a deformation
        // that should remain buried inside every skinned vertex. Extract it
        // from the body root, make the pose root-relative, and let the scene
        // apply the same trajectory to the actor's world matrix.
        if (extractRootMotion)
        {
            const RichVec3 rootMotion = worldMatrices[bodyRootBone][3] - bodyRootStart;
            animation->frameRootMotion[static_cast<size_t>(frameIndex)] = rootMotion;
            for (int boneIndex = 0; boneIndex < animation->boneCount; ++boneIndex)
                worldMatrices[boneIndex][3] = worldMatrices[boneIndex][3] - rootMotion;
        }
    }
    model->pAnim = animation;
}

void UpdatePreview(noesisModel_t* model, IDirect3DDevice9* device, int animationIndex, float deltaSeconds,
                   float* inOutAnimationTime, const FFXISqle::MotionInfo& bodyMotion,
                   const FFXISqle::MotionInfo& headMotion)
{
    if (!model || !device || !inOutAnimationTime)
        return;

    if (animationIndex <= 0)
    {
        *inOutAnimationTime = 0.0f;
        model->RestoreBindPose(device);
        return;
    }

    *inOutAnimationTime += deltaSeconds;
    if (model->pAnim)
    {
        model->UpdateAnimation(*inOutAnimationTime, device);
        return;
    }

    const FFXISqle::MotionInfo& timing =
        (bodyMotion.valid && bodyMotion.frameChannel) ? bodyMotion : headMotion;
    if (!timing.valid || !timing.frameChannel || timing.frameValues.empty())
    {
        model->RestoreBindPose(device);
        return;
    }

    const int frameCount = timing.frameCount > 1 ? timing.frameCount : 1;
    const float duration = static_cast<float>(frameCount) / 30.0f;
    int frameIndex = static_cast<int>(std::fmod(*inOutAnimationTime, duration) * 30.0f);
    frameIndex = std::max(0, std::min(frameIndex, frameCount - 1));

    model->RestoreBindPose(device);
    for (noesisModel_t::Submesh& submesh : model->submeshes)
    {
        const bool isHead = std::strncmp(submesh.materialName.c_str(), "creation_mat_1", 14) == 0;
        const FFXISqle::MotionInfo& motion = isHead ? headMotion : bodyMotion;
        ApplyFrameChannel(submesh, motion, frameIndex);
    }
}

void ApplyFrameChannel(noesisModel_t::Submesh& submesh, const FFXISqle::MotionInfo& motion,
                       int frameIndex)
{
    if (!motion.valid || !motion.frameChannel || motion.frameValues.empty() ||
        submesh.cpuBindVerts.empty() || submesh.cpuBindVerts.size() != submesh.cpuVerts.size())
    {
        return;
    }

    submesh.cpuVerts = submesh.cpuBindVerts;
    for (FFXIVertex& vertex : submesh.cpuVerts)
    {
        float sourcePosition[3] = { vertex.pos[0], -vertex.pos[1], vertex.pos[2] };
        const int groupIndex = FFXISqle::FindNearestTransformGroup(motion, sourcePosition);
        if (groupIndex < 0)
            continue;

        float bindTranslation[3] = {};
        float frameTranslation[3] = {};
        float bindQuaternion[4] = {};
        float frameQuaternion[4] = {};
        if (!FFXISqle::ReadTransformGroup(motion, 0, groupIndex, bindTranslation, bindQuaternion) ||
            !FFXISqle::ReadTransformGroup(motion, frameIndex, groupIndex,
                                           frameTranslation, frameQuaternion))
        {
            continue;
        }

        float inverseBindQuaternion[4] = {};
        float deltaQuaternion[4] = {};
        FFXISqle::InvertQuaternion(bindQuaternion, inverseBindQuaternion);
        FFXISqle::MultiplyQuaternions(frameQuaternion, inverseBindQuaternion, deltaQuaternion);

        float localPosition[3] =
        {
            sourcePosition[0] - bindTranslation[0],
            sourcePosition[1] - bindTranslation[1],
            sourcePosition[2] - bindTranslation[2]
        };
        float rotatedLocalPosition[3] = {};
        FFXISqle::RotateVector(deltaQuaternion, localPosition, rotatedLocalPosition);

        sourcePosition[0] += (frameTranslation[0] - bindTranslation[0]) +
                             (rotatedLocalPosition[0] - localPosition[0]);
        sourcePosition[1] += (frameTranslation[1] - bindTranslation[1]) +
                             (rotatedLocalPosition[1] - localPosition[1]);
        sourcePosition[2] += (frameTranslation[2] - bindTranslation[2]) +
                             (rotatedLocalPosition[2] - localPosition[2]);

        vertex.pos[0] = sourcePosition[0];
        vertex.pos[1] = -sourcePosition[1];
        vertex.pos[2] = sourcePosition[2];

        float sourceNormal[3] = { vertex.nrm[0], -vertex.nrm[1], vertex.nrm[2] };
        float rotatedNormal[3] = {};
        FFXISqle::RotateVector(deltaQuaternion, sourceNormal, rotatedNormal);
        vertex.nrm[0] = rotatedNormal[0];
        vertex.nrm[1] = -rotatedNormal[1];
        vertex.nrm[2] = rotatedNormal[2];
    }

    D3DModelBuffers::UploadVertices(submesh);
}
}
