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

static void Creation_EmitStrip(noeRAPI_t *pRapi, const unsigned char *pData, const int dataSize,
	const int posOfs, const int normalOfs, const int uvOfs,
	const std::vector<int> &strip, const float *sceneOffset)
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
		pRapi->rpgVertex3f(pos);
	}
	pRapi->rpgEnd();
}

static void Creation_EmitTriangleList(noeRAPI_t *pRapi, const unsigned char *pData, const int dataSize,
	const int posOfs, const int normalOfs, const int uvOfs,
	const std::vector<int> &indices, const float *sceneOffset)
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
		pRapi->rpgVertex3f(pos);
	}
	pRapi->rpgEnd();
}

static bool Creation_RenderShape(noeRAPI_t *pRapi, const unsigned char *pData, const int dataSize,
	const int shapeOfs, const char *materialName, const float *sceneOffset)
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
					Creation_EmitTriangleList(pRapi, pData, dataSize, posOfs, normalOfs, uvOfs, triIndices, sceneOffset);
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
		Creation_EmitStrip(pRapi, pData, dataSize, posOfs, normalOfs, uvOfs, strip, sceneOffset);
	}

	return true;
}

static int Creation_RenderDAT(noeRAPI_t *rapi, BYTE *fileBuffer, int bufferLen,
	const char *materialName, const float *sceneOffset = NULL)
{
	int renderedShapeCount = 0;
	for (int shapeOfs = 0; shapeOfs + 0x60 < bufferLen;)
	{
		if (!Creation_IsShapeBlock(fileBuffer, bufferLen, shapeOfs))
			break;

		if (Creation_RenderShape(rapi, fileBuffer, bufferLen, shapeOfs, materialName, sceneOffset))
			++renderedShapeCount;

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

static bool Creation_UseSourceAlphaForPixel(const int alphaMode,
	const int x, const int y, const int width, const int height)
{
	switch (alphaMode)
	{
	case FFXI_CREATION_ALPHA_HUMANOID_HEAD:
		return y < (height / 2) &&
			!(x >= (width * 3 / 4) && y < (height / 4));
	case FFXI_CREATION_ALPHA_ELVAAN_F_HEAD:
		return y < (height / 2);
	case FFXI_CREATION_ALPHA_TARU_HEAD:
		return y < (height / 2) &&
			!(x >= (width / 2) && x < (width * 3 / 4) &&
			  y >= (height / 4) && y < (height / 2));
	case FFXI_CREATION_ALPHA_MITHRA_HEAD:
		return x >= (width / 2) &&
			!(x >= (width * 3 / 4) && y >= (height * 3 / 4));
	case FFXI_CREATION_ALPHA_GALKA_HEAD:
		return x >= (width * 3 / 4);
	default:
		return false;
	}
}

static bool Creation_IsMatteBlackCutoutPixel(const unsigned char *pPixel)
{
	return pPixel[3] >= 250 &&
		pPixel[0] <= 3 && pPixel[1] <= 3 && pPixel[2] <= 3;
}

static bool Creation_IsSolidMatteBlackPatch(const unsigned char *pixels,
	const int pixOfs, const int bytesPerPixel, const int width, const int height,
	const int x, const int y)
{
	if (bytesPerPixel <= 3 || x < 2 || y < 2 || x >= width - 2 || y >= height - 2)
		return false;

	for (int py = y - 2; py <= y + 2; ++py)
	{
		for (int px = x - 2; px <= x + 2; ++px)
		{
			const unsigned char *pPixel = pixels + pixOfs + (py * width + px) * bytesPerPixel;
			if (!Creation_IsMatteBlackCutoutPixel(pPixel))
				return false;
		}
	}
	return true;
}

static noesisMaterial_t *Creation_BuildDMBMaterial(noeRAPI_t *rapi,
	BYTE *materialBuffer, int materialLen, const char *materialName,
	CArrayList<noesisTex_t *> &textures, const int alphaMode)
{
	if (!materialBuffer || materialLen <= 0)
		return NULL;

	const int texBlockOfs = Creation_FindDMBTextureBlock(materialBuffer, materialLen);
	if (texBlockOfs < 0)
		return NULL;

	const int width = Creation_ReadI32(materialBuffer, materialLen, texBlockOfs + 0x40);
	const int height = Creation_ReadI32(materialBuffer, materialLen, texBlockOfs + 0x44);
	const int bytesPerPixel = Creation_ReadI32(materialBuffer, materialLen, texBlockOfs + 0x48);
	const int pixOfs = texBlockOfs + 0x60;

	unsigned char *rgba = (unsigned char *)rapi->Noesis_UnpooledAlloc(width * height * 4);
	for (int y = 0; y < height; ++y)
	{
		const unsigned char *src = materialBuffer + pixOfs + y * width * bytesPerPixel;
		for (int x = 0; x < width; ++x)
		{
			const unsigned char *srcPixel = src + x * bytesPerPixel;
			unsigned char *dst = rgba + (y * width + x) * 4;
			dst[0] = srcPixel[2];
			dst[1] = srcPixel[1];
			dst[2] = srcPixel[0];
			const bool greenKey =
				srcPixel[1] > 220 && srcPixel[0] < 80 && srcPixel[2] < 80;
			const bool matteBlackKey =
				Creation_IsSolidMatteBlackPatch(materialBuffer, pixOfs, bytesPerPixel, width, height, x, y);
			if (greenKey)
				dst[3] = 0;
			else if (alphaMode == FFXI_CREATION_ALPHA_BODY_CUTOUT && bytesPerPixel > 3)
				dst[3] = matteBlackKey ? 0 : 255;
			else if (bytesPerPixel > 3 &&
				Creation_UseSourceAlphaForPixel(alphaMode, x, y, width, height))
				dst[3] = (unsigned char)std::min<int>((int)srcPixel[3] << 2, 255);
			else
				dst[3] = 255;
		}
	}

	char texName[64];
	sprintf_s(texName, "%s_tex", materialName ? materialName : "creation_default");
	noesisTex_t *pTex = rapi->Noesis_TextureAlloc(texName, width, height, rgba, NOESISTEX_RGBA32);
	pTex->shouldFreeData = true;

	noesisMaterial_t *pMat = rapi->Noesis_GetMaterialList(1, true);
	pMat->name = rapi->Noesis_PooledString(materialName ? materialName : "creation_default");
	pMat->texIdx = textures.Num();
	pMat->flags = NMATFLAG_TWOSIDED;
	pMat->noDefaultBlend = true;
	pMat->alphaTest = alphaMode != FFXI_CREATION_ALPHA_SOLID ? 0.08f : 0.5f;

	textures.Append(pTex);
	return pMat;
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

	CArrayList<noesisTex_t *> textures;
	CArrayList<noesisMaterial_t *> materials;
	char materialNames[8][32] = {};
	for (int fileIndex = 0; fileIndex < fileCount && fileIndex < 8; ++fileIndex)
	{
		sprintf_s(materialNames[fileIndex], "creation_mat_%i", fileIndex);
		noesisMaterial_t *pMat = NULL;
		if (materialBuffers && materialLens)
			pMat = Creation_BuildDMBMaterial(rapi, materialBuffers[fileIndex], materialLens[fileIndex],
				materialNames[fileIndex], textures,
				materialAlphaModes ? materialAlphaModes[fileIndex] : FFXI_CREATION_ALPHA_SOLID);
		if (pMat)
			materials.Append(pMat);
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
		const char *materialName = (fileIndex < 8) ? materialNames[fileIndex] : "creation_default";
		const float *sceneOffset = meshOffsets ? (meshOffsets + fileIndex * 3) : NULL;
		renderedShapeCount += Creation_RenderDAT(rapi, fileBuffer, bufferLen, materialName, sceneOffset);
	}

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
