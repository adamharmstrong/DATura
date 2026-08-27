#pragma once

#include "model_ff11.h"

#pragma pack(push, 1)

class CFFXISkelHandler : public CFFXIChunkHandler
{
public:
	struct SSkelHeader
	{
		short mUnknown;
		short mBoneCount;
	};

	struct SSkelBone
	{
		unsigned char mParentIndex; // parent bone index; if equal to this bone's own index, it is the root
		unsigned char mTerm;        // terminal/leaf bone flag (BONE.term in FFXI Tool source)
		RichQuat mQuat;
		RichVec3 mTran;
	};

	struct SInterpretedSkel
	{
		explicit SInterpretedSkel(modelBone_t *pBones, const int boneCount)
			: mpBones(pBones)
			, mBoneCount(boneCount)
		{
		}

		modelBone_t *mpBones;
		int mBoneCount;
	};
	typedef std::vector<SInterpretedSkel> TSInterpretedSkelList;

	CFFXISkelHandler()
		: CFFXIChunkHandler(CFFXIDat::skChunkType_Skeleton)
	{
	}

	virtual CFFXIDat::EValidateChunkResult ValidateChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk,
															const unsigned char *pChunkData, const int dataSize) const
	{
		// Content validation skipped â€” data is encrypted here; decryption happens in HandleChunk.
		return (dataSize >= 0) ? CFFXIDat::kVCR_Supported : CFFXIDat::kVCR_Invalid;
	}

	virtual bool HandleChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk,
								const unsigned char *pChunkData, const int dataSize)
	{
		const SSkelHeader *pSkelHdr = (const SSkelHeader *)pChunkData;
		const SSkelBone *pSkelBones = (const SSkelBone *)(pSkelHdr + 1);

		noeRAPI_t *pRapi = dat.GetRAPI();
		modelBone_t *pBones = pRapi->Noesis_AllocBones(pSkelHdr->mBoneCount);

		for (int boneIndex = 0; boneIndex < pSkelHdr->mBoneCount; ++boneIndex)
		{
			const SSkelBone *pSkelBone = pSkelBones + boneIndex;
			modelBone_t *pBone = pBones + boneIndex;
			RichMat43 &boneMat = (RichMat43 &)pBone->mat;

			pBone->index = boneIndex;
			sprintf_s(pBone->name, "bone%04i", boneIndex);
			boneMat = pSkelBone->mQuat.ToMat43(false);
			boneMat[3] = pSkelBone->mTran;
			pBone->eData.parent = (pSkelBone->mParentIndex != boneIndex) ? pBones + pSkelBone->mParentIndex : NULL;
		}

		pRapi->rpgMultiplyBones(pBones, pSkelHdr->mBoneCount);

		mSkeletons.push_back(SInterpretedSkel(pBones, pSkelHdr->mBoneCount));

		return true;
	}

	TSInterpretedSkelList &Skeletons() { return mSkeletons; }

protected:
	TSInterpretedSkelList mSkeletons;
};

//========================================================================================

class CFFXIAnimHandler : public CFFXIChunkHandler
{
public:
	struct SAnimHeader
	{
		unsigned short mUnknown;
		unsigned short mElemCount;
		unsigned short mFrameCount;
		float mSpeedScale; //factor with desired frame interval
	};

	//each animation element may reference up to 10 channels, 1 for each component of each transform element
	struct SAnimElemHeader
	{
		int mBoneIndex;
		int mQuatIndex[4];
		RichQuat mQuatBase;
		int mTranIndex[3];
		RichVec3 mTranBase;
		int mScaleIndex[3];
		RichVec3 mScaleBase;
	};

	struct SAnimHeaderData
	{
		explicit SAnimHeaderData(const SAnimHeader *pAnimHdr, const int animDataSize, const char *pName)
			: mpAnimHdr(pAnimHdr)
			, mAnimDataSize(animDataSize)
		{
			memcpy(mName, pName, 4);
			mName[4] = 0;
		}

		const SAnimHeader *mpAnimHdr;
		int mAnimDataSize;
		char mName[8];
	};
	typedef std::vector<SAnimHeaderData> TAnimHeaderList;

	CFFXIAnimHandler()
		: CFFXIChunkHandler(CFFXIDat::skChunkType_Animation)
	{
	}

