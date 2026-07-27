static int Creation_ReadI32(const unsigned char *pData, const int dataSize, const int ofs)
{
	if (ofs < 0 || ofs + 4 > dataSize)
		return 0;
	return *(const int *)(pData + ofs);
}

static float Creation_ReadF32(const unsigned char *pData, const int dataSize, const int ofs)
{
	if (ofs < 0 || ofs + 4 > dataSize)
		return 0.0f;
	return *(const float *)(pData + ofs);
}

static bool Creation_ParseShapeCounts(const char *pText, int &triCount, int &codeCount, int &vertCount)
{
	triCount = codeCount = vertCount = 0;
	return sscanf_s(pText, "SHAPE: TriStrip ver.2, %i tris, %i codes, %i verts",
		&triCount, &codeCount, &vertCount) == 3;
}

static int Creation_FindShapeText(const unsigned char *pData, const int dataSize, const int shapeOfs)
{
	static const char kShapeTag[] = "SHAPE:";
	const int searchEnd = std::min(shapeOfs + 256, dataSize - (int)sizeof(kShapeTag));
	for (int ofs = shapeOfs; ofs < searchEnd; ++ofs)
	{
		if (memcmp(pData + ofs, kShapeTag, sizeof(kShapeTag) - 1) == 0)
			return ofs;
	}
	return -1;
}

static int Creation_FindPositionHeader(const unsigned char *pData, const int dataSize,
	const int searchOfs, const int vertCount)
{
	const int searchEnd = std::min(searchOfs + 128, dataSize - 16);
	for (int ofs = searchOfs; ofs <= searchEnd; ofs += 4)
	{
		if (Creation_ReadI32(pData, dataSize, ofs) == vertCount &&
			Creation_ReadI32(pData, dataSize, ofs + 4) == 3 &&
			Creation_ReadI32(pData, dataSize, ofs + 12) == 3)
		{
			return ofs;
		}
	}
	return -1;
}

static bool Creation_IsShapeBlock(const unsigned char *pData, const int dataSize, const int ofs)
{
	if (ofs < 0 || ofs + 0x60 > dataSize)
		return false;
	if (Creation_ReadI32(pData, dataSize, ofs) != 4)
		return false;
	if (memcmp(pData + ofs + 8, "RT", 2) != 0)
		return false;
	return Creation_FindShapeText(pData, dataSize, ofs) >= 0;
}

static int Creation_FindNextShapeBlock(const unsigned char *pData, const int dataSize,
	const int shapeOfs, const int blockSize)
{
	const int rawEnd = shapeOfs + blockSize;
	const int searchStart = (rawEnd + 0x20 + 3) & ~3;
	const int searchEnd = std::min(rawEnd + 0x90, dataSize - 0x60);
	for (int ofs = searchStart; ofs <= searchEnd; ofs += 4)
	{
		if (Creation_IsShapeBlock(pData, dataSize, ofs))
			return ofs;
	}
	return -1;
}

static void Creation_ApplySceneOffset(float pos[3], const float *offset)
{
	if (!offset)
		return;
	pos[0] += offset[0];
	pos[1] += offset[1];
	pos[2] += offset[2];
}

struct Creation_SkinInfluence
{
	int boneIndex;
	float weight;
};

typedef std::vector<std::vector<Creation_SkinInfluence> > Creation_ShapeSkin;

static int Creation_FindSqleChunk(const unsigned char *pData, const int dataSize,
	const int chunkType, const int startOfs)
{
	for (int ofs = (startOfs + 15) & ~15; ofs + 104 <= dataSize; ofs += 16)
	{
		if (memcmp(pData + ofs, "SQLE", 4) == 0 &&
			*(const unsigned short *)(pData + ofs + 10) == chunkType)
			return ofs;
	}
	return -1;
}

static bool Creation_ParseSqleSkeleton(const unsigned char *pData, const int dataSize,
	const int fileIndex, const int combinedBoneStart, const float *sceneOffset,
	std::vector<FFXISqleBoneInfo> &outBones)
{
	const int chunkOfs = Creation_FindSqleChunk(pData, dataSize, 11, 0);
	if (chunkOfs < 0)
		return false;
	const int boneCount = Creation_ReadI32(pData, dataSize, chunkOfs + 96);
	const int recordsOfs = chunkOfs + 100;
	if (boneCount <= 0 || boneCount > 1024 || recordsOfs + boneCount * 64 > dataSize)
		return false;

	outBones.reserve(outBones.size() + boneCount);
	for (int boneIndex = 0; boneIndex < boneCount; ++boneIndex)
	{
		const int boneOfs = recordsOfs + boneIndex * 64;
		FFXISqleBoneInfo info = {};
		info.parentIndex = Creation_ReadI32(pData, dataSize, boneOfs + 60);
		if (info.parentIndex >= 0)
			info.parentIndex += combinedBoneStart;
		info.fileIndex = fileIndex;
		info.sourceBoneIndex = boneIndex;
		for (int channelGroup = 0; channelGroup < 5; ++channelGroup)
			info.channelCounts[channelGroup] = Creation_ReadI32(pData, dataSize, boneOfs + 40 + channelGroup * 4);
		for (int axis = 0; axis < 3; ++axis)
		{
			info.bindTranslation[axis] = Creation_ReadF32(pData, dataSize, boneOfs + axis * 4);
			info.bindScale[axis] = Creation_ReadF32(pData, dataSize, boneOfs + 28 + axis * 4);
			info.rootOffset[axis] = sceneOffset ? sceneOffset[axis] : 0.0f;
		}
		for (int component = 0; component < 4; ++component)
			info.bindQuaternion[component] = Creation_ReadF32(pData, dataSize, boneOfs + 12 + component * 4);
		outBones.push_back(info);
	}
	return true;
}

