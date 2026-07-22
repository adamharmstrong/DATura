class CFFXIMapHandler : public CFFXIChunkHandler
{
public:
	struct SMapHeader
	{
		unsigned char mHeaderData[4];
		unsigned int mObjectCount : 24;
		unsigned int mUnknown1 : 8;
		unsigned int mUnknown2[6];
	};

	static const int skObjectNameLength = 16;
	struct SMapObject
	{
		char mObjectName[skObjectNameLength];

		RichVec3 mTrans;
		float mRawAngles[3];
		RichVec3 mScale;

		RichVec4 mVec;
		int mData2[8];
	};

	struct SInterpretedMapObject
	{
		explicit SInterpretedMapObject(const SMapObject *pMapObject, const int objectIndex)
		{
			mObjectIndex = objectIndex;
			mTrans = pMapObject->mTrans;
			memcpy(mRawAngles, pMapObject->mRawAngles, sizeof(mRawAngles));
			mScale = pMapObject->mScale;
			mTransform = RichAngles(pMapObject->mRawAngles, true).ToMat43_XYZ();
			mTransform[3] = pMapObject->mTrans;
			mTransform[0] *= pMapObject->mScale[0];
			mTransform[1] *= pMapObject->mScale[1];
			mTransform[2] *= pMapObject->mScale[2];
			mBackwardWinding = (pMapObject->mScale[0] * pMapObject->mScale[1] * pMapObject->mScale[2]) < 0.0f;
			memcpy(mObjectName, pMapObject->mObjectName, skObjectNameLength);
			mObjectFlags[0] = pMapObject->mData2[0];
			mObjectFlags[1] = pMapObject->mData2[4];
			memcpy(mData2, pMapObject->mData2, sizeof(mData2));
			mVec = pMapObject->mVec;
		};

		char mObjectName[skObjectNameLength];
		int mObjectIndex;
		RichVec3 mTrans;
		float mRawAngles[3];
		RichVec3 mScale;

		RichMat43 mTransform;
		bool mBackwardWinding;

		unsigned int mObjectFlags[2];
		unsigned int mData2[8];
		RichVec4 mVec;
	};
	typedef std::vector<SInterpretedMapObject> TMapObjectList;

	CFFXIMapHandler()
		: CFFXIChunkHandler(CFFXIDat::skChunkType_Map)
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
		noeRAPI_t *pRapi = dat.GetRAPI();
		unsigned char *pDecrypted = (unsigned char *)pRapi->Noesis_UnpooledAlloc(dataSize);
		memcpy(pDecrypted, pChunkData, dataSize);
		FF11Decrypt::DecryptChunk(pDecrypted, CFFXIDat::skChunkType_Map, dataSize);

		if (gpFF11Opts && gpFF11Opts->collectCollision)
			CollectOfficialCollisionMesh(pDecrypted, dataSize);

		const int objectStartOfs = 0x20;
		if (dataSize < objectStartOfs)
		{
			pRapi->Noesis_UnpooledFree(pDecrypted);
			return true;
		}

		const int objectEndOfs = ReadInt32LE(pDecrypted, dataSize, 0x14);
		const int maxObjectCount = RangeIsValid(objectStartOfs, 0, dataSize) ?
			((dataSize - objectStartOfs) / (int)sizeof(SMapObject)) : 0;
		int objectCount = 0;
		if (objectEndOfs >= objectStartOfs && objectEndOfs <= dataSize)
			objectCount = (objectEndOfs - objectStartOfs) / (int)sizeof(SMapObject);

		const SMapHeader *pMapHdr = (const SMapHeader *)pDecrypted;
		if (objectCount <= 0 && pMapHdr->mObjectCount <= (unsigned int)maxObjectCount)
			objectCount = (int)pMapHdr->mObjectCount;
		if (objectCount < 0)
			objectCount = 0;
		if (objectCount > maxObjectCount)
			objectCount = maxObjectCount;

		memset(&gFF11LastMapHeader, 0, sizeof(gFF11LastMapHeader));
		gFF11LastMapHeader.valid = true;
		memcpy(gFF11LastMapHeader.headerData, pMapHdr->mHeaderData, sizeof(gFF11LastMapHeader.headerData));
		gFF11LastMapHeader.objectCount24 = pMapHdr->mObjectCount;
		gFF11LastMapHeader.unknown1 = pMapHdr->mUnknown1;
		memcpy(gFF11LastMapHeader.unknown2, pMapHdr->mUnknown2, sizeof(gFF11LastMapHeader.unknown2));
		gFF11LastMapHeader.objectTableOffset = objectStartOfs;
		gFF11LastMapHeader.objectTableEndOffset = objectEndOfs;
		gFF11LastMapHeader.parsedObjectCount = objectCount;
		gFF11LastMapHeader.trailingDataOffset = objectStartOfs + objectCount * (int)sizeof(SMapObject);
		gFF11LastMapHeader.trailingDataSize =
			(gFF11LastMapHeader.trailingDataOffset <= dataSize) ? dataSize - gFF11LastMapHeader.trailingDataOffset : 0;

		const SMapObject *pMapObjects = (const SMapObject *)(pDecrypted + objectStartOfs);
		mMapObjects.reserve(mMapObjects.size() + objectCount);
		for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex)
		{
			const SMapObject *pMapObject = pMapObjects + objectIndex;
			mMapObjects.push_back(SInterpretedMapObject(pMapObject, objectIndex));
		}

		//data following map objects isn't mapped out, but likely pertains to visibility and partitioning

		pRapi->Noesis_UnpooledFree(pDecrypted);
		return true;
	}

	TMapObjectList &MapObjects() { return mMapObjects; }

