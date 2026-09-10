#pragma once

#include "model_ff11.h"
#include "model_ff11_texture_handler.h"
#include "effect_model_layout.h"

#pragma pack(push, 1)

class CFFXIEffectHandler : public CFFXIChunkHandler
{
public:
	enum EEffectLayout
	{
		kEffectLayout_Model1F,
		kEffectLayout_Animated21,
		kEffectLayout_Morph25
	};

	struct SEffectMeshData
	{
		SEffectMeshData()
			: mpData(NULL), mpAllocatingInstance(NULL), mDataSize(0), mLayout(kEffectLayout_Model1F),
			  mPrimitiveCount(0), mPositionCount(0), mVertexOffset(0), mColorOffset(0),
			  mUVOffset(0), mIndexOffset(0)
		{
			memset(mChunkName, 0, sizeof(mChunkName));
			memset(mMaterialName, 0, sizeof(mMaterialName));
			memset(mHeaderWords, 0, sizeof(mHeaderWords));
		}

		unsigned char *mpData;
		noeRAPI_t *mpAllocatingInstance;
		int mDataSize;
		char mChunkName[8];
		std::string mDirectoryPath;
		char mMaterialName[CFFXITextureHandler::skTexNameLength + 1];
		unsigned short mHeaderWords[8];
		EEffectLayout mLayout;
		int mPrimitiveCount;
		int mPositionCount;
		int mVertexOffset;
		int mColorOffset;
		int mUVOffset;
		int mIndexOffset;
		std::vector<int> mCardVertexOffsets;
	};
	typedef std::vector<SEffectMeshData> TEffectMeshList;

	explicit CFFXIEffectHandler(const int chunkType) : CFFXIChunkHandler(chunkType) {}

	virtual ~CFFXIEffectHandler()
	{
		for (TEffectMeshList::iterator it = mEffectMeshes.begin(); it != mEffectMeshes.end(); ++it)
		{
			SEffectMeshData &mesh = *it;
			if (mesh.mpData && mesh.mpAllocatingInstance)
				mesh.mpAllocatingInstance->Noesis_UnpooledFree(mesh.mpData);
		}
	}

	virtual CFFXIDat::EValidateChunkResult ValidateChunk(const CFFXIDat &, const CFFXIDat::SChunk &chunk,
															const unsigned char *pChunkData, const int dataSize) const
	{
		if (dataSize < 0)
			return CFFXIDat::kVCR_Invalid;

		SEffectMeshData mesh;
		// A known chunk type can still use an effect subtype/layout that DATura does
		// not render yet. That is not file corruption: returning kVCR_Invalid here
		// aborts parsing of the entire DAT and makes otherwise-valid zone files fail
		// recognition. Only advertise layouts that pass the strict geometry parser;
		// leave other variants in the raw chunk table as unsupported.
		return ParseEffectChunk(chunk.mType, pChunkData, dataSize, mesh)
			? CFFXIDat::kVCR_Supported
			: CFFXIDat::kVCR_NotSupported;
	}