static void Creation_ParseSqleSkins(const unsigned char *pData, const int dataSize,
	std::vector<Creation_ShapeSkin> &outSkins)
{
	outSkins.clear();
	for (int searchOfs = 0;;)
	{
		const int chunkOfs = Creation_FindSqleChunk(pData, dataSize, 21, searchOfs);
		if (chunkOfs < 0)
			break;
		searchOfs = chunkOfs + 16;

		const int clusterCount = Creation_ReadI32(pData, dataSize, chunkOfs + 96);
		const int vertexCount = Creation_ReadI32(pData, dataSize, chunkOfs + 100);
		if (clusterCount <= 0 || clusterCount > 1024 || vertexCount <= 0 || vertexCount > 1000000)
			continue;

		Creation_ShapeSkin skin((size_t)vertexCount);
		int cursor = chunkOfs + 104;
		bool valid = true;
		for (int clusterIndex = 0; clusterIndex < clusterCount && valid; ++clusterIndex)
		{
			if (cursor + 8 > dataSize) { valid = false; break; }
			const int boneIndex = Creation_ReadI32(pData, dataSize, cursor);
			const int influenceCount = Creation_ReadI32(pData, dataSize, cursor + 4);
			cursor += 8;
			if (boneIndex < 0 || influenceCount < 0 || influenceCount > vertexCount ||
				cursor + influenceCount * 8 > dataSize)
			{
				valid = false;
				break;
			}
			const int indicesOfs = cursor;
			const int weightsOfs = cursor + influenceCount * 4;
			for (int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
			{
				const int vertexIndex = Creation_ReadI32(pData, dataSize, indicesOfs + influenceIndex * 4);
				const float weight = Creation_ReadF32(pData, dataSize, weightsOfs + influenceIndex * 4);
				if (vertexIndex >= 0 && vertexIndex < vertexCount && weight > 0.0f)
					skin[(size_t)vertexIndex].push_back({ boneIndex, weight });
			}
			cursor += influenceCount * 8;
		}
		if (valid)
			outSkins.push_back(std::move(skin));
	}
}

static void Creation_SetPendingSkin(noeRAPI_t *pRapi, const Creation_ShapeSkin *pShapeSkin,
	const int vertIndex, const int combinedBoneStart, const modelBone_t *pBones,
	const int boneCount, const float pos[3], const float nrm[3])
{
	if (!pShapeSkin || vertIndex < 0 || vertIndex >= (int)pShapeSkin->size() || !pBones)
	{
		pRapi->rpgSetPendingSkinData(NULL);
		return;
	}
	const std::vector<Creation_SkinInfluence> &influences = (*pShapeSkin)[(size_t)vertIndex];
	if (influences.empty())
	{
		pRapi->rpgSetPendingSkinData(NULL);
		return;
	}

	std::vector<Creation_SkinInfluence> sortedInfluences = influences;
	std::sort(sortedInfluences.begin(), sortedInfluences.end(),
		[](const Creation_SkinInfluence &a, const Creation_SkinInfluence &b) { return a.weight > b.weight; });
	const int weightCount = std::min((int)sortedInfluences.size(), FFXISkinVertex::kMaxWeights);
	float totalWeight = 0.0f;
	for (int weightIndex = 0; weightIndex < weightCount; ++weightIndex)
		totalWeight += sortedInfluences[(size_t)weightIndex].weight;
	if (weightCount <= 0 || totalWeight <= 0.000001f)
	{
		pRapi->rpgSetPendingSkinData(NULL);
		return;
	}

	FFXISkinVertex skin;
	skin.skinned = true;
	skin.weightCount = weightCount;
	for (int weightIndex = 0; weightIndex < skin.weightCount; ++weightIndex)
	{
		const Creation_SkinInfluence &influence = sortedInfluences[(size_t)weightIndex];
		const int boneIndex = combinedBoneStart + influence.boneIndex;
		if (boneIndex < 0 || boneIndex >= boneCount)
			continue;
		skin.boneIdx[weightIndex] = boneIndex;
		skin.boneWt[weightIndex] = influence.weight / totalWeight;
		const RichMat43 invBind = ((const RichMat43 &)pBones[boneIndex].mat).GetInverse();
		const RichMat44 invBind44 = invBind.ToMat44();
		const RichVec4 localPos = invBind44.TransformVec4(RichVec4(pos[0], pos[1], pos[2], 1.0f));
		RichVec3 localNrm = invBind44.TransformNormal(RichVec3(nrm));
		localNrm.Normalize();
		for (int axis = 0; axis < 3; ++axis)
		{
			skin.pos[weightIndex][axis] = localPos[axis];
			skin.nrm[weightIndex][axis] = localNrm[axis];
		}
	}
	pRapi->rpgSetPendingSkinData(&skin);
}

static void Creation_EmitStrip(noeRAPI_t *pRapi, const unsigned char *pData, const int dataSize,
	const int posOfs, const int normalOfs, const int uvOfs,
	const std::vector<int> &strip, const float *sceneOffset,
	const Creation_ShapeSkin *pShapeSkin, const int combinedBoneStart,
	const modelBone_t *pBones, const int boneCount)
{
	if (strip.size() < 3)
		return;

	pRapi->rpgBegin(RPGEO_TRIANGLE_STRIP);
	for (size_t i = 0; i < strip.size(); ++i)
	{
		const int vertIndex = strip[i];
		float pos[3] =
		{
			Creation_ReadF32(pData, dataSize, posOfs + vertIndex * 12 + 0),
			Creation_ReadF32(pData, dataSize, posOfs + vertIndex * 12 + 4),
			Creation_ReadF32(pData, dataSize, posOfs + vertIndex * 12 + 8)
		};
		float nrm[3] =
		{
			Creation_ReadF32(pData, dataSize, normalOfs + vertIndex * 12 + 0),
			Creation_ReadF32(pData, dataSize, normalOfs + vertIndex * 12 + 4),
			Creation_ReadF32(pData, dataSize, normalOfs + vertIndex * 12 + 8)
		};
		float uv[2] =
		{
			Creation_ReadF32(pData, dataSize, uvOfs + vertIndex * 8 + 0),
			Creation_ReadF32(pData, dataSize, uvOfs + vertIndex * 8 + 4)
		};
		// Creation meshes use the opposite vertical axis from the in-game
		// model path.  Flip it here so the head/body assemble right-side up.
		pos[1] = -pos[1];
		Creation_ApplySceneOffset(pos, sceneOffset);
		nrm[1] = -nrm[1];

		unsigned char rgba[4] = { 255, 255, 255, 255 };

		pRapi->rpgVertNormal3f(nrm);
		pRapi->rpgVertUV2f(uv, 0);
		pRapi->rpgVertColor4ub(rgba);
		Creation_SetPendingSkin(pRapi, pShapeSkin, vertIndex, combinedBoneStart, pBones, boneCount, pos, nrm);
		pRapi->rpgVertex3f(pos);
	}
	pRapi->rpgEnd();
}

static void Creation_EmitTriangleList(noeRAPI_t *pRapi, const unsigned char *pData, const int dataSize,
	const int posOfs, const int normalOfs, const int uvOfs,
	const std::vector<int> &indices, const float *sceneOffset,
	const Creation_ShapeSkin *pShapeSkin, const int combinedBoneStart,
	const modelBone_t *pBones, const int boneCount)
{
	if (indices.size() < 3)
		return;

	pRapi->rpgBegin(RPGEO_TRIANGLE);
	for (size_t i = 0; i < indices.size(); ++i)
	{
		const int vertIndex = indices[i];
		float pos[3] =
		{
			Creation_ReadF32(pData, dataSize, posOfs + vertIndex * 12 + 0),
			Creation_ReadF32(pData, dataSize, posOfs + vertIndex * 12 + 4),
			Creation_ReadF32(pData, dataSize, posOfs + vertIndex * 12 + 8)
		};
		float nrm[3] =
		{
			Creation_ReadF32(pData, dataSize, normalOfs + vertIndex * 12 + 0),
			Creation_ReadF32(pData, dataSize, normalOfs + vertIndex * 12 + 4),
			Creation_ReadF32(pData, dataSize, normalOfs + vertIndex * 12 + 8)
		};
		float uv[2] =
		{
			Creation_ReadF32(pData, dataSize, uvOfs + vertIndex * 8 + 0),
			Creation_ReadF32(pData, dataSize, uvOfs + vertIndex * 8 + 4)
		};
		pos[1] = -pos[1];
		Creation_ApplySceneOffset(pos, sceneOffset);
		nrm[1] = -nrm[1];

		unsigned char rgba[4] = { 255, 255, 255, 255 };

		pRapi->rpgVertNormal3f(nrm);
		pRapi->rpgVertUV2f(uv, 0);
		pRapi->rpgVertColor4ub(rgba);
		Creation_SetPendingSkin(pRapi, pShapeSkin, vertIndex, combinedBoneStart, pBones, boneCount, pos, nrm);
		pRapi->rpgVertex3f(pos);
	}
	pRapi->rpgEnd();
}

static bool Creation_RenderShape(noeRAPI_t *pRapi, const unsigned char *pData, const int dataSize,
	const int shapeOfs, const char *materialName, const float *sceneOffset,
	const Creation_ShapeSkin *pShapeSkin, const int combinedBoneStart,
	const modelBone_t *pBones, const int boneCount)
{
	const int textOfs = Creation_FindShapeText(pData, dataSize, shapeOfs);
	if (textOfs < 0)
		return false;

	int textEnd = textOfs;
	while (textEnd < dataSize && pData[textEnd] != 0)
		++textEnd;
	if (textEnd >= dataSize)
		return false;

	int triCount, codeCount, vertCount;
	if (!Creation_ParseShapeCounts((const char *)(pData + textOfs), triCount, codeCount, vertCount))
		return false;
	if (vertCount <= 0 || codeCount <= 0 || triCount <= 0)
		return false;

	const int posHdrOfs = Creation_FindPositionHeader(pData, dataSize, (textEnd + 3) & ~3, vertCount);
	if (posHdrOfs < 0)
		return false;

	const int posOfs = posHdrOfs + 16;
	const int normalHdrOfs = posOfs + vertCount * 12;
	const int normalOfs = normalHdrOfs + 8;
	const int uvHdrOfs = normalOfs + vertCount * 12;
	const int uvOfs = uvHdrOfs + 8;
	int codesOfs = uvOfs + vertCount * 8;

	if (codesOfs + 4 > dataSize)
		return false;
	if (Creation_ReadI32(pData, dataSize, codesOfs) == codeCount)
		codesOfs += 4;
	if (codesOfs + codeCount * 4 > dataSize)
		return false;

	char name[32];
	sprintf_s(name, "creation_%06X", shapeOfs);
	pRapi->rpgSetName(name);
	pRapi->rpgSetMaterial(materialName ? materialName : "creation_default");

	// The code stream is mixed: non-negative values outside a strip are draw
	// state/material selectors, while a negative value starts a strip whose
	// vertex count is the absolute value of that command.
	std::vector<int> strip;
	strip.reserve(64);
	std::vector<int> triIndices;
	triIndices.reserve(256);
	for (int codeIndex = 0; codeIndex < codeCount;)
	{
		const int cmd = Creation_ReadI32(pData, dataSize, codesOfs + codeIndex * 4);
		++codeIndex;
		if (cmd >= 0)
		{
			if (cmd >= 3 && (cmd % 3) == 0 && codeIndex + cmd <= codeCount)
			{
				triIndices.clear();
				bool validList = true;
				for (int triIndex = 0; triIndex < cmd; ++triIndex)
				{
					const int vertIndex = Creation_ReadI32(pData, dataSize, codesOfs + (codeIndex + triIndex) * 4);
					if (vertIndex < 0 || vertIndex >= vertCount)
					{
						validList = false;
						break;
					}
					triIndices.push_back(vertIndex);
				}
				if (validList)
				{
					Creation_EmitTriangleList(pRapi, pData, dataSize, posOfs, normalOfs, uvOfs, triIndices, sceneOffset,
						pShapeSkin, combinedBoneStart, pBones, boneCount);
					codeIndex += cmd;
				}
			}
			continue;
		}

		const int stripVertCount = -cmd;
		if (stripVertCount < 3 || codeIndex + stripVertCount > codeCount)
			break;

		strip.clear();
		for (int stripIndex = 0; stripIndex < stripVertCount; ++stripIndex)
		{
			const int vertIndex = Creation_ReadI32(pData, dataSize, codesOfs + codeIndex * 4);
			++codeIndex;
			if (vertIndex >= 0 && vertIndex < vertCount)
				strip.push_back(vertIndex);
		}
		Creation_EmitStrip(pRapi, pData, dataSize, posOfs, normalOfs, uvOfs, strip, sceneOffset,
			pShapeSkin, combinedBoneStart, pBones, boneCount);
	}

	return true;
}

static int Creation_RenderDAT(noeRAPI_t *rapi, BYTE *fileBuffer, int bufferLen,
	const char *opaqueMaterialName, const char *alphaMaterialName = NULL,
	const char *blackKeyMaterialName = NULL,
	const std::vector<bool> *shapeUsesAlpha = NULL,
	const std::vector<bool> *shapeUsesBlackKey = NULL, const float *sceneOffset = NULL,
	const std::vector<Creation_ShapeSkin> *pShapeSkins = NULL, const int combinedBoneStart = 0,
	const modelBone_t *pBones = NULL, const int boneCount = 0)
{
	int renderedShapeCount = 0;
	int shapeIndex = 0;
	for (int shapeOfs = 0; shapeOfs + 0x60 < bufferLen;)
	{
		if (!Creation_IsShapeBlock(fileBuffer, bufferLen, shapeOfs))
			break;

		const bool useAlpha = alphaMaterialName && shapeUsesAlpha &&
			shapeIndex < (int)shapeUsesAlpha->size() && (*shapeUsesAlpha)[shapeIndex];
		const bool useBlackKey = blackKeyMaterialName && shapeUsesBlackKey &&
			shapeIndex < (int)shapeUsesBlackKey->size() && (*shapeUsesBlackKey)[shapeIndex];
		const char *materialName = useAlpha ? alphaMaterialName :
			(useBlackKey ? blackKeyMaterialName : opaqueMaterialName);
		const Creation_ShapeSkin *pShapeSkin = pShapeSkins && shapeIndex < (int)pShapeSkins->size() ?
			&(*pShapeSkins)[(size_t)shapeIndex] : NULL;
		if (Creation_RenderShape(rapi, fileBuffer, bufferLen, shapeOfs, materialName, sceneOffset,
			pShapeSkin, combinedBoneStart, pBones, boneCount))
			++renderedShapeCount;
		++shapeIndex;

		const int blockSize = Creation_ReadI32(fileBuffer, bufferLen, shapeOfs + 4);
		const int nextShapeOfs = Creation_FindNextShapeBlock(fileBuffer, bufferLen, shapeOfs, blockSize);
		if (nextShapeOfs <= shapeOfs)
			break;
		shapeOfs = nextShapeOfs;
	}

	return renderedShapeCount;
}

static int Creation_ComputeDMBTextureScore(const unsigned char *pData, const int dataSize,
	const int texBlockOfs)
{
	const int width = Creation_ReadI32(pData, dataSize, texBlockOfs + 0x40);
	const int height = Creation_ReadI32(pData, dataSize, texBlockOfs + 0x44);
	const int bytesPerPixel = Creation_ReadI32(pData, dataSize, texBlockOfs + 0x48);
	const int pixOfs = texBlockOfs + 0x60;
	const int sampleCount = std::min(width * height, 4096);

	int brightness = 0;
	for (int i = 0; i < sampleCount; ++i)
	{
		const unsigned char *pPixel = pData + pixOfs + i * bytesPerPixel;
		brightness += (int)pPixel[0] + (int)pPixel[1] + (int)pPixel[2];
	}

	const int averageBrightness = (sampleCount > 0) ? (brightness / sampleCount) : 0;
	return width * height + averageBrightness;
}

static int Creation_FindDMBTextureBlock(const unsigned char *pData, const int dataSize)
{
	if (!pData || dataSize < 0x100 || memcmp(pData, "DMB\0", 4) != 0)
		return -1;

	int bestOfs = -1;
	int bestScore = -1;
	for (int ofs = 0x20; ofs + 0x460 < dataSize; ofs += 16)
	{
		const int width = Creation_ReadI32(pData, dataSize, ofs + 0x40);
		const int height = Creation_ReadI32(pData, dataSize, ofs + 0x44);
		const int bytesPerPalColor = Creation_ReadI32(pData, dataSize, ofs + 0x48);
		if (width < 16 || height < 16 || width > 2048 || height > 2048)
			continue;
		if (bytesPerPalColor != 3 && bytesPerPalColor != 4)
			continue;

		const int dataOfs = ofs + 0x60;
		const int requiredSize = dataOfs + width * height * bytesPerPalColor;
		if (requiredSize <= dataSize)
		{
			const int score = Creation_ComputeDMBTextureScore(pData, dataSize, ofs);
			if (score > bestScore)
			{
				bestScore = score;
				bestOfs = ofs;
			}
		}

	}

	return bestOfs;
}

static char Creation_ToLowerASCII(const char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static bool Creation_EqualASCIIInsensitive(const std::string &a, const std::string &b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (Creation_ToLowerASCII(a[i]) != Creation_ToLowerASCII(b[i]))
			return false;
	return true;
}

static bool Creation_EndsWithASCIIInsensitive(const std::string &text, const char *suffix)
{
	if (!suffix)
		return false;
	const size_t suffixLen = strlen(suffix);
	if (text.size() < suffixLen)
		return false;
	for (size_t i = 0; i < suffixLen; ++i)
		if (Creation_ToLowerASCII(text[text.size() - suffixLen + i]) !=
			Creation_ToLowerASCII(suffix[i]))
			return false;
	return true;
}

static void Creation_GetDMBShapeMaterialFlags(const BYTE *materialBuffer, const int materialLen,
	std::vector<bool> &shapeUsesAlpha, std::vector<bool> &shapeUsesBlackKey)
{
	shapeUsesAlpha.clear();
	shapeUsesBlackKey.clear();
	if (!materialBuffer || materialLen <= 0)
		return;

	std::vector<std::string> sortedShapeNames;
	std::vector<std::string> geometryShapeNames;
	for (int ofs = 0; ofs < materialLen;)
	{
		if (materialBuffer[ofs] < 0x20 || materialBuffer[ofs] > 0x7E)
		{
			++ofs;
			continue;
		}

		const int stringOfs = ofs;
		while (ofs < materialLen && materialBuffer[ofs] >= 0x20 && materialBuffer[ofs] <= 0x7E)
			++ofs;
		if (ofs >= materialLen || materialBuffer[ofs] != 0 || ofs - stringOfs < 4)
			continue;

		std::string value((const char *)materialBuffer + stringOfs, ofs - stringOfs);
		if (Creation_EndsWithASCIIInsensitive(value, "Shape_sort"))
		{
			value.resize(value.size() - strlen("_sort"));
			size_t nameOfs = value.size();
			while (nameOfs > 0)
			{
				const char c = value[nameOfs - 1];
				if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
					(c >= '0' && c <= '9') || c == '_'))
					break;
				--nameOfs;
			}
			value = value.substr(nameOfs);
			sortedShapeNames.push_back(value);
		}
		else if (Creation_EndsWithASCIIInsensitive(value, "shape.sqo"))
		{
			const size_t slash = value.find_last_of("/\\");
			std::string shapeName = value.substr(slash == std::string::npos ? 0 : slash + 1);
			shapeName.resize(shapeName.size() - strlen(".sqo"));
			geometryShapeNames.push_back(shapeName);
		}
		++ofs;
	}

	for (size_t shapeIndex = 0; shapeIndex < geometryShapeNames.size(); ++shapeIndex)
	{
		bool sorted = false;
		for (size_t sortedIndex = 0; sortedIndex < sortedShapeNames.size(); ++sortedIndex)
		{
			if (Creation_EqualASCIIInsensitive(geometryShapeNames[shapeIndex], sortedShapeNames[sortedIndex]))
			{
				sorted = true;
				break;
			}
		}
		const bool namedAlphaShape =
			Creation_EqualASCIIInsensitive(geometryShapeNames[shapeIndex], "alphaShape");
		shapeUsesAlpha.push_back(sorted || namedAlphaShape);
		// Hume male's strap shape contains an opaque lace/trim overlay on an
		// exported black, zero-alpha backing. It is not tagged *_sort, so it
		// needs the hard color-key material rather than general body masking.
		shapeUsesBlackKey.push_back(
			Creation_EqualASCIIInsensitive(geometryShapeNames[shapeIndex], "strapShape"));
	}
}