protected:
	static int ReadInt32LE(const unsigned char *pData, const int dataSize, const int ofs, const int fallback = 0)
	{
		if (ofs < 0 || ofs + 4 > dataSize)
			return fallback;
		int value = 0;
		memcpy(&value, pData + ofs, sizeof(value));
		return value;
	}

	static short ReadInt16LE(const unsigned char *pData, const int dataSize, const int ofs, const short fallback = 0)
	{
		if (ofs < 0 || ofs + 2 > dataSize)
			return fallback;
		short value = 0;
		memcpy(&value, pData + ofs, sizeof(value));
		return value;
	}

	static unsigned short ReadUInt16LE(const unsigned char *pData, const int dataSize, const int ofs, const unsigned short fallback = 0)
	{
		if (ofs < 0 || ofs + 2 > dataSize)
			return fallback;
		unsigned short value = 0;
		memcpy(&value, pData + ofs, sizeof(value));
		return value;
	}

	static float ReadFloatLE(const unsigned char *pData, const int dataSize, const int ofs, const float fallback = 0.0f)
	{
		if (ofs < 0 || ofs + 4 > dataSize)
			return fallback;
		float value = 0.0f;
		memcpy(&value, pData + ofs, sizeof(value));
		return value;
	}

	static bool RangeIsValid(const int ofs, const int bytes, const int dataSize)
	{
		return ofs >= 0 && bytes >= 0 && ofs <= dataSize && bytes <= dataSize - ofs;
	}

	static void CollectOfficialCollisionMesh(const unsigned char *pData, const int dataSize)
	{
		if (dataSize < 0x20)
			return;

		const int meshOfs = ReadInt32LE(pData, dataSize, 0x08);
		if (!RangeIsValid(meshOfs, 0x20, dataSize))
			return;

		const int gridWidth = 10 * (int)pData[0x0C];
		const int gridHeight = 10 * (int)pData[0x0D];
		const int meshGridLists = ReadInt32LE(pData, dataSize, meshOfs + 0x0C);
		const int gridOfs = ReadInt32LE(pData, dataSize, meshOfs + 0x10);
		if (gridWidth <= 0 || gridHeight <= 0 || gridWidth > 4096 || gridHeight > 4096)
			return;
		if (!RangeIsValid(gridOfs, gridWidth * gridHeight * 4, dataSize))
			return;
		if (meshGridLists < 0 || meshGridLists >= dataSize)
			return;

		for (int gy = 0; gy < gridHeight; ++gy)
		{
			for (int gx = 0; gx < gridWidth; ++gx)
			{
				const int entryOfs = ReadInt32LE(pData, dataSize, gridOfs + (gy * gridWidth + gx) * 4);
				if (entryOfs != 0)
					CollectOfficialCollisionGridEntry(pData, dataSize, entryOfs, gx, gy);
			}
		}
	}

	static void CollectOfficialCollisionGridEntry(const unsigned char *pData, const int dataSize, int entryOfs,
												 const int gridX, const int gridY)
	{
		if (!RangeIsValid(entryOfs, 8, dataSize))
			return;

		// First int stores bucket position/flags. The remaining entries are
		// {transform offset, geometry offset} pairs, terminated by zero.
		entryOfs += 4;
		for (int pairCount = 0; pairCount < 4096; ++pairCount)
		{
			if (!RangeIsValid(entryOfs, 4, dataSize))
				return;
			const int transformOfs = ReadInt32LE(pData, dataSize, entryOfs);
			entryOfs += 4;
			if (transformOfs == 0)
				return;
			if (!RangeIsValid(entryOfs, 4, dataSize))
				return;
			const int geometryOfs = ReadInt32LE(pData, dataSize, entryOfs);
			entryOfs += 4;
			if (geometryOfs != 0)
				CollectOfficialCollisionGridMesh(pData, dataSize, transformOfs, geometryOfs, gridX, gridY);
		}
	}

	static void CollectOfficialCollisionGridMesh(const unsigned char *pData, const int dataSize,
												const int transformOfs, const int geometryOfs,
												const int gridX, const int gridY)
	{
		if (!RangeIsValid(transformOfs, 16 * 4, dataSize) || !RangeIsValid(geometryOfs, 0x10, dataSize))
			return;

		float m[16];
		for (int i = 0; i < 16; ++i)
			m[i] = ReadFloatLE(pData, dataSize, transformOfs + i * 4);

		const int verticesOfs = ReadInt32LE(pData, dataSize, geometryOfs + 0x00);
		const int normalsOfs = ReadInt32LE(pData, dataSize, geometryOfs + 0x04);
		const int trisOfs = ReadInt32LE(pData, dataSize, geometryOfs + 0x08);
		const int triCount = (int)ReadInt16LE(pData, dataSize, geometryOfs + 0x0C);
		if (triCount <= 0 || triCount > 65535)
			return;
		if (!RangeIsValid(verticesOfs, 0, dataSize) || !RangeIsValid(normalsOfs, 0, dataSize) ||
			!RangeIsValid(trisOfs, triCount * 8, dataSize) ||
			normalsOfs < verticesOfs || trisOfs < normalsOfs)
			return;

		const int vertexCount = (normalsOfs - verticesOfs) / 12;
		if (vertexCount <= 0 || vertexCount > 0x3FFF)
			return;

		ff11CollisionMeshDebug_t meshDebug = {};
		meshDebug.triStart = (int)gFF11LastCollisionTriangles.size();
		meshDebug.gridX = gridX;
		meshDebug.gridY = gridY;
		meshDebug.transformOfs = transformOfs;
		meshDebug.geometryOfs = geometryOfs;
		sprintf_s(meshDebug.displayName, "Grid %03d,%03d mesh %05d",
				  gridX, gridY, (int)gFF11LastCollisionMeshes.size());
		bool haveMeshBounds = false;

		for (int triIndex = 0; triIndex < triCount; ++triIndex)
		{
			const int triOfs = trisOfs + triIndex * 8;
			const int idx0 = (int)(ReadUInt16LE(pData, dataSize, triOfs + 0) & 0x3FFF);
			const int idx1 = (int)(ReadUInt16LE(pData, dataSize, triOfs + 2) & 0x3FFF);
			const int idx2 = (int)(ReadUInt16LE(pData, dataSize, triOfs + 4) & 0x3FFF);
			if (idx0 >= vertexCount || idx1 >= vertexCount || idx2 >= vertexCount)
				continue;

			ff11CollisionTriangle_t outTri = {};
			const int indices[3] = { idx0, idx1, idx2 };
			for (int v = 0; v < 3; ++v)
			{
				const int vertOfs = verticesOfs + indices[v] * 12;
				const float x = ReadFloatLE(pData, dataSize, vertOfs + 0);
				const float y = ReadFloatLE(pData, dataSize, vertOfs + 4);
				const float z = ReadFloatLE(pData, dataSize, vertOfs + 8);
				outTri.p[v][0] = m[0] * x + m[4] * y + m[8]  * z + m[12];
				outTri.p[v][1] = m[1] * x + m[5] * y + m[9]  * z + m[13];
				outTri.p[v][2] = m[2] * x + m[6] * y + m[10] * z + m[14];
				if (!haveMeshBounds)
				{
					meshDebug.boundsMin[0] = meshDebug.boundsMax[0] = outTri.p[v][0];
					meshDebug.boundsMin[1] = meshDebug.boundsMax[1] = outTri.p[v][1];
					meshDebug.boundsMin[2] = meshDebug.boundsMax[2] = outTri.p[v][2];
					haveMeshBounds = true;
				}
				else
				{
					for (int axis = 0; axis < 3; ++axis)
					{
						if (outTri.p[v][axis] < meshDebug.boundsMin[axis]) meshDebug.boundsMin[axis] = outTri.p[v][axis];
						if (outTri.p[v][axis] > meshDebug.boundsMax[axis]) meshDebug.boundsMax[axis] = outTri.p[v][axis];
					}
				}
			}
			gFF11LastCollisionTriangles.push_back(outTri);
		}
		meshDebug.triCount = (int)gFF11LastCollisionTriangles.size() - meshDebug.triStart;
		if (meshDebug.triCount > 0)
			gFF11LastCollisionMeshes.push_back(meshDebug);
	}

	TMapObjectList mMapObjects;
};