	virtual bool HandleChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk,
								const unsigned char *pChunkData, const int dataSize)
	{
		SEffectMeshData mesh;
		if (!ParseEffectChunk(chunk.mType, pChunkData, dataSize, mesh))
			return true;

		noeRAPI_t *pRapi = dat.GetRAPI();
		mesh.mpData = (unsigned char *)pRapi->Noesis_UnpooledAlloc(dataSize);
		memcpy(mesh.mpData, pChunkData, dataSize);
		mesh.mpAllocatingInstance = pRapi;
		mesh.mDataSize = dataSize;
		memcpy(mesh.mChunkName, chunk.mName, 4);
		mesh.mChunkName[4] = 0;
		mesh.mDirectoryPath = dat.GetChunkDirectoryPath(chunk);
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

	static int Align16(const int value) { return (value + 15) & ~15; }
	static int UnwrapU16Offset(const unsigned short storedOffset, const int minimumOffset)
	{
		int offset = storedOffset;
		while (offset < minimumOffset)
			offset += 0x10000;
		return offset;
	}

	static bool PositionsAreSane(const unsigned char *pData, const int dataSize, const int ofs,
									  const int count, const int stride)
	{
		for (int i = 0; i < count; ++i)
		{
			const int vertexOfs = ofs + i * stride;
			if (!FloatIsSane(ReadFloat(pData, dataSize, vertexOfs + 0)) ||
				!FloatIsSane(ReadFloat(pData, dataSize, vertexOfs + 4)) ||
				!FloatIsSane(ReadFloat(pData, dataSize, vertexOfs + 8)))
				return false;
		}
		return true;
	}

	static void ReadHeaderWords(const unsigned char *pData, const int dataSize, SEffectMeshData &mesh)
	{
		for (int i = 0; i < 8; ++i)
			mesh.mHeaderWords[i] = ReadU16(pData, dataSize, i * 2);
	}

	static bool ParseEffectModel1F(const unsigned char *pData, const int dataSize, SEffectMeshData &mesh)
	{
		// FFXI Tool documented marker 6. Later retail marker-3 records retain the same
		// expanded triangle vertices behind a fixed 0x50-byte extended header.
		if (dataSize < 0x20 || (pData[0] != 3 && pData[0] != 6) ||
			pData[1] || pData[2] || pData[3])
			return false;
		ReadHeaderWords(pData, dataSize, mesh);
		const int imageCount = pData[4];
		mesh.mPrimitiveCount = ReadU16(pData, dataSize, 6);
		const bool extendedMarker3 = pData[0] == 3;
		const int materialOffset = extendedMarker3 ? 0x10 :
			EffectModelLayout::MaterialOffset6(imageCount, pData[5]);
		mesh.mVertexOffset = extendedMarker3 ? 0x50 :
			EffectModelLayout::VertexOffset6(imageCount, pData[5]);
		mesh.mLayout = kEffectLayout_Model1F;
		if (imageCount > 0 && RangeIsValid(materialOffset, CFFXITextureHandler::skTexNameLength, dataSize))
			memcpy(mesh.mMaterialName, pData + materialOffset, CFFXITextureHandler::skTexNameLength);
		if (mesh.mPrimitiveCount <= 0 ||
			!RangeIsValid(mesh.mVertexOffset, mesh.mPrimitiveCount * 3 * 36, dataSize))
			return false;
		return PositionsAreSane(pData, dataSize, mesh.mVertexOffset, mesh.mPrimitiveCount * 3, 36);
	}

	static bool ParseEffectAnimated21(const unsigned char *pData, const int dataSize, SEffectMeshData &mesh)
	{
		// Type 0x21 has two observed encodings. The older/direct encoding uses header
		// byte 6 as its total card count. Each card is two expanded triangles.
		// Its extended records are 0xA4 bytes: 4 bytes of flags, 16 bytes of
		// control/pivot data, then six 24-byte vertices.
		//
		// Generator-produced records use header byte 2 as a group count instead.
		// Each group is { uint16 kind=1, uint16 cardCount } followed by cardCount
		// contiguous 144-byte cards. Header byte 6 usually equals the sum, but is
		// only 39 in the known 106-group tam3 record, so the group headers are the
		// authoritative card count.
		if (dataSize < 0x1C)
			return false;
		ReadHeaderWords(pData, dataSize, mesh);
		const int extendedCardCount = pData[2];
		const int cardCount = pData[6];
		mesh.mLayout = kEffectLayout_Animated21;
		memcpy(mesh.mMaterialName, pData + 0x08, CFFXITextureHandler::skTexNameLength);

		// Retail compact cards: each frame has a four-byte 0x77010001 tag,
		// followed by six 24-byte vertices (Home Point tama/tubu resources).
		// Do not mistake the frame count for the number of 0xA4 extended cards.
		if (cardCount > 0 && Align16(0x18 + cardCount * 148) == dataSize)
		{
			bool compact = true;
			for (int card = 0; card < cardCount; ++card)
			{
				const int header = 0x18 + card * 148;
				if (!RangeIsValid(header, 148, dataSize) ||
					ReadU16(pData, dataSize, header) != 1 ||
					ReadU16(pData, dataSize, header + 2) != 0x7701 ||
					!PositionsAreSane(pData, dataSize, header + 4, 6, 24))
					compact = false;
			}
			if (compact)
			{
				for (int card = 0; card < cardCount; ++card)
					mesh.mCardVertexOffsets.push_back(0x1C + card * 148);
				mesh.mPrimitiveCount = cardCount * 2;
				return true;
			}
		}

		bool directLayoutValid = cardCount > 0;
		if (directLayoutValid && extendedCardCount <= 1)
		{
			mesh.mVertexOffset = 0x1C;
			if (!RangeIsValid(mesh.mVertexOffset, cardCount * 6 * 24, dataSize) ||
				!PositionsAreSane(pData, dataSize, mesh.mVertexOffset, cardCount * 6, 24))
				directLayoutValid = false;
			else
			{
				for (int card = 0; card < cardCount; ++card)
					mesh.mCardVertexOffsets.push_back(mesh.mVertexOffset + card * 6 * 24);
			}
		}
		else if (directLayoutValid)
		{
			if (cardCount < extendedCardCount)
				directLayoutValid = false;
			int recordOffset = 0x18;
			for (int card = 0; directLayoutValid && card < extendedCardCount; ++card)
			{
				const int vertexOffset = recordOffset + 20;
				if (!RangeIsValid(recordOffset, 0xA4, dataSize) ||
					!PositionsAreSane(pData, dataSize, vertexOffset, 6, 24))
					directLayoutValid = false;
				else
					mesh.mCardVertexOffsets.push_back(vertexOffset);
				recordOffset += 0xA4;
			}

			const int trailingCardCount = cardCount - extendedCardCount;
			if (directLayoutValid && trailingCardCount > 0)
			{
				const int trailingBytes = trailingCardCount * 6 * 24;
				const int extraBytes = dataSize - recordOffset - trailingBytes;
				if (extraBytes >= 4 && RangeIsValid(recordOffset, 4, dataSize) &&
					pData[recordOffset + 0] == 1 && pData[recordOffset + 1] == 0 &&
					pData[recordOffset + 2] == 1 && pData[recordOffset + 3] == 0)
					recordOffset += 4;
				if (!RangeIsValid(recordOffset, trailingBytes, dataSize) ||
					!PositionsAreSane(pData, dataSize, recordOffset, trailingCardCount * 6, 24))
					directLayoutValid = false;
				else
				{
					for (int card = 0; card < trailingCardCount; ++card)
						mesh.mCardVertexOffsets.push_back(recordOffset + card * 6 * 24);
				}
			}
		}
		if (directLayoutValid && (int)mesh.mCardVertexOffsets.size() == cardCount)
		{
			mesh.mPrimitiveCount = cardCount * 2;
			return true;
		}

		// The direct-layout probe can leave partial offsets behind.
		mesh.mCardVertexOffsets.clear();
		const int groupCount = pData[2];
		if (groupCount <= 0)
			return false;
		int cursor = 0x18;
		for (int group = 0; group < groupCount; ++group)
		{
			if (!RangeIsValid(cursor, 4, dataSize))
				return false;
			const int groupKind = ReadU16(pData, dataSize, cursor);
			const int groupCardCount = ReadU16(pData, dataSize, cursor + 2);
			cursor += 4;
			if (groupKind != 1)
				return false;
			if (!RangeIsValid(cursor, groupCardCount * 6 * 24, dataSize) ||
				!PositionsAreSane(pData, dataSize, cursor, groupCardCount * 6, 24))
				return false;
			for (int card = 0; card < groupCardCount; ++card)
				mesh.mCardVertexOffsets.push_back(cursor + card * 6 * 24);
			cursor += groupCardCount * 6 * 24;
		}
		if (mesh.mCardVertexOffsets.empty() || Align16(cursor) != dataSize)
			return false;
		mesh.mVertexOffset = mesh.mCardVertexOffsets.front();
		mesh.mPrimitiveCount = (int)mesh.mCardVertexOffsets.size() * 2;
		return true;
	}

	static bool ParseEffectMorph25(const unsigned char *pData, const int dataSize, SEffectMeshData &mesh)
	{
		// FFXI Tool names the words: image count, morph count, two position-pool counts,
		// index offset, triangle count, color offset, UV offset. Offsets are payload-relative.
		if (dataSize < 0x20)
			return false;
		ReadHeaderWords(pData, dataSize, mesh);
		mesh.mPositionCount = mesh.mHeaderWords[2];
		mesh.mPrimitiveCount = mesh.mHeaderWords[5];
		mesh.mVertexOffset = 0x20;
		// The offsets are 16-bit and wrap for payloads larger than 64 KiB. The fixed-size
		// per-corner blocks make the full offsets recoverable in color -> UV -> index order.
		mesh.mColorOffset = UnwrapU16Offset(mesh.mHeaderWords[6], mesh.mVertexOffset + mesh.mPositionCount * 16);
		mesh.mUVOffset = UnwrapU16Offset(mesh.mHeaderWords[7], mesh.mColorOffset + mesh.mPrimitiveCount * 3 * 4);
		mesh.mIndexOffset = UnwrapU16Offset(mesh.mHeaderWords[4], mesh.mUVOffset + mesh.mPrimitiveCount * 3 * 8);
		mesh.mLayout = kEffectLayout_Morph25;
		memcpy(mesh.mMaterialName, pData + 0x10, CFFXITextureHandler::skTexNameLength);
		if (mesh.mPositionCount <= 0 || mesh.mPrimitiveCount <= 0 ||
			!RangeIsValid(mesh.mVertexOffset, mesh.mPositionCount * 16, dataSize) ||
			!RangeIsValid(mesh.mColorOffset, mesh.mPrimitiveCount * 3 * 4, dataSize) ||
			!RangeIsValid(mesh.mUVOffset, mesh.mPrimitiveCount * 3 * 8, dataSize) ||
			!RangeIsValid(mesh.mIndexOffset, mesh.mPrimitiveCount * 3 * 4, dataSize))
			return false;
		if (!PositionsAreSane(pData, dataSize, mesh.mVertexOffset, mesh.mPositionCount, 16))
			return false;
		for (int i = 0; i < mesh.mPrimitiveCount * 3; ++i)
		{
			if (ReadU16(pData, dataSize, mesh.mIndexOffset + i * 2) >= mesh.mPositionCount)
				return false;
		}
		return true;
	}

	static bool ParseEffectChunk(const int chunkType, const unsigned char *pData, const int dataSize, SEffectMeshData &mesh)
	{
		switch (chunkType)
		{
		case CFFXIDat::skChunkType_EffectModel: return ParseEffectModel1F(pData, dataSize, mesh);
		case CFFXIDat::skChunkType_EffectAnimated: return ParseEffectAnimated21(pData, dataSize, mesh);
		case CFFXIDat::skChunkType_EffectMorph: return ParseEffectMorph25(pData, dataSize, mesh);
		default: return false;
		}
	}

	static void SetEffectMaterial(noeRAPI_t *pRapi, const SEffectMeshData &mesh)
	{
		char materialName[CFFXITextureHandler::skTexNameLength + CFFXITextureHandler::skMaterialNamePad];
		memcpy(materialName, mesh.mMaterialName, CFFXITextureHandler::skTexNameLength);
		materialName[CFFXITextureHandler::skTexNameLength] = 0;
		const size_t matNameLen = strlen(materialName);
		strcpy_s(&materialName[matNameLen], sizeof(materialName) - matNameLen, CFFXITextureHandler::skpSoftBlendSuffix);
		pRapi->rpgSetMaterial(materialName);
	}

	static void PlotDirectVertexAtOffset(noeRAPI_t *pRapi, const SEffectMeshData &mesh, const int ofs)
	{
		float pos[3] = {
			ReadFloat(mesh.mpData, mesh.mDataSize, ofs + 0),
			ReadFloat(mesh.mpData, mesh.mDataSize, ofs + 4),
			ReadFloat(mesh.mpData, mesh.mDataSize, ofs + 8) };
		const int normalOfs = (mesh.mLayout == kEffectLayout_Model1F) ? ofs + 12 : -1;
		const int colorOfs = (mesh.mLayout == kEffectLayout_Model1F) ? ofs + 24 : ofs + 12;
		const int uvOfs = colorOfs + 4;
		float normal[3] = { 0.0f, 0.0f, 1.0f };
		if (normalOfs >= 0)
		{
			normal[0] = ReadFloat(mesh.mpData, mesh.mDataSize, normalOfs + 0);
			normal[1] = ReadFloat(mesh.mpData, mesh.mDataSize, normalOfs + 4);
			normal[2] = ReadFloat(mesh.mpData, mesh.mDataSize, normalOfs + 8);
		}
		float uv[2] = { ReadFloat(mesh.mpData, mesh.mDataSize, uvOfs), ReadFloat(mesh.mpData, mesh.mDataSize, uvOfs + 4) };
		pRapi->rpgVertNormal3f(normal);
		pRapi->rpgVertUV2f(uv, 0);
		pRapi->rpgVertColor4ub(mesh.mpData + colorOfs);
		pRapi->rpgVertex3f(pos);
	}

	static void PlotDirectVertex(noeRAPI_t *pRapi, const SEffectMeshData &mesh, const int vertexIndex)
	{
		const int stride = (mesh.mLayout == kEffectLayout_Model1F) ? 36 : 24;
		PlotDirectVertexAtOffset(pRapi, mesh, mesh.mVertexOffset + vertexIndex * stride);
	}

	static void PlotMorphVertex(noeRAPI_t *pRapi, const SEffectMeshData &mesh, const int cornerIndex,
									const float normal[3])
	{
		const int positionIndex = ReadU16(mesh.mpData, mesh.mDataSize, mesh.mIndexOffset + cornerIndex * 2);
		const int posOfs = mesh.mVertexOffset + positionIndex * 16;
		float pos[3] = {
			ReadFloat(mesh.mpData, mesh.mDataSize, posOfs + 0),
			ReadFloat(mesh.mpData, mesh.mDataSize, posOfs + 4),
			ReadFloat(mesh.mpData, mesh.mDataSize, posOfs + 8) };
		const int uvOfs = mesh.mUVOffset + cornerIndex * 8;
		float uv[2] = { ReadFloat(mesh.mpData, mesh.mDataSize, uvOfs), ReadFloat(mesh.mpData, mesh.mDataSize, uvOfs + 4) };
		pRapi->rpgVertNormal3f((float *)normal);
		pRapi->rpgVertUV2f(uv, 0);
		pRapi->rpgVertColor4ub(mesh.mpData + mesh.mColorOffset + cornerIndex * 4);
		pRapi->rpgVertex3f(pos);
	}

	static void RenderEffectMesh(noeRAPI_t *pRapi, const SEffectMeshData &mesh)
	{
		char nameString[64];
		sprintf_s(nameString, "effect: %s", mesh.mChunkName);
		pRapi->rpgSetName(nameString);
		SetEffectMaterial(pRapi, mesh);

		pRapi->rpgBegin(RPGEO_TRIANGLE);
		if (mesh.mLayout == kEffectLayout_Model1F)
		{
			for (int i = 0; i < mesh.mPrimitiveCount * 3; ++i)
				PlotDirectVertex(pRapi, mesh, i);
		}
		else if (mesh.mLayout == kEffectLayout_Animated21)
		{
			for (std::vector<int>::const_iterator cardIt = mesh.mCardVertexOffsets.begin();
				 cardIt != mesh.mCardVertexOffsets.end(); ++cardIt)
			{
				for (int corner = 0; corner < 6; ++corner)
					PlotDirectVertexAtOffset(pRapi, mesh, *cardIt + corner * 24);
			}
		}
		else
		{
			for (int tri = 0; tri < mesh.mPrimitiveCount; ++tri)
			{
				float p[3][3];
				for (int corner = 0; corner < 3; ++corner)
				{
					const int cornerIndex = tri * 3 + corner;
					const int posIndex = ReadU16(mesh.mpData, mesh.mDataSize, mesh.mIndexOffset + cornerIndex * 2);
					const int ofs = mesh.mVertexOffset + posIndex * 16;
					for (int axis = 0; axis < 3; ++axis)
						p[corner][axis] = ReadFloat(mesh.mpData, mesh.mDataSize, ofs + axis * 4);
				}
				const float a[3] = { p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2] };
				const float b[3] = { p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2] };
				float normal[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
				const float len = sqrtf(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
				if (len > 0.000001f)
				{
					normal[0] /= len; normal[1] /= len; normal[2] /= len;
				}
				for (int corner = 0; corner < 3; ++corner)
					PlotMorphVertex(pRapi, mesh, tri * 3 + corner, normal);
			}
		}
		pRapi->rpgEnd();
	}

	TEffectMeshList mEffectMeshes;
};

#pragma pack(pop)