static bool Creation_BuildDMBMaterials(noeRAPI_t *rapi,
	BYTE *materialBuffer, int materialLen, const char *opaqueMaterialName,
	const char *alphaMaterialName, const char *blackKeyMaterialName,
	CArrayList<noesisTex_t *> &textures,
	CArrayList<noesisMaterial_t *> &materials, const int alphaMode)
{
	if (!materialBuffer || materialLen <= 0)
		return false;

	const int texBlockOfs = Creation_FindDMBTextureBlock(materialBuffer, materialLen);
	if (texBlockOfs < 0)
		return false;

	const int width = Creation_ReadI32(materialBuffer, materialLen, texBlockOfs + 0x40);
	const int height = Creation_ReadI32(materialBuffer, materialLen, texBlockOfs + 0x44);
	const int bytesPerPixel = Creation_ReadI32(materialBuffer, materialLen, texBlockOfs + 0x48);
	const int pixOfs = texBlockOfs + 0x60;

	std::vector<unsigned char> blackAlphaKey;
	if (alphaMode == FFXI_CREATION_ALPHA_BODY_CUTOUT && bytesPerPixel > 3)
	{
		blackAlphaKey.resize(width * height, 0);
		for (int y = 1; y < height - 1; ++y)
		{
			for (int x = 1; x < width - 1; ++x)
			{
				bool solidKey = true;
				for (int py = y - 1; py <= y + 1 && solidKey; ++py)
				{
					for (int px = x - 1; px <= x + 1; ++px)
					{
						const unsigned char *p = materialBuffer + pixOfs +
							(py * width + px) * bytesPerPixel;
						// Values below 16 collapse to the transparent zero nibble when
						// the source is encoded as DXT3.
						if (p[3] >= 16 || p[0] > 16 || p[1] > 16 || p[2] > 16)
						{
							solidKey = false;
							break;
						}
					}
				}
				if (solidKey)
					blackAlphaKey[y * width + x] = 1;
			}
		}

		const std::vector<unsigned char> keyCores = blackAlphaKey;
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				const unsigned char *p = materialBuffer + pixOfs +
					(y * width + x) * bytesPerPixel;
				if (p[3] >= 16 || p[0] > 16 || p[1] > 16 || p[2] > 16)
					continue;
				for (int py = std::max(0, y - 2); py <= std::min(height - 1, y + 2); ++py)
				{
					for (int px = std::max(0, x - 2); px <= std::min(width - 1, x + 2); ++px)
					{
						if (keyCores[py * width + px])
							blackAlphaKey[y * width + x] = 1;
					}
				}
			}
		}
	}

	unsigned char *opaqueRgba = (unsigned char *)rapi->Noesis_UnpooledAlloc(width * height * 4);
	unsigned char *alphaRgba = (unsigned char *)rapi->Noesis_UnpooledAlloc(width * height * 4);
	unsigned char *blackKeyRgba = (unsigned char *)rapi->Noesis_UnpooledAlloc(width * height * 4);
	for (int y = 0; y < height; ++y)
	{
		const unsigned char *src = materialBuffer + pixOfs + y * width * bytesPerPixel;
		for (int x = 0; x < width; ++x)
		{
			const unsigned char *srcPixel = src + x * bytesPerPixel;
			unsigned char *opaqueDst = opaqueRgba + (y * width + x) * 4;
			unsigned char *alphaDst = alphaRgba + (y * width + x) * 4;
			unsigned char *blackKeyDst = blackKeyRgba + (y * width + x) * 4;
			for (int channel = 0; channel < 3; ++channel)
			{
				const unsigned char value = srcPixel[2 - channel];
				opaqueDst[channel] = value;
				alphaDst[channel] = value;
				blackKeyDst[channel] = value;
			}
			// The fourth DMB channel is only opacity on alpha-enabled surfaces.
			// Opaque body surfaces use it solely to confirm the exporter's exact
			// black, zero-alpha color key; other dark texture detail stays opaque.
			opaqueDst[3] = 255;
			blackKeyDst[3] = !blackAlphaKey.empty() && blackAlphaKey[y * width + x] ? 0 : 255;
			if (bytesPerPixel > 3)
			{
				// Match the DXT3 handling used by the retail DAT path: FFXI authors
				// opacity in the lower half of the decoded 4-bit range, so expand a
				// decoded nibble by roughly 1.875 before applying the alpha test.
				const int dxtAlphaNibble = srcPixel[3] >> 4;
				alphaDst[3] = (unsigned char)std::min(dxtAlphaNibble * 32, 255);
			}
			else
			{
				alphaDst[3] = 255;
			}
		}
	}

	char opaqueTexName[64];
	char alphaTexName[64];
	char blackKeyTexName[64];
	sprintf_s(opaqueTexName, "%s_tex", opaqueMaterialName ? opaqueMaterialName : "creation_default");
	sprintf_s(alphaTexName, "%s_tex", alphaMaterialName ? alphaMaterialName : "creation_alpha");
	sprintf_s(blackKeyTexName, "%s_tex", blackKeyMaterialName ? blackKeyMaterialName : "creation_black_key");
	noesisTex_t *pOpaqueTex = rapi->Noesis_TextureAlloc(
		opaqueTexName, width, height, opaqueRgba, NOESISTEX_RGBA32);
	noesisTex_t *pAlphaTex = rapi->Noesis_TextureAlloc(
		alphaTexName, width, height, alphaRgba, NOESISTEX_RGBA32);
	noesisTex_t *pBlackKeyTex = rapi->Noesis_TextureAlloc(
		blackKeyTexName, width, height, blackKeyRgba, NOESISTEX_RGBA32);
	pOpaqueTex->shouldFreeData = true;
	pAlphaTex->shouldFreeData = true;
	pBlackKeyTex->shouldFreeData = true;

	const int opaqueTexIndex = textures.Num();
	textures.Append(pOpaqueTex);
	const int alphaTexIndex = textures.Num();
	textures.Append(pAlphaTex);
	const int blackKeyTexIndex = textures.Num();
	textures.Append(pBlackKeyTex);

	noesisMaterial_t *pOpaqueMat = rapi->Noesis_GetMaterialList(1, true);
	pOpaqueMat->name = rapi->Noesis_PooledString(opaqueMaterialName ? opaqueMaterialName : "creation_default");
	pOpaqueMat->texIdx = opaqueTexIndex;
	pOpaqueMat->flags = NMATFLAG_TWOSIDED;
	pOpaqueMat->noDefaultBlend = true;
	pOpaqueMat->alphaTest = 0.0f;

	noesisMaterial_t *pAlphaMat = rapi->Noesis_GetMaterialList(1, true);
	pAlphaMat->name = rapi->Noesis_PooledString(alphaMaterialName ? alphaMaterialName : "creation_alpha");
	pAlphaMat->texIdx = alphaTexIndex;
	pAlphaMat->flags = NMATFLAG_TWOSIDED;
	pAlphaMat->noDefaultBlend = true;
	pAlphaMat->alphaTest = 0.5f;

	noesisMaterial_t *pBlackKeyMat = rapi->Noesis_GetMaterialList(1, true);
	pBlackKeyMat->name = rapi->Noesis_PooledString(
		blackKeyMaterialName ? blackKeyMaterialName : "creation_black_key");
	pBlackKeyMat->texIdx = blackKeyTexIndex;
	pBlackKeyMat->flags = NMATFLAG_TWOSIDED;
	pBlackKeyMat->noDefaultBlend = true;
	pBlackKeyMat->alphaTest = 0.5f;
	materials.Append(pOpaqueMat);
	materials.Append(pAlphaMat);
	materials.Append(pBlackKeyMat);
	return true;
}