//========================================================================================

static bool FF11_MapGeoEnvNameStartsWith(const char *name, const char *prefix)
{
	for (int i = 0; prefix[i] != 0; ++i)
	{
		if (i >= CFFXIMapHandler::skObjectNameLength || name[i] != prefix[i])
			return false;
	}
	return true;
}

static bool FF11_MapGeoNameContains(const char *name, const char *token)
{
	char objectName[CFFXIMapHandler::skObjectNameLength + 1];
	memcpy(objectName, name, CFFXIMapHandler::skObjectNameLength);
	objectName[CFFXIMapHandler::skObjectNameLength] = 0;
	for (int i = 0; objectName[i] != 0; ++i)
		objectName[i] = (char)tolower((unsigned char)objectName[i]);

	char tokenLower[32];
	const int tokenMax = (int)sizeof(tokenLower) - 1;
	int tokenLen = 0;
	for (; token[tokenLen] != 0 && tokenLen < tokenMax; ++tokenLen)
		tokenLower[tokenLen] = (char)tolower((unsigned char)token[tokenLen]);
	tokenLower[tokenLen] = 0;

	return strstr(objectName, tokenLower) != NULL;
}

static bool FF11_MapGeoNameUsesHardAlpha(const char *name)
{
	// Zone mesh names beginning with '_' are the retail cutout convention.
	if (name[0] == '_')
		return true;

	static const char *skHardAlphaNames[] =
	{
		"_ami",
		"_con_hana_",
		"_himono",
		"_kusa_",
		"_sel_w",
		"_tyo",
		"_umisaku-ami",
		"_wakame",
		"ami",
		"cry_line",
		"dust",
		"himono",
		"hit_cry",
		"_mixmdl_000",
		"_mixmdl_001",
		"_mixmdl_002",
		"_mixmdl_003",
		"_mixmdl_004",
		"_mixmdl_005",
		"_mixmdl_006",
		"_mixmdl_007",
		"_mixmdl_008",
		"_mixmdl_009",
		"_mixmdl_010",
		"mixmdl_0008",
		"mixmdl_0009",
		"mixmdl_0010",
		"sakana",
		"star"
	};

	for (int i = 0; i < (int)(sizeof(skHardAlphaNames) / sizeof(skHardAlphaNames[0])); ++i)
	{
		if (FF11_MapGeoEnvNameStartsWith(name, skHardAlphaNames[i]))
			return true;
	}

	if (FF11_MapGeoNameContains(name, "hana") ||
		FF11_MapGeoNameContains(name, "kusa"))
	{
		return true;
	}
	return false;
}

