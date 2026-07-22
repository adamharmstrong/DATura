class CFFXIEffectHandler : public CFFXIChunkHandler
{
public:
	struct SEffectMeshData
	{
		SEffectMeshData()
			: mpData(NULL)
			, mpAllocatingInstance(NULL)
			, mDataSize(0)
			, mVertexCount(0)
			, mPositionOffset(0)
			, mAttributeOffset(0)
			, mUVOffset(0)
			, mIndexOffset(0)
		{
			memset(mChunkName, 0, sizeof(mChunkName));
			memset(mMaterialName, 0, sizeof(mMaterialName));
		}

		unsigned char *mpData;
		noeRAPI_t *mpAllocatingInstance;
		int mDataSize;
		char mChunkName[8];
		char mMaterialName[CFFXITextureHandler::skTexNameLength + 1];
		unsigned short mHeaderWords[8];
		int mVertexCount;
		int mPositionOffset;
		int mAttributeOffset;
		int mUVOffset;
		int mIndexOffset;
	};
	typedef std::vector<SEffectMeshData> TEffectMeshList;

	explicit CFFXIEffectHandler(const int chunkType)
		: CFFXIChunkHandler(chunkType)
	{
	}

	virtual ~CFFXIEffectHandler()
	{
		for (TEffectMeshList::iterator it = mEffectMeshes.begin(); it != mEffectMeshes.end(); ++it)
		{
			SEffectMeshData &mesh = *it;
			if (mesh.mpData && mesh.mpAllocatingInstance)
				mesh.mpAllocatingInstance->Noesis_UnpooledFree(mesh.mpData);
		}
	}

	virtual CFFXIDat::EValidateChunkResult ValidateChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk,
															const unsigned char *pChunkData, const int dataSize) const
	{
		return (dataSize >= 0) ? CFFXIDat::kVCR_Supported : CFFXIDat::kVCR_Invalid;
	}

	virtual bool HandleChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk,
								const unsigned char *pChunkData, const int dataSize)
	{
		if (chunk.mType != CFFXIDat::skChunkType_EffectMesh)
			return true;

		SEffectMeshData mesh;
		if (!ParseEffectMeshHeader(pChunkData, dataSize, mesh))
			return true;

		noeRAPI_t *pRapi = dat.GetRAPI();
		mesh.mpData = (unsigned char *)pRapi->Noesis_UnpooledAlloc(dataSize);
		memcpy(mesh.mpData, pChunkData, dataSize);
		mesh.mpAllocatingInstance = pRapi;
		mesh.mDataSize = dataSize;
		memcpy(mesh.mChunkName, chunk.mName, 4);
		mesh.mChunkName[4] = 0;
		mEffectMeshes.push_back(mesh);
		return true;
	}

	void RenderEffectMeshes(noeRAPI_t *pRapi) const
	{
		for (TEffectMeshList::const_iterator it = mEffectMeshes.begin(); it != mEffectMeshes.end(); ++it)
			RenderEffectMesh(pRapi, *it);
	}

	const TEffectMeshList &EffectMeshes() const { return mEffectMeshes; }