bool Model_FF11_CheckCreationDAT(BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi)
{
	(void)rapi;
	if (!fileBuffer || bufferLen < 0x60)
		return false;
	return Creation_IsShapeBlock(fileBuffer, bufferLen, 0);
}

bool Model_FF11_GetDATBonePosition(BYTE *fileBuffer, int bufferLen,
	const char *boneName, float outPos[3], noeRAPI_t *rapi)
{
	if (outPos)
	{
		outPos[0] = 0.0f;
		outPos[1] = 0.0f;
		outPos[2] = 0.0f;
	}
	if (!fileBuffer || bufferLen <= 0 || !boneName || !boneName[0] || !outPos || !rapi)
		return false;

	CFFXIDat dat(fileBuffer, bufferLen, rapi);
	CFFXIDefaultHandlerSet datHandlers(&dat);
	if (!dat.ParseChunksOfInterest() ||
		!dat.RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_Skeleton))
	{
		return false;
	}

	CFFXISkelHandler *pSkelHandler = datHandlers.SkelHandler();
	if (!pSkelHandler || pSkelHandler->Skeletons().empty())
		return false;

	const CFFXISkelHandler::SInterpretedSkel &skel = pSkelHandler->Skeletons()[0];
	for (int boneIndex = 0; boneIndex < skel.mBoneCount; ++boneIndex)
	{
		const modelBone_t *pBone = skel.mpBones + boneIndex;
		if (pBone && strcmp(pBone->name, boneName) == 0)
		{
			const RichMat43 &boneMat = (const RichMat43 &)pBone->mat;
			outPos[0] = boneMat[3][0];
			outPos[1] = boneMat[3][1];
			outPos[2] = boneMat[3][2];
			return true;
		}
	}

	return false;
}