static bool FF11_MapGeoNameLooksLikeCollision(const char *name)
{
	static const char *skCollisionTokens[] =
	{
		"col",
		"wall",
		"floor",
		"walk",
		"hit",
		"bound"
	};

	char objectName[CFFXIMapHandler::skObjectNameLength + 1];
	memcpy(objectName, name, CFFXIMapHandler::skObjectNameLength);
	objectName[CFFXIMapHandler::skObjectNameLength] = 0;
	for (int i = 0; objectName[i] != 0; ++i)
		objectName[i] = (char)tolower((unsigned char)objectName[i]);

	for (int i = 0; i < (int)(sizeof(skCollisionTokens) / sizeof(skCollisionTokens[0])); ++i)
	{
		if (strstr(objectName, skCollisionTokens[i]))
			return true;
	}
	return false;
}

class CFFXIMapGeoHandler : public CFFXIChunkHandler
{
public:
	struct SMapGeoHeader
	{
		unsigned char mHeaderData[4];
		unsigned int mUnknown1;
		char mUnknownName[8];
		char mObjectName[CFFXIMapHandler::skObjectNameLength];
	};

	struct SMapGeoData
	{
		explicit SMapGeoData(const SMapGeoHeader *pMapGeoHdr, noeRAPI_t *pAllocatingInstance, const int dataSize)
			: mpMapGeoHdr(pMapGeoHdr)
			, mpAllocatingInstance(pAllocatingInstance)
			, mDataSize(dataSize)
		{
		}

		const SMapGeoHeader *mpMapGeoHdr;
		noeRAPI_t *mpAllocatingInstance;
		int mDataSize;
	};
	typedef std::vector<SMapGeoData> TMapGeoList;

	struct SDrawHeader
	{
		int mSegCount;
		float mBounds[6];
		int mFlag; //typically 64 for super header, varying for sub
	};

	// XIM's zone-mesh state decoder: 0x2000 disables the normal back-face
	// cull, while 0x8000 selects the translucent, depth-biased layer pass.
	static const int skMapGeoFlag_DisableBackFaceCull = 0x2000;
	static const int skMapGeoFlag_AlphaBlend = 0x8000;

	static const int skDefaultVertColorFixShift = 0;
	static const int skDefaultVertAlphaFixShift = 0;

	CFFXIMapGeoHandler()
		: CFFXIChunkHandler(CFFXIDat::skChunkType_MapGeo)
		, mMapGeoHash(CFFXIMapHandler::skObjectNameLength)
	{
	}

	virtual ~CFFXIMapGeoHandler()
	{
		for (TMapGeoList::iterator it = mMapGeoList.begin(); it != mMapGeoList.end(); ++it)
		{
			SMapGeoData &geoData = *it;
			geoData.mpAllocatingInstance->Noesis_UnpooledFree((void *)geoData.mpMapGeoHdr);
		}
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
		noeRAPI_t *pRapi = dat.GetRAPI();
		unsigned char *pDecrypted = (unsigned char *)pRapi->Noesis_UnpooledAlloc(dataSize);
		memcpy(pDecrypted, pChunkData, dataSize);
		FF11Decrypt::DecryptChunk(pDecrypted, CFFXIDat::skChunkType_MapGeo, dataSize);

		const SMapGeoHeader *pMapGeoHdr = (const SMapGeoHeader *)pDecrypted;
		const int index = mMapGeoList.size();
		mMapGeoList.push_back(SMapGeoData(pMapGeoHdr, pRapi, dataSize));
		mMapGeoHash.FindOrAddResource(pMapGeoHdr->mObjectName, index, true);
		return true;
	}

