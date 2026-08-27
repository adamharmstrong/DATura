#pragma once

#include "model_ff11.h"
#include "model_ff11_internal.h"
#include "model_ff11_animation_handlers.h"
#include "model_ff11_texture_handler.h"

#pragma pack(push, 1)

class CFFXIGeoHandler : public CFFXIChunkHandler
{
public:
	struct SGeoHeader
	{
		// Most offsets/sizes are in units of shorts (i.e. byte offset = field * 2).
		// Byte offsets in comments below match DAT2AHeader (FFXI Tool source, "ffxi.h").
		short mUnknown1;                 // 0x00: ver (byte) + nazo (byte)
		unsigned short mVertAndBoneRefFlag; // 0x02: type; low 7 bits: 0=normal model, 1=cloth/class; bit 7: use bone refs
		unsigned short mMirror;          // 0x04: flip; 0=OFF, non-zero=ON

		int mDrawDataOfs;                // 0x06: offsetPoly - draw command stream
		unsigned short mDrawDataSize;    // 0x0A: PolySuu

		int mBoneRefOfs;                 // 0x0C: offsetBoneTbl
		unsigned short mBoneRefCount;    // 0x10: BoneTblSuu

		//weird setup - offset to short-sized counts
		int mWeightedVertCountOfs;       // 0x12: offsetWeight
		unsigned short mMaxWeightsPerVertex; // 0x16: WeightSuu

		//each data entry contains 2 bone indices and mirror type (see SGeoWeightData)
		int mWeightDataOfs;              // 0x18: offsetBone
		unsigned short mWeightDataCount; // 0x1C: BoneSuu

		int mVertOfs;                    // 0x1E: offsetVertex
		unsigned short mVertDataSize;    // 0x22: VertexSuu

		// LOD polygon set 1 (lower detail draw list, loaded conditionally)
		int mPolyLod1Ofs;                // 0x24: offsetPolyLoad
		unsigned short mPolyLod1Count;   // 0x28: PolyLoadSuu
		unsigned short mPolyLod1VertCount0; // 0x2A: PolyLodVtx0Suu
		unsigned short mPolyLod1VertCount1; // 0x2C: PolyLodVtx1Suu

		// LOD polygon set 2 (lowest detail draw list)
		int mPolyLod2Ofs;                // 0x2E: offsetPolyLod2
		unsigned short mPolyLod2Count;   // 0x32: PolyLod2Suu

		int mUnknown8;                   // 0x34: nazo1
		int mUnknown9;                   // 0x38: nazo2
		unsigned short mUnknown10;       // 0x3C: nazo3
		unsigned short mUnknown11;       // 0x3E: nazo4
	};

	struct SGeoDrawState
	{
		SGeoDrawState()
		{
			mEnableReflection = false;
			mReflectionTextureFactorAlpha = 0.0f;
			mMaterialName[0] = 0;
		}

		bool mEnableReflection;
		float mReflectionTextureFactorAlpha;
		char mMaterialName[CFFXITextureHandler::skTexNameLength + 1];
	};

	struct SGeoTriPrim
	{
		unsigned short mIndices[3];
		RichVec2 mUVs[3];
	};

	struct SGeoStripPrim
	{
		unsigned short mIndex;
		RichVec2 mUV;
	};

	struct SGeoWeightData
	{
		unsigned short mBoneIndexPass0 : 7; //unmirrored index
		unsigned short mBoneIndexPass1 : 7; //mirrored index
		unsigned short mMirrorAxis : 2; //only relevant for mirror pass
	};

	struct SGeoNativeDrawState
	{
		unsigned char mTextureFactorBGRA[4]; // +0x00
		unsigned char mState04[8];           // +0x05 contains the blend mode
		unsigned char mState0C[4];           // +0x0F is the display type
		float mReflectionEnable;             // +0x10: 1.0 enables the environment stage
		float mUnknown14;
		float mUnknown18;
		float mUnknown1C;
		unsigned int mUnknown20;
		float mReflectionIntensity;          // +0x24: TEXTUREFACTOR alpha = value * 0.5
		float mUnknown28;
	};
	static_assert(sizeof(SGeoNativeDrawState) == 44, "Unexpected 0x8010 state size.");

	struct SGeoHeaderData
	{
		explicit SGeoHeaderData(const SGeoHeader *pGeoHdr, const int geoDataSize, const char *pName)
			: mpGeoHdr(pGeoHdr)
			, mGeoDataSize(geoDataSize)
		{
			memcpy(mName, pName, 4);
			mName[4] = 0;
		}