	virtual CFFXIDat::EValidateChunkResult ValidateChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk,
															const unsigned char *pChunkData, const int dataSize) const
	{
		// Content validation skipped â€” data is encrypted here; decryption happens in HandleChunk.
		return (dataSize >= 0) ? CFFXIDat::kVCR_Supported : CFFXIDat::kVCR_Invalid;
	}

	virtual bool HandleChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk,
								const unsigned char *pChunkData, const int dataSize)
	{
		const SAnimHeader *pAnimHdr = (const SAnimHeader *)pChunkData;
		mAnimHeaderList.push_back(SAnimHeaderData(pAnimHdr, dataSize, chunk.mName));
		return true;
	}

	noesisAnim_t *ConstructAnimations(noeRAPI_t *pRapi, const CFFXISkelHandler::SInterpretedSkel *pSkel) const
	{
		CArrayList<noesisAnim_t *> anims;

		const SAnimHeaderData *pWalkLower = FindAnimation("wlk0");
		const SAnimHeaderData *pWalkUpper = FindAnimation("wlk1");
		if (pWalkLower && pWalkUpper &&
			pWalkLower->mpAnimHdr->mFrameCount == pWalkUpper->mpAnimHdr->mFrameCount &&
			pWalkLower->mpAnimHdr->mFrameCount > 0)
		{
			noesisAnim_t *pWalkAnim = ConstructCompositedAnimation(pRapi, pSkel, pWalkLower, pWalkUpper, "wlk");
			if (pWalkAnim)
			{
				anims.Append(pWalkAnim);
			}
		}

		for (TAnimHeaderList::const_iterator it = mAnimHeaderList.begin(); it != mAnimHeaderList.end(); ++it)
		{
			const SAnimHeaderData &animHeaderData = *it;
			const SAnimHeader *pAnimHdr = animHeaderData.mpAnimHdr;
			if (pAnimHdr->mFrameCount > 0 && pAnimHdr->mElemCount > 0)
			{
				const int transformCount = pSkel->mBoneCount * pAnimHdr->mFrameCount;
				//create a parent-relative base frame from the skeleton
				RichMat43 *pBaseMats = (RichMat43 *)pRapi->Noesis_UnpooledAlloc(sizeof(RichMat43) * pSkel->mBoneCount);
				for (int boneIndex = 0; boneIndex < pSkel->mBoneCount; ++boneIndex)
				{
					const modelBone_t *pBone = pSkel->mpBones + boneIndex;
					const RichMat43 &boneMat = (const RichMat43 &)pBone->mat;
					if (!pBone->eData.parent)
					{
						pBaseMats[boneIndex] = boneMat;
					}
					else
					{
						const RichMat43 &parentBoneMat = (const RichMat43 &)pBone->eData.parent->mat;
						pBaseMats[boneIndex] = boneMat * parentBoneMat.GetInverse();
					}
				}

				//initialize the anim frames with the default pose
				RichMat43 *pMats = (RichMat43 *)pRapi->Noesis_UnpooledAlloc(sizeof(RichMat43) * transformCount);
				for (int frameIndex = 0; frameIndex < pAnimHdr->mFrameCount; ++frameIndex)
				{
					memcpy(pMats + frameIndex * pSkel->mBoneCount, pBaseMats, sizeof(RichMat43) * pSkel->mBoneCount);
				}

				//now modify the transforms
				const SAnimElemHeader *pElems = (const SAnimElemHeader *)(pAnimHdr + 1);
				const float *pAnimData = (const float *)(pElems + pAnimHdr->mElemCount);
				const int animDataOfs = (int)sizeof(SAnimHeader) + (int)sizeof(SAnimElemHeader) * pAnimHdr->mElemCount;
				const int animDataBaseFloat = ((int)sizeof(SAnimElemHeader) * pAnimHdr->mElemCount) / (int)sizeof(float);
				const int animFloatCount = (animHeaderData.mAnimDataSize > animDataOfs) ?
					(animHeaderData.mAnimDataSize - animDataOfs) / (int)sizeof(float) : 0;
				RichQuat frameQ;
				RichVec3 frameT;
				RichVec3 frameS;
				for (int elemIndex = 0; elemIndex < pAnimHdr->mElemCount; ++elemIndex)
				{
					const SAnimElemHeader *pElem = pElems + elemIndex;
					if (pElem->mBoneIndex < 0 || pElem->mBoneIndex >= pSkel->mBoneCount)
					{
						pRapi->LogOutput("WARNING: Out of range animation element! (index %i, but only %i bones)\n",
							pElem->mBoneIndex, pSkel->mBoneCount);
						continue;
					}

					for (int frameIndex = 0; frameIndex < pAnimHdr->mFrameCount; ++frameIndex)
					{
						RichMat43 *pFrameMats = pMats + pSkel->mBoneCount * frameIndex;
						RichMat43 &mat = pFrameMats[pElem->mBoneIndex];
						if (pElem->mQuatIndex[0] < 0 || pElem->mQuatIndex[1] < 0 || pElem->mQuatIndex[2] < 0 || pElem->mQuatIndex[3] < 0)
						{
							//no change
							mat = pBaseMats[pElem->mBoneIndex];
						}
						else
						{
							const int q0 = pElem->mQuatIndex[0] + frameIndex;
							const int q1 = pElem->mQuatIndex[1] + frameIndex;
							const int q2 = pElem->mQuatIndex[2] + frameIndex;
							const int q3 = pElem->mQuatIndex[3] + frameIndex;
							const int t0 = pElem->mTranIndex[0] + frameIndex;
							const int t1 = pElem->mTranIndex[1] + frameIndex;
							const int t2 = pElem->mTranIndex[2] + frameIndex;
							const int s0 = pElem->mScaleIndex[0] + frameIndex;
							const int s1 = pElem->mScaleIndex[1] + frameIndex;
							const int s2 = pElem->mScaleIndex[2] + frameIndex;

							const int qd0 = q0 - animDataBaseFloat;
							const int qd1 = q1 - animDataBaseFloat;
							const int qd2 = q2 - animDataBaseFloat;
							const int qd3 = q3 - animDataBaseFloat;
							const int td0 = t0 - animDataBaseFloat;
							const int td1 = t1 - animDataBaseFloat;
							const int td2 = t2 - animDataBaseFloat;
							const int sd0 = s0 - animDataBaseFloat;
							const int sd1 = s1 - animDataBaseFloat;
							const int sd2 = s2 - animDataBaseFloat;

							frameQ[0] = (qd0 >= 0 && qd0 < animFloatCount) ? pAnimData[qd0] : pElem->mQuatBase[0];
							frameQ[1] = (qd1 >= 0 && qd1 < animFloatCount) ? pAnimData[qd1] : pElem->mQuatBase[1];
							frameQ[2] = (qd2 >= 0 && qd2 < animFloatCount) ? pAnimData[qd2] : pElem->mQuatBase[2];
							frameQ[3] = (qd3 >= 0 && qd3 < animFloatCount) ? pAnimData[qd3] : pElem->mQuatBase[3];
							// FF11 limb animations are primarily rotational. Keep child bone
							// offsets/scales from the bind pose until the translation/scale
							// channel semantics are fully mapped; bad local offsets collapse
							// feet and shins up into the thighs.
							if (!pSkel->mpBones[pElem->mBoneIndex].eData.parent)
							{
								frameT[0] = (td0 >= 0 && td0 < animFloatCount) ? pAnimData[td0] : pElem->mTranBase[0];
								frameT[1] = (td1 >= 0 && td1 < animFloatCount) ? pAnimData[td1] : pElem->mTranBase[1];
								frameT[2] = (td2 >= 0 && td2 < animFloatCount) ? pAnimData[td2] : pElem->mTranBase[2];
							}
							else
							{
								frameT = pBaseMats[pElem->mBoneIndex][3];
							}
							frameS = pElem->mScaleBase;

							RichMat43 animRot = frameQ.ToMat43(false);
							mat = pBaseMats[pElem->mBoneIndex] * animRot;
							mat[0] = mat[0] * frameS[0];
							mat[1] = mat[1] * frameS[1];
							mat[2] = mat[2] * frameS[2];
							mat[3] = frameT;
						}
					}
				}

				noesisAnim_t *pAnim = pRapi->rpgAnimFromBonesAndMatsFinish(pSkel->mpBones, pSkel->mBoneCount, (modelMatrix_t *)pMats,
																			pAnimHdr->mFrameCount, pAnimHdr->mSpeedScale * 30.0f);
				pRapi->Noesis_UnpooledFree(pBaseMats);
				pRapi->Noesis_UnpooledFree(pMats);
				if (pAnim)
				{
					pAnim->filename = pRapi->Noesis_PooledString(const_cast<char *>(animHeaderData.mName));
					pAnim->flags = NANIMFLAG_FILENAMETOSEQ;
					anims.Append(pAnim);
				}
			}
		}

		return (anims.Num() > 0) ? pRapi->Noesis_AnimFromAnimsList(anims, anims.Num()) : NULL;
	}

	bool AnimDataIsPresent() const { return mAnimHeaderList.size() > 0; }