protected:
	static unsigned short ReadU16(const unsigned char *pData, const int dataSize, const int ofs, const unsigned short fallback = 0)
	{
		if (ofs < 0 || ofs + 2 > dataSize)
			return fallback;
		unsigned short value = 0;
		memcpy(&value, pData + ofs, sizeof(value));
		return value;
	}

	static float ReadFloat(const unsigned char *pData, const int dataSize, const int ofs, const float fallback = 0.0f)
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

	static bool FloatIsSane(const float value)
	{
		return value == value && value >= -1000000.0f && value <= 1000000.0f;
	}

	static bool EffectPositionsAreSane(const unsigned char *pData, const int dataSize, const int ofs, const int vertexCount)
	{
		for (int i = 0; i < vertexCount; ++i)
		{
			const int vertexOfs = ofs + i * 16;
			if (!FloatIsSane(ReadFloat(pData, dataSize, vertexOfs + 0)) ||
				!FloatIsSane(ReadFloat(pData, dataSize, vertexOfs + 4)) ||
				!FloatIsSane(ReadFloat(pData, dataSize, vertexOfs + 8)))
			{
				return false;
			}
		}
		return true;
	}

	static bool ParseEffectMeshHeader(const unsigned char *pData, const int dataSize, SEffectMeshData &mesh)
	{
		if (dataSize < 0x20)
			return false;

		for (int i = 0; i < 8; ++i)
			mesh.mHeaderWords[i] = ReadU16(pData, dataSize, i * 2);

		mesh.mVertexCount = mesh.mHeaderWords[3];
		mesh.mIndexOffset = mesh.mHeaderWords[4];
		mesh.mPositionOffset = mesh.mHeaderWords[5];
		mesh.mAttributeOffset = mesh.mHeaderWords[6];
		mesh.mUVOffset = mesh.mHeaderWords[7];
		memcpy(mesh.mMaterialName, pData + 0x10, CFFXITextureHandler::skTexNameLength);
		mesh.mMaterialName[CFFXITextureHandler::skTexNameLength] = 0;

		if (mesh.mVertexCount <= 0 || mesh.mVertexCount > 65535)
			return false;
		if (!RangeIsValid(mesh.mPositionOffset, mesh.mVertexCount * 16, dataSize))
			return false;
		if (!RangeIsValid(mesh.mIndexOffset, mesh.mVertexCount * 6, dataSize))
			return false;
		if (mesh.mUVOffset != 0 && !RangeIsValid(mesh.mUVOffset, mesh.mVertexCount * 12, dataSize))
			return false;
		if (!EffectPositionsAreSane(pData, dataSize, mesh.mPositionOffset, mesh.mVertexCount))
			return false;

		return true;
	}

	static RichVec3 ReadPosition(const SEffectMeshData &mesh, const int vertexIndex)
	{
		const int ofs = mesh.mPositionOffset + vertexIndex * 16;
		return RichVec3(
			ReadFloat(mesh.mpData, mesh.mDataSize, ofs + 0),
			ReadFloat(mesh.mpData, mesh.mDataSize, ofs + 4),
			ReadFloat(mesh.mpData, mesh.mDataSize, ofs + 8));
	}

	static RichVec2 ReadUV(const SEffectMeshData &mesh, const int vertexIndex)
	{
		if (mesh.mUVOffset == 0)
			return RichVec2(0.0f, 0.0f);
		const int ofs = mesh.mUVOffset + vertexIndex * 12;
		return RichVec2(
			ReadFloat(mesh.mpData, mesh.mDataSize, ofs + 0),
			ReadFloat(mesh.mpData, mesh.mDataSize, ofs + 4));
	}

	static void PlotEffectVertex(noeRAPI_t *pRapi, const SEffectMeshData &mesh, const int vertexIndex)
	{
		RichVec3 pos = ReadPosition(mesh, vertexIndex);
		RichVec2 uv = ReadUV(mesh, vertexIndex);
		float normal[3] = { 0.0f, 1.0f, 0.0f };
		unsigned char color[4] = { 255, 255, 255, 255 };
		pRapi->rpgVertNormal3f(normal);
		pRapi->rpgVertUV2f(uv.v, 0);
		pRapi->rpgVertColor4ub(color);
		pRapi->rpgVertex3f(pos.v);
	}

	static void RenderEffectMesh(noeRAPI_t *pRapi, const SEffectMeshData &mesh)
	{
		char nameString[64];
		sprintf_s(nameString, "effect: %s", mesh.mChunkName);
		pRapi->rpgSetName(nameString);

		char materialName[CFFXITextureHandler::skTexNameLength + CFFXITextureHandler::skMaterialNamePad];
		memcpy(materialName, mesh.mMaterialName, CFFXITextureHandler::skTexNameLength);
		materialName[CFFXITextureHandler::skTexNameLength] = 0;
		const size_t matNameLen = strlen(materialName);
		strcpy_s(&materialName[matNameLen], sizeof(materialName) - matNameLen, CFFXITextureHandler::skpSoftBlendSuffix);
		pRapi->rpgSetMaterial(materialName);

		pRapi->rpgBegin(RPGEO_TRIANGLE);
		for (int triIndex = 0; triIndex < mesh.mVertexCount; ++triIndex)
		{
			const int indexOfs = mesh.mIndexOffset + triIndex * 6;
			const int idx0 = ReadU16(mesh.mpData, mesh.mDataSize, indexOfs + 0);
			const int idx1 = ReadU16(mesh.mpData, mesh.mDataSize, indexOfs + 2);
			const int idx2 = ReadU16(mesh.mpData, mesh.mDataSize, indexOfs + 4);
			if (idx0 < 0 || idx0 >= mesh.mVertexCount ||
				idx1 < 0 || idx1 >= mesh.mVertexCount ||
				idx2 < 0 || idx2 >= mesh.mVertexCount ||
				idx0 == idx1 || idx1 == idx2 || idx0 == idx2)
			{
				continue;
			}
			PlotEffectVertex(pRapi, mesh, idx0);
			PlotEffectVertex(pRapi, mesh, idx1);
			PlotEffectVertex(pRapi, mesh, idx2);
		}
		pRapi->rpgEnd();
	}

	TEffectMeshList mEffectMeshes;
};

//========================================================================================