		const SGeoHeader *mpGeoHdr;
		int mGeoDataSize;
		char mName[8];
	};
	typedef std::vector<SGeoHeaderData> TGeoHeaderList;

	static const int skVertFlag_NoNormals = 0x7F; //if any of these bits are set, seems to indicate lack of normals. not sure what else this means.
	static const int skVertFlag_UseBoneRefs = 0x80;

	static const int skDrawCmd_SetMaterial = 0x8000;
	static const int skDrawCmd_TriList = 0x0054;
	static const int skDrawCmd_TriStrip = 0x5453;
	static const int skDrawCmd_DrawState = 0x8010;
	static const int skDrawCmd_Unknown2 = 0x4353;
	static const int skDrawCmd_Unknown3 = 0x0043;
	static const int skDrawCmd_End = 0xFFFF;

	static const int skTriCWIdx[3];
	static const int skTriCCWIdx[3];
	static const RichMat44 skMirrorTransforms[3];

	CFFXIGeoHandler()
		: CFFXIChunkHandler(CFFXIDat::skChunkType_Geo)
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
		const SGeoHeader *pGeoHdr = (const SGeoHeader *)pChunkData;
		mGeoHeaderList.push_back(SGeoHeaderData(pGeoHdr, dataSize, chunk.mName));
		return true;
	}

	void RenderGeoData(noeRAPI_t *pRapi, const CFFXISkelHandler::SInterpretedSkel *pSkel) const
	{
		for (TGeoHeaderList::const_iterator it = mGeoHeaderList.begin(); it != mGeoHeaderList.end(); ++it)
		{
			const SGeoHeaderData &geoHeaderData = *it;
			const SGeoHeader *pGeoHdr = geoHeaderData.mpGeoHdr;
			const unsigned short *pSData = (const unsigned short *)pGeoHdr;

			pRapi->rpgSetName(const_cast<char *>(geoHeaderData.mName));

			SGeoDrawState drawState;

			const unsigned char *pDrawCommands = (const unsigned char *)(pSData + pGeoHdr->mDrawDataOfs);
			const int drawCommandEndOfs = (pGeoHdr->mDrawDataSize << 1);
			const int drawCommandLastCommandOfs = drawCommandEndOfs - sizeof(unsigned short);
			const int passCount = (pGeoHdr->mMirror) ? 2 : 1;
			for (int passIndex = 0; passIndex < passCount; ++passIndex)
			{
				const bool isMirroring = (passIndex > 0);
				int drawCommandOfs = 0;
				while (drawCommandOfs <= drawCommandLastCommandOfs)
				{
					const unsigned short cmdType = *get_and_incr_offset<unsigned short>(pDrawCommands, drawCommandOfs);
					switch (cmdType)
					{
					case skDrawCmd_DrawState:
						{
							const SGeoNativeDrawState *pNativeState = get_and_incr_offset<SGeoNativeDrawState>(pDrawCommands, drawCommandOfs);
							drawState.mEnableReflection = pNativeState->mReflectionEnable == 1.0f;
							drawState.mReflectionTextureFactorAlpha =
								std::max(0.0f, std::min(255.0f,
									pNativeState->mReflectionIntensity * 0.5f));
							UpdateDrawState(pRapi, drawState);
						}
						break;

					case skDrawCmd_SetMaterial:
						{
							const char *pMatNameData = get_and_incr_offset<char>(pDrawCommands, drawCommandOfs, CFFXITextureHandler::skTexNameLength);
							memcpy(drawState.mMaterialName, pMatNameData, CFFXITextureHandler::skTexNameLength);
							drawState.mMaterialName[CFFXITextureHandler::skTexNameLength] = 0;

							UpdateDrawState(pRapi, drawState);
						}
						break;

					case skDrawCmd_TriList:
						{
							//tri list
							const unsigned short primCount = *get_and_incr_offset<unsigned short>(pDrawCommands, drawCommandOfs);
							const int primDataSize = sizeof(SGeoTriPrim) * primCount;
							const SGeoTriPrim *pTris = get_and_incr_offset<SGeoTriPrim>(pDrawCommands, drawCommandOfs, primCount);

							pRapi->rpgBegin(RPGEO_TRIANGLE);

							const int *pTriWindIdx = (isMirroring) ? CFFXIGeoHandler::skTriCCWIdx : CFFXIGeoHandler::skTriCWIdx;
							for (int triIdx = 0; triIdx < primCount; ++triIdx)
							{
								const SGeoTriPrim *pTri = pTris + triIdx;
								for (int triVertIdx = 0; triVertIdx < 3; ++triVertIdx)
								{
									const int windIdx = pTriWindIdx[triVertIdx];
									//really need to fix the fucking constness on these functions one of these days.
									pRapi->rpgVertUV2f(const_cast<float *>(pTri->mUVs[windIdx].v), 0);
									PlotVertex(pRapi, pTri->mIndices[windIdx], isMirroring, pGeoHdr, pSkel);
								}
							}

							pRapi->rpgEnd();
						}
						break;

					case skDrawCmd_TriStrip:
						{
							//tri strip
							const unsigned short primCount = *get_and_incr_offset<unsigned short>(pDrawCommands, drawCommandOfs);
							const int stripVertCount = primCount - 1;
							const SGeoTriPrim *pFirstTri = get_and_incr_offset<SGeoTriPrim>(pDrawCommands, drawCommandOfs);
							const SGeoStripPrim *pStripVerts = get_and_incr_offset<SGeoStripPrim>(pDrawCommands, drawCommandOfs, stripVertCount);

							const rpgeoPrimType_e stripType = (isMirroring) ? RPGEO_TRIANGLE_STRIP_FLIPPED : RPGEO_TRIANGLE_STRIP;
							pRapi->rpgBegin(stripType);

							for (int triVertIdx = 0; triVertIdx < 3; ++triVertIdx)
							{
								pRapi->rpgVertUV2f(const_cast<float *>(pFirstTri->mUVs[triVertIdx].v), 0);
								PlotVertex(pRapi, pFirstTri->mIndices[triVertIdx], isMirroring, pGeoHdr, pSkel);
							}
							for (int stripIdx = 0; stripIdx < stripVertCount; ++stripIdx)
							{
								const SGeoStripPrim *pStripVert = pStripVerts + stripIdx;
								pRapi->rpgVertUV2f(const_cast<float *>(pStripVert->mUV.v), 0);
								PlotVertex(pRapi, pStripVert->mIndex, isMirroring, pGeoHdr, pSkel);
							}

							pRapi->rpgEnd();
						}
						break;

					case skDrawCmd_Unknown2:
						{
							const unsigned short primCount = *get_and_incr_offset<unsigned short>(pDrawCommands, drawCommandOfs);
							get_and_incr_offset<unsigned char>(pDrawCommands, drawCommandOfs, 8);
							get_and_incr_offset<unsigned short>(pDrawCommands, drawCommandOfs, primCount);
						}
						break;

					case skDrawCmd_Unknown3:
						{
							const unsigned short primCount = *get_and_incr_offset<unsigned short>(pDrawCommands, drawCommandOfs);
							get_and_incr_offset<unsigned char>(pDrawCommands, drawCommandOfs, primCount * 10);
						}
						break;

					default:
						{
							NoeAssert(cmdType == skDrawCmd_End);
							drawCommandOfs = drawCommandEndOfs;
						}
						break;
					}
				}
			}
		}

		if (pSkel)
		{
			pRapi->rpgSetExData_Bones(pSkel->mpBones, pSkel->mBoneCount);
		}
	}

	bool GeoDataIsPresent() const { return mGeoHeaderList.size() > 0; }

