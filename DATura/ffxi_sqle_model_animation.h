#pragma once

#include "ffxi_sqle_motion.h"
#include "noesis_rapi.h"

namespace FFXISqleModelAnimation
{
    // Long PB presentations can begin with several seconds of an authored
    // static hold. Return the first sustained-motion time for initial preview
    // placement, or zero when the sequence starts moving immediately.
    float SuggestedPreviewStartTime(const FFXISqle::MotionInfo& motion);

    // Sample the extracted PB root trajectory at the same fractional frame as
    // the skinned pose. Returns false for clips without extracted root motion.
    bool SampleRootMotion(const noesisModel_t* model, float animationTime,
                          float outTranslation[3]);
    bool GetRootMotionOrigin(const noesisModel_t* model, float outTranslation[3]);

    void BuildSkeletalAnimation(noesisModel_t* model, const FFXISqle::MotionInfo& bodyMotion,
                                const FFXISqle::MotionInfo& headMotion);
    void UpdatePreview(noesisModel_t* model, IDirect3DDevice9* device, int animationIndex, float deltaSeconds,
                       float* inOutAnimationTime, const FFXISqle::MotionInfo& bodyMotion,
                       const FFXISqle::MotionInfo& headMotion);
    void ApplyFrameChannel(noesisModel_t::Submesh& submesh, const FFXISqle::MotionInfo& motion,
                           int frameIndex);
}