protected:
	const SAnimHeaderData *FindAnimation(const char *pName) const
	{
		for (TAnimHeaderList::const_iterator it = mAnimHeaderList.begin(); it != mAnimHeaderList.end(); ++it)
		{
			if (!strcmp(it->mName, pName))
				return &(*it);
		}
		return NULL;
	}

	noesisAnim_t *ConstructCompositedAnimation(noeRAPI_t *pRapi, const CFFXISkelHandler::SInterpretedSkel *pSkel,
											   const SAnimHeaderData *pLowerAnimData,
											   const SAnimHeaderData *pUpperAnimData,
											   const char *pName) const
	{
		const SAnimHeader *pAnimHdr = pLowerAnimData->mpAnimHdr;
		const int transformCount = pSkel->mBoneCount * pAnimHdr->mFrameCount;
		RichMat43 *pBaseMats = (RichMat43 *)pRapi->Noesis_UnpooledAlloc(sizeof(RichMat43) * pSkel->mBoneCount);
		for (int boneIndex = 0; boneIndex < pSkel->mBoneCount; ++boneIndex)
		{
			const modelBone_t *pBone = pSkel->mpBones + boneIndex;
			const RichMat43 &boneMat = (const RichMat43 &)pBone->mat;
			if (!pBone->eData.parent)
			{
				pBaseMats[boneIndex] = boneMat;
			}
			else
			{
				const RichMat43 &parentBoneMat = (const RichMat43 &)pBone->eData.parent->mat;
				pBaseMats[boneIndex] = boneMat * parentBoneMat.GetInverse();
			}
		}

		RichMat43 *pMats = (RichMat43 *)pRapi->Noesis_UnpooledAlloc(sizeof(RichMat43) * transformCount);
		for (int frameIndex = 0; frameIndex < pAnimHdr->mFrameCount; ++frameIndex)
		{
			memcpy(pMats + frameIndex * pSkel->mBoneCount, pBaseMats, sizeof(RichMat43) * pSkel->mBoneCount);
		}

		ApplyAnimationChunk(pRapi, pSkel, *pLowerAnimData, pBaseMats, pMats);
		ApplyAnimationChunk(pRapi, pSkel, *pUpperAnimData, pBaseMats, pMats);

		noesisAnim_t *pAnim = pRapi->rpgAnimFromBonesAndMatsFinish(pSkel->mpBones, pSkel->mBoneCount, (modelMatrix_t *)pMats,
																	pAnimHdr->mFrameCount, pAnimHdr->mSpeedScale * 30.0f);
		pRapi->Noesis_UnpooledFree(pBaseMats);
		pRapi->Noesis_UnpooledFree(pMats);
		if (pAnim)
		{
			pAnim->filename = pRapi->Noesis_PooledString(const_cast<char *>(pName));
			pAnim->flags = NANIMFLAG_FILENAMETOSEQ;
		}
		return pAnim;
	}

	void ApplyAnimationChunk(noeRAPI_t *pRapi, const CFFXISkelHandler::SInterpretedSkel *pSkel,
							 const SAnimHeaderData &animHeaderData, const RichMat43 *pBaseMats,
							 RichMat43 *pMats) const
	{
		const SAnimHeader *pAnimHdr = animHeaderData.mpAnimHdr;
		const SAnimElemHeader *pElems = (const SAnimElemHeader *)(pAnimHdr + 1);
		const float *pAnimData = (const float *)(pElems + pAnimHdr->mElemCount);
		const int animDataOfs = (int)sizeof(SAnimHeader) + (int)sizeof(SAnimElemHeader) * pAnimHdr->mElemCount;
		const int animDataBaseFloat = ((int)sizeof(SAnimElemHeader) * pAnimHdr->mElemCount) / (int)sizeof(float);
		const int animFloatCount = (animHeaderData.mAnimDataSize > animDataOfs) ?
			(animHeaderData.mAnimDataSize - animDataOfs) / (int)sizeof(float) : 0;
		RichQuat frameQ;
		RichVec3 frameT;
		RichVec3 frameS;

		for (int elemIndex = 0; elemIndex < pAnimHdr->mElemCount; ++elemIndex)
		{
			const SAnimElemHeader *pElem = pElems + elemIndex;
			if (pElem->mBoneIndex < 0 || pElem->mBoneIndex >= pSkel->mBoneCount)
			{
				pRapi->LogOutput("WARNING: Out of range animation element! (index %i, but only %i bones)\n",
					pElem->mBoneIndex, pSkel->mBoneCount);
				continue;
			}

			for (int frameIndex = 0; frameIndex < pAnimHdr->mFrameCount; ++frameIndex)
			{
				RichMat43 *pFrameMats = pMats + pSkel->mBoneCount * frameIndex;
				RichMat43 &mat = pFrameMats[pElem->mBoneIndex];
				if (pElem->mQuatIndex[0] < 0 || pElem->mQuatIndex[1] < 0 || pElem->mQuatIndex[2] < 0 || pElem->mQuatIndex[3] < 0)
				{
					mat = pBaseMats[pElem->mBoneIndex];
				}
				else
				{
					const int q0 = pElem->mQuatIndex[0] + frameIndex;
					const int q1 = pElem->mQuatIndex[1] + frameIndex;
					const int q2 = pElem->mQuatIndex[2] + frameIndex;
					const int q3 = pElem->mQuatIndex[3] + frameIndex;
					const int t0 = pElem->mTranIndex[0] + frameIndex;
					const int t1 = pElem->mTranIndex[1] + frameIndex;
					const int t2 = pElem->mTranIndex[2] + frameIndex;
					const int qd0 = q0 - animDataBaseFloat;
					const int qd1 = q1 - animDataBaseFloat;
					const int qd2 = q2 - animDataBaseFloat;
					const int qd3 = q3 - animDataBaseFloat;
					const int td0 = t0 - animDataBaseFloat;
					const int td1 = t1 - animDataBaseFloat;
					const int td2 = t2 - animDataBaseFloat;

					frameQ[0] = (qd0 >= 0 && qd0 < animFloatCount) ? pAnimData[qd0] : pElem->mQuatBase[0];
					frameQ[1] = (qd1 >= 0 && qd1 < animFloatCount) ? pAnimData[qd1] : pElem->mQuatBase[1];
					frameQ[2] = (qd2 >= 0 && qd2 < animFloatCount) ? pAnimData[qd2] : pElem->mQuatBase[2];
					frameQ[3] = (qd3 >= 0 && qd3 < animFloatCount) ? pAnimData[qd3] : pElem->mQuatBase[3];
					if (!pSkel->mpBones[pElem->mBoneIndex].eData.parent)
					{
						frameT[0] = (td0 >= 0 && td0 < animFloatCount) ? pAnimData[td0] : pElem->mTranBase[0];
						frameT[1] = (td1 >= 0 && td1 < animFloatCount) ? pAnimData[td1] : pElem->mTranBase[1];
						frameT[2] = (td2 >= 0 && td2 < animFloatCount) ? pAnimData[td2] : pElem->mTranBase[2];
					}
					else
					{
						frameT = pBaseMats[pElem->mBoneIndex][3];
					}
					frameS = pElem->mScaleBase;

					RichMat43 animRot = frameQ.ToMat43(false);
					mat = pBaseMats[pElem->mBoneIndex] * animRot;
					mat[0] = mat[0] * frameS[0];
					mat[1] = mat[1] * frameS[1];
					mat[2] = mat[2] * frameS[2];
					mat[3] = frameT;
				}
			}
		}
	}

	TAnimHeaderList mAnimHeaderList;
};

//========================================================================================

#pragma pack(pop)