protected:
	static void PlotVertex(noeRAPI_t *pRapi, const int index, const bool isMirroring,
							const SGeoHeader *pGeoHdr, const CFFXISkelHandler::SInterpretedSkel *pSkel)
	{
		const unsigned short *pSData = (const unsigned short *)pGeoHdr;
		const bool isSkinned = (pSkel && pGeoHdr->mWeightedVertCountOfs > 0 && pGeoHdr->mWeightDataOfs > 0);
		const bool noNormals = (pGeoHdr->mVertAndBoneRefFlag & skVertFlag_NoNormals) != 0;
		int oneWeightVertCount;
		if (pGeoHdr->mWeightedVertCountOfs > 0)
		{
			const unsigned short *pWeightedCounts = pSData + pGeoHdr->mWeightedVertCountOfs;
			oneWeightVertCount = pWeightedCounts[0];
		}
		else
		{
			//consider every vert single-weight
			oneWeightVertCount = 0x10000;
		}

		const int weightCount = (index < oneWeightVertCount) ? 1 : 2;
		const int oneWeightVertSize = (noNormals) ? sizeof(float) * 3 : sizeof(float) * 6;
		const int twoWeightVertSize = oneWeightVertSize * 2 + sizeof(float) * 2;
		const int oneWeightVertElemCount = oneWeightVertSize / sizeof(float);
		const int twoWeightVertElemCount = twoWeightVertSize / sizeof(float);

		const int firstTwoWeightElemIndex = oneWeightVertCount * oneWeightVertElemCount;

		const float *pVertElems = (const float *)(pSData + pGeoHdr->mVertOfs);
		const float *pVertData = (weightCount == 1) ? pVertElems + index * oneWeightVertElemCount :
									pVertElems + firstTwoWeightElemIndex + (index - oneWeightVertCount) * twoWeightVertElemCount;
		const float *pNrmVertData = (noNormals) ? NULL : pVertData + 3 * weightCount + ((weightCount > 1) ? weightCount : 0);

		//interleaved elements for each weight, pretty nasty.
		if (!isSkinned)
		{
			pRapi->rpgVertBoneIndexI(NULL, 0);
			pRapi->rpgVertBoneWeightF(NULL, 0);
			pRapi->rpgSetPendingSkinData(NULL);
			if (pNrmVertData)
			{
				const RichVec3 nrm(pNrmVertData[0 * weightCount], pNrmVertData[1 * weightCount], pNrmVertData[2 * weightCount]);
				pRapi->rpgVertNormal3f(const_cast<float *>(nrm.v));
			}
			else
			{
				pRapi->rpgVertNormal3f(NULL);
			}
			const RichVec3 pos(pVertData[0 * weightCount], pVertData[1 * weightCount], pVertData[2 * weightCount]);
			pRapi->rpgVertex3f(const_cast<float *>(pos.v));
		}
		else
		{
			const unsigned short *pBoneRefs = ((pGeoHdr->mVertAndBoneRefFlag & skVertFlag_UseBoneRefs) && pGeoHdr->mBoneRefOfs > 0) ?
												pSData + pGeoHdr->mBoneRefOfs : NULL;

			static const float skDefaultWeights[2] = { 1.0f, 0.0f };
			const SGeoWeightData *pWeightDatas = (const SGeoWeightData *)(pSData + pGeoHdr->mWeightDataOfs) + index * 2;
			int boneIndices[2] = { 0, 0 };
			RichMat44 skinMats[2]; //we pull the matrices out, as we may end up needing to modify them for mirroring anyway. could be optimized.
			for (int weightIndex = 0; weightIndex < weightCount; ++weightIndex)
			{
				const SGeoWeightData *pWeightData = pWeightDatas + weightIndex;
				int &boneIndex = boneIndices[weightIndex];
				boneIndex = (isMirroring) ? pWeightData->mBoneIndexPass1 : pWeightData->mBoneIndexPass0;
				NoeAssert(!pBoneRefs || boneIndex < pGeoHdr->mBoneRefCount);
				if (pBoneRefs && boneIndex < pGeoHdr->mBoneRefCount)
				{
					boneIndex = pBoneRefs[boneIndex];
				}
				NoeAssert(boneIndex >= 0 && boneIndex < pSkel->mBoneCount);
				skinMats[weightIndex] = ((RichMat43 *)&pSkel->mpBones[boneIndex].mat)->ToMat44();
				if (isMirroring && pWeightData->mMirrorAxis)
				{
					skinMats[weightIndex] = skinMats[weightIndex] * skMirrorTransforms[pWeightData->mMirrorAxis - 1];
				}
			}
			const float *pWeightValues = (weightCount > 1) ? pVertData + 3 * weightCount : skDefaultWeights;
			//feed 2 weights to Noesis in order to keep consistent per-triangle. Noesis doesn't require this, but this allows the draws between
			//1 and 2 weight segments to not be partitioned.
			pRapi->rpgVertBoneIndexI(const_cast<int *>(boneIndices), 2);
			pRapi->rpgVertBoneWeightF(const_cast<float *>(pWeightValues), 2);

			FFXISkinVertex skinVertex;
			skinVertex.skinned = true;
			skinVertex.weightCount = weightCount;
			for (int weightIndex = 0; weightIndex < weightCount; ++weightIndex)
			{
				const SGeoWeightData *pWeightData = pWeightDatas + weightIndex;
				skinVertex.boneIdx[weightIndex] = boneIndices[weightIndex];
				skinVertex.mirrorAxis[weightIndex] = (isMirroring) ? pWeightData->mMirrorAxis : 0;
				skinVertex.boneWt[weightIndex] = pWeightValues[weightIndex];
				skinVertex.pos[weightIndex][0] = pVertData[weightIndex + 0 * weightCount];
				skinVertex.pos[weightIndex][1] = pVertData[weightIndex + 1 * weightCount];
				skinVertex.pos[weightIndex][2] = pVertData[weightIndex + 2 * weightCount];
				if (pNrmVertData)
				{
					skinVertex.nrm[weightIndex][0] = pNrmVertData[weightIndex + 0 * weightCount];
					skinVertex.nrm[weightIndex][1] = pNrmVertData[weightIndex + 1 * weightCount];
					skinVertex.nrm[weightIndex][2] = pNrmVertData[weightIndex + 2 * weightCount];
				}
			}
			pRapi->rpgSetPendingSkinData(&skinVertex);

			//now that we've pulled the weights out and fed them in, we need to put our verts in model space.
			RichVec4 transformedPos;
			RichVec3 transformedNrm;
			for (int weightIndex = 0; weightIndex < weightCount; ++weightIndex)
			{
				const RichMat44 &skinMat = skinMats[weightIndex];
				const RichVec4 pos(
					pVertData[weightIndex + 0 * weightCount],
					pVertData[weightIndex + 1 * weightCount],
					pVertData[weightIndex + 2 * weightCount],
					//this is certainly one of the most terrible ways you can possibly do weighted transforms. we're effectively weighting the
					//matrix translation with each transform. this is why the per-weight positions are needed. so, rather than having to actually
					//weight each translation, we can just stuff the weight into the w for our 4x4 transform.
					pWeightValues[weightIndex]
				);

				transformedPos += skinMat.TransformVec4(pos);

				if (pNrmVertData)
				{
					const RichVec3 nrm(
						pNrmVertData[weightIndex + 0 * weightCount],
						pNrmVertData[weightIndex + 1 * weightCount],
						pNrmVertData[weightIndex + 2 * weightCount]
					);
					//this is inconsistent with the position transform, but appears to be in line with the game itself.
					transformedNrm += skinMat.TransformNormal(nrm) * pWeightValues[weightIndex];
				}
			}

			if (pNrmVertData)
			{
				transformedNrm.Normalize();
				pRapi->rpgVertNormal3f(transformedNrm.v);
			}
			else
			{
				pRapi->rpgVertNormal3f(NULL);
			}
			pRapi->rpgVertex3f(transformedPos.v);
		}
	}

	static void UpdateDrawState(noeRAPI_t *pRapi, SGeoDrawState &drawState)
	{
		if (drawState.mEnableReflection && (!gpFF11Opts || !gpFF11Opts->noShinyMaterials))
		{
			char materialName[CFFXITextureHandler::skTexNameLength + CFFXITextureHandler::skMaterialNamePad];
			sprintf_s(materialName, "%s%s", drawState.mMaterialName, CFFXITextureHandler::skpShinySuffix);
			pRapi->rpgSetMaterial(materialName);
		}
		else
		{
			pRapi->rpgSetMaterial(drawState.mMaterialName);
		}
	}

	TGeoHeaderList mGeoHeaderList;
};

const RichMat44 CFFXIGeoHandler::skMirrorTransforms[3] =
{
	RichMat44(
		-RichVec4(g_identityMatrix4x4.c1),
		 RichVec4(g_identityMatrix4x4.c2),
		 RichVec4(g_identityMatrix4x4.c3),
		 RichVec4(g_identityMatrix4x4.c4)
	),
	RichMat44(
		 RichVec4(g_identityMatrix4x4.c1),
		-RichVec4(g_identityMatrix4x4.c2),
		 RichVec4(g_identityMatrix4x4.c3),
		 RichVec4(g_identityMatrix4x4.c4)
	),
	RichMat44(
		 RichVec4(g_identityMatrix4x4.c1),
		 RichVec4(g_identityMatrix4x4.c2),
		-RichVec4(g_identityMatrix4x4.c3),
		 RichVec4(g_identityMatrix4x4.c4)
	)
};

const int CFFXIGeoHandler::skTriCWIdx[3] = { 0, 1, 2 };

const int CFFXIGeoHandler::skTriCCWIdx[3] = { 2, 1, 0 };

//========================================================================================

#pragma pack(pop)