	void RenderMapObjectGeo(noeRAPI_t *pRapi, const int index, const RichMat43 &transform, const bool backwardWinding,
							const CFFXIMapHandler::SInterpretedMapObject *pMapObject, const bool emitRender = true)
	{
		SMapGeoData &geoData = mMapGeoList[index];
		const SMapGeoHeader *pMapGeoHdr = geoData.mpMapGeoHdr;
		const unsigned char *pDrawData = (const unsigned char *)pMapGeoHdr;
		const int endDrawOfs = geoData.mDataSize - sizeof(SDrawHeader);
		const unsigned int objectFlags0 = (pMapObject) ? pMapObject->mObjectFlags[0] : 0;
		const unsigned int objectFlags1 = (pMapObject) ? pMapObject->mObjectFlags[1] : 0;

		char nameString[64];
		if (emitRender && pMapObject)
		{
			char objectName[CFFXIMapHandler::skObjectNameLength + 1];
			memcpy(objectName, pMapObject->mObjectName, CFFXIMapHandler::skObjectNameLength);
			objectName[CFFXIMapHandler::skObjectNameLength] = 0;
			sprintf_s(nameString, "%03d: %s", pMapObject->mObjectIndex, objectName);
		}
		else if (emitRender)
		{
			char objectName[CFFXIMapHandler::skObjectNameLength + 1];
			memcpy(objectName, pMapGeoHdr->mObjectName, CFFXIMapHandler::skObjectNameLength);
			objectName[CFFXIMapHandler::skObjectNameLength] = 0;
			sprintf_s(nameString, "env: %s", objectName);
		}
		if (emitRender)
		{
			pRapi->rpgSetName(nameString);
			pRapi->rpgSetTransform(const_cast<modelMatrix_t *>(&transform.m));
		}

		char matName[CFFXITextureHandler::skTexNameLength + CFFXITextureHandler::skMaterialNamePad];

		int drawOfs = sizeof(SMapGeoHeader);
		while (drawOfs <= endDrawOfs)
		{
			const SDrawHeader *pSuperHeader = get_and_incr_offset<SDrawHeader>(pDrawData, drawOfs);
			for (int superIndex = 0; superIndex < pSuperHeader->mSegCount && drawOfs <= endDrawOfs; ++superIndex)
			{
				const SDrawHeader *pSubHeader = get_and_incr_offset<SDrawHeader>(pDrawData, drawOfs);
				for (int subIndex = 0; subIndex < pSubHeader->mSegCount && drawOfs <= endDrawOfs; ++subIndex)
				{
					NoeAssert((drawOfs & 3) == 0);
					const int batchDrawOfs = drawOfs;
					const char *pMatName = get_and_incr_offset<char>(pDrawData, drawOfs, CFFXITextureHandler::skTexNameLength);

					const unsigned short vertCount = *get_and_incr_offset<unsigned short>(pDrawData, drawOfs);
					const unsigned short blendFlags = *get_and_incr_offset<unsigned short>(pDrawData, drawOfs);
					const int vertStride = (pSubHeader->mFlag == 0) ? 48 : 36;
					const unsigned char *pVertData = get_and_incr_offset<unsigned char>(pDrawData, drawOfs, vertStride * vertCount);
					const unsigned short indexCount = *get_and_incr_offset<unsigned short>(pDrawData, drawOfs);
					const unsigned short flags2 = *get_and_incr_offset<unsigned short>(pDrawData, drawOfs);
					const unsigned short *pIndexData = get_and_incr_offset<unsigned short>(pDrawData, drawOfs, indexCount);
					align_offset(drawOfs, 4);

					ff11MapGeoDrawBatchDebug_t batchDebug = {};
					memcpy(batchDebug.objectName, pMapGeoHdr->mObjectName, CFFXIMapHandler::skObjectNameLength);
					batchDebug.objectName[CFFXIMapHandler::skObjectNameLength] = 0;
					memcpy(batchDebug.mapGeoUnknownName, pMapGeoHdr->mUnknownName, sizeof(pMapGeoHdr->mUnknownName));
					batchDebug.mapGeoUnknownName[sizeof(pMapGeoHdr->mUnknownName)] = 0;
					memcpy(batchDebug.materialName, pMatName, CFFXITextureHandler::skTexNameLength);
					batchDebug.materialName[CFFXITextureHandler::skTexNameLength] = 0;
					batchDebug.mapGeoIndex = index;
					batchDebug.mapRecordIndex = pMapObject ? pMapObject->mObjectIndex : -1;
					memcpy(batchDebug.mapGeoHeaderData, pMapGeoHdr->mHeaderData, sizeof(batchDebug.mapGeoHeaderData));
					batchDebug.mapGeoUnknown1 = pMapGeoHdr->mUnknown1;
					batchDebug.superIndex = superIndex;
					batchDebug.subIndex = subIndex;
					batchDebug.drawOffset = batchDrawOfs;
					batchDebug.vertexCount = vertCount;
					batchDebug.indexCount = indexCount;
					batchDebug.vertexStride = vertStride;
					batchDebug.indexMode = (pSubHeader->mFlag == 0) ? 0 : 1;
					batchDebug.objectFlags[0] = objectFlags0;
					batchDebug.objectFlags[1] = objectFlags1;
					batchDebug.blendFlags = blendFlags;
					batchDebug.flags2 = flags2;
					batchDebug.superFlag = pSuperHeader->mFlag;
					batchDebug.subFlag = pSubHeader->mFlag;
					batchDebug.galkaReeveBlendMultiplier = (blendFlags & 0xF000) >> 12;
					batchDebug.galkaReeveUseAlpha = (blendFlags & skMapGeoFlag_AlphaBlend) != 0;
					batchDebug.galkaReeveWouldAlphaBlend = batchDebug.galkaReeveUseAlpha;
					memcpy(batchDebug.superBounds, pSuperHeader->mBounds, sizeof(batchDebug.superBounds));
					memcpy(batchDebug.subBounds, pSubHeader->mBounds, sizeof(batchDebug.subBounds));
					sprintf_s(batchDebug.displayName, "%03d:%s S%d/%d %s",
							  index, batchDebug.objectName, superIndex, subIndex,
							  batchDebug.indexMode == 0 ? "tri list" : "tri strip");

#if defined(_DEBUG_MAP_MESHES)
					//segment things up to give us some more info about each mesh in debug
					char nameString[CFFXIMapHandler::skObjectNameLength + 128];
					memcpy(nameString, pMapGeoHdr->mObjectName, CFFXIMapHandler::skObjectNameLength);
					NoeAssert(pMapObject);
					sprintf_s(&nameString[CFFXIMapHandler::skObjectNameLength], 128, "_fl%08x_fb%08x_ps%08x_bl%08x",
								objectFlags0, objectFlags1, pSubHeader->mFlag, blendFlags);
					sprintf_s(&nameString[CFFXIMapHandler::skObjectNameLength], 128, "_fl%08x_x%.02fy%.02fz%.02fw%.02f",
								objectFlags0, pMapObject->mVec[0], pMapObject->mVec[1], pMapObject->mVec[2], pMapObject->mVec[3]);
					pRapi->rpgSetName(nameString);
#endif

					memcpy(matName, pMatName, CFFXITextureHandler::skTexNameLength);
					matName[CFFXITextureHandler::skTexNameLength] = 0;
					const bool explicitObjectTransparency = (objectFlags0 & 0x01000000) != 0;
					const bool hardAlpha = (explicitObjectTransparency && vertStride == 48) ||
						FF11_MapGeoNameUsesHardAlpha(pMapGeoHdr->mObjectName);
					const bool shouldBlend = !hardAlpha && batchDebug.galkaReeveWouldAlphaBlend;
					const bool backFaceCull = (blendFlags & skMapGeoFlag_DisableBackFaceCull) == 0;
					batchDebug.daturaHardAlpha = hardAlpha;
					batchDebug.daturaSoftBlend = shouldBlend;
					strcpy_s(batchDebug.daturaRenderMode, shouldBlend ? "soft blend" : (hardAlpha ? "hard alpha" : "no blend"));
					if (hardAlpha)
						strcpy_s(batchDebug.daturaRenderReason, "FFXI zone cutout mesh: DXT alpha mask with the retail threshold");
					else if (shouldBlend)
						strcpy_s(batchDebug.daturaRenderReason, "FFXI zone alpha layer: depth-biased blend with depth writes disabled");
					else
						strcpy_s(batchDebug.daturaRenderReason, "FFXI zone opaque base layer");
					const char *pBlendSuffix = (shouldBlend) ?
						(backFaceCull ? CFFXITextureHandler::skpSoftBlendCullBackSuffix : CFFXITextureHandler::skpSoftBlendSuffix) :
						(hardAlpha ?
							(backFaceCull ? CFFXITextureHandler::skpHardAlphaCullBackSuffix : CFFXITextureHandler::skpHardAlphaSuffix) :
							(backFaceCull ? CFFXITextureHandler::skpNoBlendCullBackSuffix : CFFXITextureHandler::skpNoBlendSuffix));
					const size_t matNameLen = strlen(matName);
					strcpy_s(&matName[matNameLen],
						sizeof(matName) - matNameLen,
						pBlendSuffix);
					strcpy_s(batchDebug.materialName, matName);
					if (emitRender)
						gFF11LastMapGeoDrawBatches.push_back(batchDebug);
					if (emitRender)
						pRapi->rpgSetMaterial(matName);

					if (vertCount == 0 || indexCount < 3)
					{
						if (emitRender)
							pRapi->LogOutput("WARNING: Unexpected vert/index count.\n");
						break;
					}
					else if (drawOfs > geoData.mDataSize)
					{
						if (emitRender)
							pRapi->LogOutput("WARNING: Ran off end of MapGeo.\n");
						break;
					}

					// Preserve each 0x8000 draw record as its own transparent layer.
					// Merging same-material records breaks ordering for Cermet Crag
					// overlays and other intersecting zone details.
					if (emitRender && shouldBlend)
						pRapi->rpgForceNextSubmesh();

					int posOfs, nrmOfs, clrOfs, uvOfs;
					switch (vertStride)
					{
					case 48:
						posOfs = 0;
						nrmOfs = 24;
						clrOfs = 36;
						uvOfs = 40;
						break;
					default:
						NoeAssert(vertStride == 36);
						posOfs = 0;
						nrmOfs = 12;
						clrOfs = 24;
						uvOfs = 28;
						break;
					}

					const int colorShift = (gpFF11Opts && gpFF11Opts->explicitVertColorShift) ?
											gpFF11Opts->fixVertColorShift : skDefaultVertColorFixShift;
					int alphaShift = (gpFF11Opts && gpFF11Opts->explicitVertAlphaShift) ?
											gpFF11Opts->fixVertAlphaShift : skDefaultVertAlphaFixShift;

					const unsigned int debugSeedBase = 0;

					if (pSubHeader->mFlag == 0)
					{
						//tri list
						const int *pTriWindIdx = (backwardWinding) ? CFFXIGeoHandler::skTriCCWIdx : CFFXIGeoHandler::skTriCWIdx;
						if (emitRender)
							pRapi->rpgBegin(RPGEO_TRIANGLE);
						for (int index = 0; index <= indexCount - 3; index += 3)
						{
							bool validTri = true;
							for (int triIndex = 0; triIndex < 3; ++triIndex)
							{
								const int rawIdx = pIndexData[index + pTriWindIdx[triIndex]];
								if (rawIdx < 0 || rawIdx >= (int)vertCount)
								{
									validTri = false;
									break;
								}
							}
							if (!validTri)
								continue;
							if (!MapTriangleLooksValid(pVertData, vertStride, posOfs,
													   pIndexData[index + pTriWindIdx[0]],
													   pIndexData[index + pTriWindIdx[1]],
													   pIndexData[index + pTriWindIdx[2]],
													   transform))
								continue;
							CollectCollisionTriangle(pVertData, vertStride, posOfs,
								pIndexData[index + pTriWindIdx[0]],
								pIndexData[index + pTriWindIdx[1]],
								pIndexData[index + pTriWindIdx[2]],
								transform, pMapGeoHdr->mObjectName, pMapObject);

							if (emitRender)
							{
								for (int triIndex = 0; triIndex < 3; ++triIndex)
								{
									const int rawIdx = pIndexData[index + pTriWindIdx[triIndex]];
									const unsigned char *pVert = pVertData + rawIdx * vertStride;
									PlotMapVertex(pRapi, pVert, posOfs, nrmOfs, clrOfs, uvOfs, colorShift, alphaShift, debugSeedBase);
								}
							}
						}
						if (emitRender)
							pRapi->rpgEnd();
					}
					else
					{
						//tri strip
						const int *pTriWindIdx = (backwardWinding) ? CFFXIGeoHandler::skTriCCWIdx : CFFXIGeoHandler::skTriCWIdx;
						if (emitRender)
							pRapi->rpgBegin(RPGEO_TRIANGLE);
						for (int index = 0; index <= indexCount - 3; ++index)
						{
							int triIdx[3] =
							{
								(int)pIndexData[index],
								(int)pIndexData[index + 1],
								(int)pIndexData[index + 2],
							};
							if ((index & 1) != 0)
								std::swap(triIdx[1], triIdx[2]);

							if (triIdx[0] == triIdx[1] || triIdx[1] == triIdx[2] || triIdx[0] == triIdx[2])
								continue;

							bool validTri = true;
							for (int triIndex = 0; triIndex < 3; ++triIndex)
							{
								if (triIdx[triIndex] < 0 || triIdx[triIndex] >= (int)vertCount)
								{
									validTri = false;
									break;
								}
							}
							if (!validTri)
								continue;
							if (!MapTriangleLooksValid(pVertData, vertStride, posOfs,
													   triIdx[0], triIdx[1], triIdx[2],
													   transform))
								continue;
							CollectCollisionTriangle(pVertData, vertStride, posOfs,
								triIdx[pTriWindIdx[0]], triIdx[pTriWindIdx[1]], triIdx[pTriWindIdx[2]],
								transform, pMapGeoHdr->mObjectName, pMapObject);

							if (emitRender)
							{
								for (int triIndex = 0; triIndex < 3; ++triIndex)
								{
									const int vertIndex = triIdx[pTriWindIdx[triIndex]];
									const unsigned char *pVert = pVertData + vertIndex * vertStride;
									PlotMapVertex(pRapi, pVert, posOfs, nrmOfs, clrOfs, uvOfs, colorShift, alphaShift, debugSeedBase);
								}
							}
						}
						if (emitRender)
							pRapi->rpgEnd();
					}
				}
			}
		}

		if (emitRender)
			pRapi->rpgSetTransform(NULL);
	}