noesisModel_t *Model_FF11_LoadCreationDAT(BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi)
{
	numMdl = 0;
	if (!Model_FF11_CheckCreationDAT(fileBuffer, bufferLen, rapi))
		return NULL;

	void *pCtx = rapi->rpgCreateContext();
	rapi->rpgSetOption(RPGOPT_TRIWINDBACKWARD, true);

	const int renderedShapeCount = Creation_RenderDAT(rapi, fileBuffer, bufferLen, "creation_default");

	noesisModel_t *pMdl = NULL;
	if (renderedShapeCount > 0)
	{
		pMdl = rapi->rpgConstructModelAndSort();
		numMdl = pMdl ? 1 : 0;
	}

	rapi->rpgDestroyContext(pCtx);
	Model_FF11_SetPreviewOffset(rapi);
	return pMdl;
}

noesisModel_t *Model_FF11_LoadCreationDATList(BYTE **fileBuffers, int *bufferLens,
	BYTE **materialBuffers, int *materialLens, int *materialAlphaModes, float *meshOffsets,
	int fileCount, int &numMdl, noeRAPI_t *rapi)
{
	numMdl = 0;
	if (!fileBuffers || !bufferLens || fileCount <= 0)
		return NULL;

	void *pCtx = rapi->rpgCreateContext();
	rapi->rpgSetOption(RPGOPT_TRIWINDBACKWARD, true);

	std::vector<FFXISqleBoneInfo> sqleBones;
	std::vector<Creation_ShapeSkin> sqleSkins[8];
	int fileBoneStarts[8] = {};
	int fileBoneCounts[8] = {};
	for (int fileIndex = 0; fileIndex < fileCount && fileIndex < 8; ++fileIndex)
	{
		fileBoneStarts[fileIndex] = (int)sqleBones.size();
		const float *sceneOffset = meshOffsets ? (meshOffsets + fileIndex * 3) : NULL;
		Creation_ParseSqleSkeleton(fileBuffers[fileIndex], bufferLens[fileIndex], fileIndex,
			fileBoneStarts[fileIndex], sceneOffset, sqleBones);
		fileBoneCounts[fileIndex] = (int)sqleBones.size() - fileBoneStarts[fileIndex];
		Creation_ParseSqleSkins(fileBuffers[fileIndex], bufferLens[fileIndex], sqleSkins[fileIndex]);
	}

	modelBone_t *pCombinedBones = NULL;
	if (!sqleBones.empty())
	{
		pCombinedBones = rapi->Noesis_AllocBones((int)sqleBones.size());
		for (int boneIndex = 0; boneIndex < (int)sqleBones.size(); ++boneIndex)
		{
			const FFXISqleBoneInfo &info = sqleBones[(size_t)boneIndex];
			modelBone_t &bone = pCombinedBones[boneIndex];
			bone.index = boneIndex;
			sprintf_s(bone.name, "sqle_%i_%04i", info.fileIndex, info.sourceBoneIndex);
			RichQuat q(info.bindQuaternion[0], info.bindQuaternion[1],
				info.bindQuaternion[2], info.bindQuaternion[3]);
			RichMat43 local = q.ToMat43(false);
			local[0] = local[0] * info.bindScale[0];
			local[1] = local[1] * info.bindScale[1];
			local[2] = local[2] * info.bindScale[2];
			local[3] = RichVec3(info.bindTranslation);

			// Conjugate by the Y reflection used for creation geometry.  Applying
			// it to each local transform keeps hierarchy multiplication intact.
			static const float signs[3] = { 1.0f, -1.0f, 1.0f };
			for (int row = 0; row < 3; ++row)
				for (int col = 0; col < 3; ++col)
					local[row][col] *= signs[row] * signs[col];
			for (int axis = 0; axis < 3; ++axis)
				local[3][axis] *= signs[axis];
			if (info.parentIndex < 0)
				for (int axis = 0; axis < 3; ++axis)
					local[3][axis] += info.rootOffset[axis];

			(RichMat43 &)bone.mat = local;
			bone.eData.parent = (info.parentIndex >= 0 && info.parentIndex < (int)sqleBones.size()) ?
				pCombinedBones + info.parentIndex : NULL;
		}
		rapi->rpgMultiplyBones(pCombinedBones, (int)sqleBones.size());
		rapi->rpgSetExData_Bones(pCombinedBones, (int)sqleBones.size());
	}

	CArrayList<noesisTex_t *> textures;
	CArrayList<noesisMaterial_t *> materials;
	char opaqueMaterialNames[8][32] = {};
	char alphaMaterialNames[8][32] = {};
	char blackKeyMaterialNames[8][32] = {};
	std::vector<bool> shapeUsesAlpha[8];
	std::vector<bool> shapeUsesBlackKey[8];
	for (int fileIndex = 0; fileIndex < fileCount && fileIndex < 8; ++fileIndex)
	{
		sprintf_s(opaqueMaterialNames[fileIndex], "creation_mat_%i", fileIndex);
		sprintf_s(alphaMaterialNames[fileIndex], "creation_mat_%i_alpha", fileIndex);
		sprintf_s(blackKeyMaterialNames[fileIndex], "creation_mat_%i_black_key", fileIndex);
		if (materialBuffers && materialLens)
		{
			Creation_GetDMBShapeMaterialFlags(materialBuffers[fileIndex], materialLens[fileIndex],
				shapeUsesAlpha[fileIndex], shapeUsesBlackKey[fileIndex]);
			Creation_BuildDMBMaterials(rapi, materialBuffers[fileIndex], materialLens[fileIndex],
				opaqueMaterialNames[fileIndex], alphaMaterialNames[fileIndex],
				blackKeyMaterialNames[fileIndex], textures, materials,
				materialAlphaModes ? materialAlphaModes[fileIndex] : FFXI_CREATION_ALPHA_SOLID);
		}
	}
	if (materials.Num() > 0)
	{
		noesisMatData_t *pMd = rapi->Noesis_GetMatDataFromLists(materials, textures);
		rapi->rpgSetExData_Materials(pMd);
	}

	int renderedShapeCount = 0;
	for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
	{
		BYTE *fileBuffer = fileBuffers[fileIndex];
		const int bufferLen = bufferLens[fileIndex];
		if (!Model_FF11_CheckCreationDAT(fileBuffer, bufferLen, rapi))
			continue;
		const char *opaqueMaterialName = (fileIndex < 8) ? opaqueMaterialNames[fileIndex] : "creation_default";
		const char *alphaMaterialName = (fileIndex < 8) ? alphaMaterialNames[fileIndex] : NULL;
		const char *blackKeyMaterialName = (fileIndex < 8) ? blackKeyMaterialNames[fileIndex] : NULL;
		const std::vector<bool> *alphaFlags = (fileIndex < 8) ? &shapeUsesAlpha[fileIndex] : NULL;
		const std::vector<bool> *blackKeyFlags = (fileIndex < 8) ? &shapeUsesBlackKey[fileIndex] : NULL;
		const float *sceneOffset = meshOffsets ? (meshOffsets + fileIndex * 3) : NULL;
		renderedShapeCount += Creation_RenderDAT(rapi, fileBuffer, bufferLen,
			opaqueMaterialName, alphaMaterialName, blackKeyMaterialName,
			alphaFlags, blackKeyFlags, sceneOffset,
			(fileIndex < 8) ? &sqleSkins[fileIndex] : NULL,
			(fileIndex < 8) ? fileBoneStarts[fileIndex] : 0,
			pCombinedBones, (int)sqleBones.size());
	}

	noesisModel_t *pMdl = NULL;
	if (renderedShapeCount > 0)
	{
		pMdl = rapi->rpgConstructModelAndSort();
		if (pMdl)
			pMdl->sqleBones = sqleBones;
		numMdl = pMdl ? 1 : 0;
	}

	rapi->rpgDestroyContext(pCtx);
	Model_FF11_SetPreviewOffset(rapi);
	return pMdl;
}