	void RenderMapObjectGeoForMapObject(noeRAPI_t *pRapi, const CFFXIMapHandler::SInterpretedMapObject &mapObject)
	{
		const int index = mMapGeoHash.FindOrAddResource(mapObject.mObjectName, -1);
		if (index < 0)
		{
			char fallbackName[CFFXIMapHandler::skObjectNameLength];
			int fallbackIndex = -1;
			if (BuildHighDetailFallbackName(fallbackName, mapObject.mObjectName))
				fallbackIndex = mMapGeoHash.FindOrAddResource(fallbackName, -1);

			if (fallbackIndex < 0)
			{
				pRapi->LogOutput("WARNING: Could not find object in resource hash, skipping.\n");
			}
			else
			{
				char objectName[CFFXIMapHandler::skObjectNameLength + 1];
				char fallbackDisplayName[CFFXIMapHandler::skObjectNameLength + 1];
				memcpy(objectName, mapObject.mObjectName, CFFXIMapHandler::skObjectNameLength);
				memcpy(fallbackDisplayName, fallbackName, CFFXIMapHandler::skObjectNameLength);
				objectName[CFFXIMapHandler::skObjectNameLength] = 0;
				fallbackDisplayName[CFFXIMapHandler::skObjectNameLength] = 0;
				pRapi->LogOutput("WARNING: Using map geo fallback %s for missing object %s.\n",
					fallbackDisplayName, objectName);
				RenderMapObjectGeo(pRapi, fallbackIndex, mapObject.mTransform, mapObject.mBackwardWinding, &mapObject);
			}
		}
		else
		{
			RenderMapObjectGeo(pRapi, index, mapObject.mTransform, mapObject.mBackwardWinding, &mapObject);
		}
	}

	const TMapGeoList &GetMapGeoList() const { return mMapGeoList; }

protected:
	static bool BuildHighDetailFallbackName(char *dst, const char *src)
	{
		memcpy(dst, src, CFFXIMapHandler::skObjectNameLength);
		for (int i = CFFXIMapHandler::skObjectNameLength - 1; i > 0; --i)
		{
			if (dst[i] == 0 || dst[i] == ' ')
				continue;
			if (dst[i] == 'm' && dst[i - 1] == '_')
			{
				dst[i] = 'h';
				return true;
			}
			break;
		}
		return false;
	}

	static RichVec3 TransformMapPoint(const float *p, const RichMat43 &transform)
	{
		RichVec3 out;
		for (int c = 0; c < 3; ++c)
		{
			out[c] = p[0] * transform[0][c] +
					 p[1] * transform[1][c] +
					 p[2] * transform[2][c] +
					 transform[3][c];
		}
		return out;
	}

	static void CollectCollisionTriangle(const unsigned char *pVertData, const int vertStride, const int posOfs,
										const int idx0, const int idx1, const int idx2,
										const RichMat43 &transform, const char *objectName,
										const CFFXIMapHandler::SInterpretedMapObject *pMapObject)
	{
		if (!gpFF11Opts || !gpFF11Opts->collectCollision)
			return;
		if (!gFF11LastCollisionTriangles.empty())
			return;

		const bool placedGeometry = (pMapObject != NULL);
		const bool unreferencedCandidate =
			gpFF11Opts->collectCollisionUnreferenced &&
			FF11_MapGeoNameLooksLikeCollision(objectName);
		if (!placedGeometry && !unreferencedCandidate)
			return;

		ff11CollisionTriangle_t tri = {};
		const RichVec3 p0 = TransformMapPoint((const float *)(pVertData + idx0 * vertStride + posOfs), transform);
		const RichVec3 p1 = TransformMapPoint((const float *)(pVertData + idx1 * vertStride + posOfs), transform);
		const RichVec3 p2 = TransformMapPoint((const float *)(pVertData + idx2 * vertStride + posOfs), transform);
		for (int axis = 0; axis < 3; ++axis)
		{
			tri.p[0][axis] = p0[axis];
			tri.p[1][axis] = p1[axis];
			tri.p[2][axis] = p2[axis];
		}
		gFF11LastCollisionTriangles.push_back(tri);
	}

	static bool MapTriangleLooksValid(const unsigned char *pVertData, const int vertStride, const int posOfs,
									const int idx0, const int idx1, const int idx2,
									const RichMat43 &transform)
	{
		const RichVec3 p0 = TransformMapPoint((const float *)(pVertData + idx0 * vertStride + posOfs), transform);
		const RichVec3 p1 = TransformMapPoint((const float *)(pVertData + idx1 * vertStride + posOfs), transform);
		const RichVec3 p2 = TransformMapPoint((const float *)(pVertData + idx2 * vertStride + posOfs), transform);

		auto distSq = [](const RichVec3 &a, const RichVec3 &b)
		{
			const float dx = a[0] - b[0];
			const float dy = a[1] - b[1];
			const float dz = a[2] - b[2];
			return dx*dx + dy*dy + dz*dz;
		};

		const float maxEdge = 80.0f;
		const float maxEdgeSq = maxEdge * maxEdge;

		return distSq(p0, p1) <= maxEdgeSq &&
			   distSq(p1, p2) <= maxEdgeSq &&
			   distSq(p2, p0) <= maxEdgeSq;
	}

	void PlotMapVertex(noeRAPI_t *pRapi, const unsigned char *pVert, const int posOfs, const int nrmOfs, const int clrOfs, const int uvOfs,
						const int colorShift, const int alphaShift, const unsigned int debugSeedBase)
	{
		pRapi->rpgVertNormal3f((float *)(pVert + nrmOfs));

		if (!debugSeedBase)
		{
			pRapi->rpgVertUV2f((float *)(pVert + uvOfs), 0);
			if (!gpFF11Opts || !gpFF11Opts->noVertColors)
			{
				if (colorShift || alphaShift)
				{
					//fix the color/alpha range
					unsigned char clr[4];
					memcpy(clr, pVert + clrOfs, 4);
					clr[0] = (unsigned char)std::min<int>((int)clr[0] << colorShift, 255);
					clr[1] = (unsigned char)std::min<int>((int)clr[1] << colorShift, 255);
					clr[2] = (unsigned char)std::min<int>((int)clr[2] << colorShift, 255);
					clr[3] = (unsigned char)std::min<int>((int)clr[3] << alphaShift, 255);
					pRapi->rpgVertColor4ub(clr);
				}
				else
				{
					pRapi->rpgVertColor4ub(const_cast<unsigned char *>(pVert + clrOfs));
				}
			}
		}
		else
		{
			unsigned int debugSeed = debugSeedBase;
			pRapi->rpgVertUV2f(NULL, 0);
			const float debugColor[4] =
			{
				g_mfn->Math_RandFloatOnSeed(0.5f, 1.0f, debugSeed),
				g_mfn->Math_RandFloatOnSeed(0.5f, 1.0f, debugSeed),
				g_mfn->Math_RandFloatOnSeed(0.5f, 1.0f, debugSeed),
				1.0f
			};
			pRapi->rpgVertColor4f(const_cast<float *>(debugColor));
		}

		pRapi->rpgVertex3f((float *)(pVert + posOfs));
	}

	TMapGeoList mMapGeoList;
	CLocalResHash mMapGeoHash;
};

//========================================================================================
