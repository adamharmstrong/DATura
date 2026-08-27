#pragma once

#include "model_ff11.h"

#pragma pack(push, 1)

class CFFXITextureHandler : public CFFXIChunkHandler
{
public:
	static const int skTexNameLength = 16;
	static const int skMaterialNamePad = 64;
	static const char *skpShinySuffix;
	static const char *skpSoftBlendSuffix;
	static const char *skpNoBlendSuffix;
	static const char *skpHardAlphaSuffix;
	static const char *skpStrictHardAlphaSuffix;
	static const char *skpSoftBlendCullBackSuffix;
	static const char *skpNoBlendCullBackSuffix;
	static const char *skpHardAlphaCullBackSuffix;

	struct STexHeader
	{
		unsigned char mType;
		char mName[skTexNameLength];
		unsigned int mVer; //maybe
		int mWidth;
		int mHeight;
		unsigned int mUnknown[6];
		unsigned int mBitsPerPalClr; //unverified - kind of seems more like bits per pixel * 4, since i've only seen 16 for dxt1.
	};

	CFFXITextureHandler()
		: CFFXIChunkHandler(CFFXIDat::skChunkType_Texture)
		, mFlatNormalIndex(-1)
		, mFlatSpecIndex(-1)
	{
	}

	static const int skDefaultColorFixShift = 0;
	static const int skDefaultAlphaFixShift = 2;

	static const int skTextureVersion = 40;
	//texture type is probably only the high 4 bits, but 1 always seems to be set
	static const int skTextureType_DXT = 0xA1;
	static const int skTextureType_Pal = 0x91;
	static const int skTextureType_Pal2 = 0x01; //unsure how this is different from skTextureType_Pal
	static const int skTextureType_PalCombo = 0x81; //paletted followed by dxt
	static const int skTextureType_PalLeadingInt = 0xB1; //preceded by 32 bits, unknown

	virtual CFFXIDat::EValidateChunkResult ValidateChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk,
															const unsigned char *pChunkData, const int dataSize) const
	{
		// Detailed validation happens in HandleChunk, but no texture handler can safely
		// inspect a chunk that does not contain the complete packed header.
		if (!pChunkData || dataSize < (int)sizeof(STexHeader))
		{
			return CFFXIDat::kVCR_Invalid;
		}

		return CFFXIDat::kVCR_Supported;
	}

	static unsigned char *CreateRgbaFromPaletted(noeRAPI_t *pRapi, const unsigned char *pRawPalData, const unsigned char *pPixelData,
													const int width, const int height, const int bitsPerColor)
	{
		//unverified - can bpc be 16 in this context, and if so, does it mean 16-color 8888 or 256-color 5551
		const int palSize = 256 * (bitsPerColor / 8);
		unsigned int *pPalData = (unsigned int *)pRapi->Noesis_ImageDecodeRaw(const_cast<unsigned char *>(pRawPalData), palSize, 256, 1,
			(bitsPerColor == 32) ? b8g8r8a8 : b5g5r5a1);
		unsigned int *pDst = (unsigned int *)pRapi->Noesis_UnpooledAlloc(width * height * 4);
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				const int palIndex = pPixelData[(height - y - 1) * width + x];
				pDst[y * width + x] = pPalData[palIndex];
			}
		}
		pRapi->Noesis_UnpooledFree(pPalData);

		return (unsigned char *)pDst;
	}

	static void ShiftRgbaData(unsigned char *pTexData, const int width, const int height, const int texColorShift, const int texAlphaShift)
	{
		for (int pixelIndex = 0; pixelIndex < width * height; ++pixelIndex)
		{
			unsigned char *pPix = pTexData + pixelIndex * 4;
			if (texColorShift)
			{
				pPix[0] = std::min<int>((int)pPix[0] << texColorShift, 255);
				pPix[1] = std::min<int>((int)pPix[1] << texColorShift, 255);
				pPix[2] = std::min<int>((int)pPix[2] << texColorShift, 255);
			}
			if (texAlphaShift)
			{
				pPix[3] = std::min<int>((int)pPix[3] << texAlphaShift, 255);
			}
		}
	}

	static bool IsWeatherCloudTexture(const char *directoryPath, const char *textureName)
	{
		if (!Model_FF11_IsWeatherDirectory(directoryPath) || !textureName)
			return false;
		char lowerName[skTexNameLength + 1];
		strcpy_s(lowerName, textureName);
		for (int i = 0; lowerName[i]; ++i)
			lowerName[i] = (char)tolower((unsigned char)lowerName[i]);
		return strncmp(lowerName, "clo", 3) == 0 ||
			strncmp(lowerName, "cld", 3) == 0 ||
			strncmp(lowerName, "fine", 4) == 0 ||
			strncmp(lowerName, "suny", 4) == 0 ||
			strstr(lowerName, "kumori") != NULL ||
			strstr(lowerName, "kum1") != NULL;
	}

	static bool IsTitleBackdropDat(const noeRAPI_t *rapi)
	{
		const char *path = rapi ? rapi->GetCurrentFilePath() : nullptr;
		if (!path)
			return false;

		// The custom title screen uses Konschtat (ROM/0/90.DAT) as a composed
		// backdrop. Its cloud layer is an intentional additive title effect;
		// resolving its stipple into continuous alpha makes its broad aurora band
		// nearly solid white.
		char normalizedPath[MAX_NOESIS_PATH] = {};
		for (size_t index = 0; path[index] && index + 1 < sizeof(normalizedPath); ++index)
		{
			const char c = path[index];
			normalizedPath[index] = (c == '/') ? '\\' : (char)tolower((unsigned char)c);
		}
		const char suffix[] = "\\rom\\0\\90.dat";
		const size_t pathLength = strlen(normalizedPath);
		return pathLength >= sizeof(suffix) - 1 &&
			strcmp(normalizedPath + pathLength - (sizeof(suffix) - 1), suffix) == 0;
	}

	static bool IsDxt1StippleAlpha(const unsigned char *rgba, const size_t pixelCount)
	{
		bool hasTransparent = false;
		bool hasOpaque = false;
		for (size_t pixel = 0; pixel < pixelCount; ++pixel)
		{
			const unsigned char alpha = rgba[pixel * 4 + 3];
			if (alpha == 0)
				hasTransparent = true;
			else if (alpha == 255)
				hasOpaque = true;
			else
				return false;
		}
		return hasTransparent && hasOpaque;
	}

	static bool IsDxt3OpaqueDither(const unsigned char *rgba, const size_t pixelCount)
	{
		bool hasDitheredOpaque = false;
		for (size_t pixel = 0; pixel < pixelCount; ++pixel)
		{
			const unsigned char alpha = rgba[pixel * 4 + 3];
			if (alpha == 119 || alpha == 136)
				hasDitheredOpaque = true;
			else if (alpha != 0)
				return false;
		}
		return hasDitheredOpaque;
	}

	static void ReconstructWeatherCloudAlpha(unsigned char *rgba, const int width,
		const int height, const bool fillDxt1ColorHoles, const bool normalizeDxt3Alpha)
	{
		if (!rgba || width <= 0 || height <= 0)
			return;
		const size_t pixelCount = (size_t)width * (size_t)height;
		std::vector<unsigned char> source(rgba, rgba + pixelCount * 4);
		const bool filterDxt1Stipple = fillDxt1ColorHoles && IsDxt1StippleAlpha(source.data(), pixelCount);
		const bool filterDxt3Opaque = normalizeDxt3Alpha && IsDxt3OpaqueDither(source.data(), pixelCount);
		if (!filterDxt1Stipple && !filterDxt3Opaque)
			return;

		// DXT1 transparent indices decode as transparent black. Fill their colour
		// from nearby opaque texels before mip generation to prevent dark fringes.
		if (filterDxt1Stipple)
		{
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					const size_t pixel = (size_t)y * width + x;
					const unsigned char *srcPixel = &source[pixel * 4];
					if (srcPixel[3] >= 8)
						continue;
					int sums[3] = {};
					int alphaWeight = 0;
					for (int dy = -2; dy <= 2; ++dy)
					{
						const int sampleY = (y + dy + height) % height;
						for (int dx = -2; dx <= 2; ++dx)
						{
							const int sampleX = (x + dx + width) % width;
							const unsigned char *neighbor = &source[((size_t)sampleY * width + sampleX) * 4];
							if (neighbor[3] == 0)
								continue;
							sums[0] += neighbor[0] * neighbor[3];
							sums[1] += neighbor[1] * neighbor[3];
							sums[2] += neighbor[2] * neighbor[3];
							alphaWeight += neighbor[3];
						}
					}
					if (alphaWeight > 0)
					{
						rgba[pixel * 4 + 0] = (unsigned char)((sums[0] + alphaWeight / 2) / alphaWeight);
						rgba[pixel * 4 + 1] = (unsigned char)((sums[1] + alphaWeight / 2) / alphaWeight);
						rgba[pixel * 4 + 2] = (unsigned char)((sums[2] + alphaWeight / 2) / alphaWeight);
					}
				}
			}
		}

		// The PS2 cloud masks are spatial stipple patterns, not a grid intended
		// to remain visible. Do this only for the decoded DXT1 0/255 pattern or
		// the DXT3 {0,119,136} half-range signature; other sky assets retain their
		// authored alpha shape. A wrapping 5x5 density filter avoids edge seams.
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				int alphaSum = 0;
				for (int dy = -2; dy <= 2; ++dy)
				{
					const int sampleY = (y + dy + height) % height;
					for (int dx = -2; dx <= 2; ++dx)
					{
						const int sampleX = (x + dx + width) % width;
						alphaSum += source[((size_t)sampleY * width + sampleX) * 4 + 3];
					}
				}
				int filteredAlpha = (alphaSum + 12) / 25;
				if (filterDxt3Opaque)
					filteredAlpha = std::min(255, (filteredAlpha * 255 + 68) / 136);
				rgba[((size_t)y * width + x) * 4 + 3] = (unsigned char)filteredAlpha;
			}
		}
	}

	virtual bool HandleChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk,
								const unsigned char *pChunkData, const int dataSize)
	{
		// FFXI's client-facing texture path appears to use the embedded DXT block payload when a
		// combo texture contains both palette and compressed versions. Keep the palette
		// path available for source-data inspection, but default to the client-like path.
		const bool preferDxtToPalette = !(gpFF11Opts && gpFF11Opts->preferPaletteOverDxt);
		const int texColorShift = (gpFF11Opts && gpFF11Opts->explicitColorShift) ? gpFF11Opts->fixColorShift : skDefaultColorFixShift;
		const int texAlphaShift = (gpFF11Opts && gpFF11Opts->explicitAlphaShift) ? gpFF11Opts->fixAlphaShift : skDefaultAlphaFixShift;
		const bool fixAlphaOrColor = (texColorShift || texAlphaShift);
		noeRAPI_t *pRapi = dat.GetRAPI();

		if (!pChunkData || dataSize < (int)sizeof(STexHeader))
		{
			if (pRapi)
				pRapi->LogOutput("WARNING: Texture chunk is smaller than its header. Skipping.\n");
			return true;
		}

		const STexHeader *pTexHdr = (const STexHeader *)pChunkData;
		char texName[skTexNameLength + skMaterialNamePad];
		memcpy(texName, pTexHdr->mName, skTexNameLength);
		texName[skTexNameLength] = 0;
		const char *directoryPath = dat.GetChunkDirectoryPath(chunk);
		const bool weatherCloudTexture = IsWeatherCloudTexture(directoryPath, texName) &&
			!IsTitleBackdropDat(pRapi);
		if (pTexHdr->mWidth <= 0 || pTexHdr->mWidth > 4096 ||
			pTexHdr->mHeight <= 0 || pTexHdr->mHeight > 4096)
		{
			if (pRapi)
				pRapi->LogOutput("WARNING: Texture has invalid dimensions %d x %d. Skipping.\n",
					pTexHdr->mWidth, pTexHdr->mHeight);
			return true;
		}

		bool copyFromSource = false;
		unsigned char *pSrcData = (unsigned char *)(pTexHdr + 1);
		int srcDataSize = dataSize - sizeof(STexHeader);
		unsigned char *pTexData = NULL;
		int texDataSize = 0;
		noesisTexType_e texType = NOESISTEX_UNKNOWN;
		noesisTexType_e encodedTexType = NOESISTEX_UNKNOWN;
		if (!pRapi)
			return false;

		// Validate and sanitise palette color depth.
		// mBitsPerPalClr is labelled "unverified" in the original code, so guard against
		// garbage values.  Acceptable values are 16 (B5G5R5A1) and 32 (B8G8R8A8).
		// If the field reads anything else, default to 32.
		const unsigned int rawBpc = pTexHdr->mBitsPerPalClr;
		const unsigned int safeBpc = (rawBpc == 16 || rawBpc == 32) ? rawBpc : 32u;
		const int palSize = 256 * ((int)safeBpc / 8);  // 512 or 1024 bytes

		switch (pTexHdr->mType)
		{
		case skTextureType_PalCombo:
			{
				const size_t pixelCount = (size_t)pTexHdr->mWidth * (size_t)pTexHdr->mHeight;
				const size_t palTextureSize = (size_t)palSize + pixelCount;
				if (palTextureSize > (size_t)srcDataSize)
				{
					pRapi->LogOutput("WARNING: Combination texture is too small for its palette copy. Skipping.\n");
					return true;
				}

				if (!preferDxtToPalette)
				{
					goto PickPalOverDXT;
				}
				else
				{
					// Skip the palette/index copy and use the client-like compressed copy.
					pSrcData += palTextureSize;
					srcDataSize -= (int)palTextureSize;
				}
			}
			//fall through intentionally in the else case
		case skTextureType_DXT:
			{
				static const int kDxtMiniHeaderSize = 12;
				if (srcDataSize < kDxtMiniHeaderSize)
				{
					pRapi->LogOutput("WARNING: DXT texture is missing its 12-byte mini-header. Skipping.\n");
					return true;
				}

				// The tag is stored in FFXI's byte order (for example, bytes "3TXD"
				// compare equal to MSVC's 'DXT3' multi-character constant). Use memcpy
				// because the packed mini-header is not guaranteed to be int-aligned.
				int dxtType = 0;
				memcpy(&dxtType, pSrcData, sizeof(dxtType));
				int blockSize = 0;
				switch (dxtType)
				{
				case 'DXT1':
					texType = NOESISTEX_DXT1;
					blockSize = 8;
					break;
				case 'DXT3':
					texType = NOESISTEX_DXT3;
					blockSize = 16;
					break;
				case 'DXT5':
					texType = NOESISTEX_DXT5;
					blockSize = 16;
					break;
				default:
					break;
				}
				if (texType != NOESISTEX_UNKNOWN)
				{
					encodedTexType = texType;
					const size_t blockColumns = ((size_t)pTexHdr->mWidth + 3) / 4;
					const size_t blockRows = ((size_t)pTexHdr->mHeight + 3) / 4;
					const size_t expectedDxtSize = blockColumns * blockRows * (size_t)blockSize;
					const size_t availableDxtSize = (size_t)(srcDataSize - kDxtMiniHeaderSize);
					if (expectedDxtSize > availableDxtSize || expectedDxtSize > (size_t)INT_MAX)
					{
						pRapi->LogOutput("WARNING: Truncated DXT texture (need %zu block bytes, have %zu). Skipping.\n",
							expectedDxtSize, availableDxtSize);
						return true;
					}

					const bool convertDxtForFix = weatherCloudTexture ||
						(texColorShift != 0) ||
						((gpFF11Opts && gpFF11Opts->explicitAlphaShift) && texAlphaShift != 0);
					if (convertDxtForFix)
					{
						//it wouldn't be too much work to just shift the 4-bit alphas in the dxt3 blocks, but, fuck it.
						pTexData = pRapi->Noesis_ConvertDXT(pTexHdr->mWidth, pTexHdr->mHeight,
							const_cast<unsigned char *>(pSrcData) + kDxtMiniHeaderSize, texType);
						if (!pTexData)
						{
							pRapi->LogOutput("WARNING: Unable to decode DXT texture. Skipping.\n");
							return true;
						}
						const bool explicitAlphaShift = gpFF11Opts && gpFF11Opts->explicitAlphaShift;
						const int dxtAlphaShift = (weatherCloudTexture && !explicitAlphaShift) ? 0 : texAlphaShift;
						ShiftRgbaData(pTexData, pTexHdr->mWidth, pTexHdr->mHeight, texColorShift, dxtAlphaShift);
						texDataSize = pTexHdr->mWidth * pTexHdr->mHeight * 4;
						texType = NOESISTEX_RGBA32;
					}
					else
					{
						copyFromSource = true;
						pTexData = const_cast<unsigned char *>(pSrcData) + kDxtMiniHeaderSize;
						// DAT chunks are aligned and may contain trailing padding. Retain exactly
						// the source top level; the D3D9 uploader generates lower levels separately.
						texDataSize = (int)expectedDxtSize;
					}
				}
				else
				{
					pRapi->LogOutput("WARNING: Unknown DXT texture type.\n");
				}
			}
			break;

		case skTextureType_PalLeadingInt:
			//not sure what this is used for
			if (srcDataSize < (int)sizeof(int))
			{
				pRapi->LogOutput("WARNING: Palette texture is missing its leading value. Skipping.\n");
				return true;
			}
			pSrcData += sizeof(int);
			srcDataSize -= sizeof(int);
			//intentionally fall through
		case skTextureType_Pal:
		case skTextureType_Pal2:
		PickPalOverDXT:
			// Guard: pixel data must fit within the remaining chunk
			if ((size_t)palSize + (size_t)pTexHdr->mWidth * (size_t)pTexHdr->mHeight > (size_t)srcDataSize)
			{
				pRapi->LogOutput("WARNING: Texture chunk too small for palette+pixels. Skipping.\n");
				return true;
			}
			pTexData = CreateRgbaFromPaletted(pRapi, pSrcData, pSrcData + palSize, pTexHdr->mWidth, pTexHdr->mHeight, (int)safeBpc);
			if (fixAlphaOrColor)
			{
				ShiftRgbaData(pTexData, pTexHdr->mWidth, pTexHdr->mHeight, texColorShift, texAlphaShift);
			}
			texDataSize = pTexHdr->mWidth * pTexHdr->mHeight * 4;
			texType = NOESISTEX_RGBA32;
			break;
		}

		if (!pTexData)
		{
			//just use a stub if something went wrong
			texDataSize = pTexHdr->mWidth * pTexHdr->mHeight * 4;
			pTexData = (unsigned char *)pRapi->Noesis_UnpooledAlloc(texDataSize);
			memset(pTexData, 0, texDataSize);
			texType = NOESISTEX_RGBA32;
		}
		else if (copyFromSource)
		{
			NoeAssert(texDataSize > 0);
			const unsigned char *pSourceData = pTexData;
			pTexData = (unsigned char *)pRapi->Noesis_UnpooledAlloc(texDataSize);
			memcpy(pTexData, pSourceData, texDataSize);
		}

		if (weatherCloudTexture && texType == NOESISTEX_RGBA32)
			ReconstructWeatherCloudAlpha(pTexData, pTexHdr->mWidth, pTexHdr->mHeight,
				encodedTexType == NOESISTEX_DXT1,
				encodedTexType == NOESISTEX_DXT3 && !(gpFF11Opts && gpFF11Opts->explicitAlphaShift));

		char scopedTexName[skTexNameLength + skMaterialNamePad];
		Model_FF11_BuildScopedResourceName(scopedTexName, sizeof(scopedTexName),
			directoryPath, texName);

		//allocate flat normal and spec texture in case we haven't yet, used for shiny material
		if (mFlatNormalIndex == -1 && (!gpFF11Opts || !gpFF11Opts->noShinyMaterials))
		{
			NoeAssert(mFlatSpecIndex == -1);
			unsigned char *pFlatNormalData = (unsigned char *)pRapi->Noesis_UnpooledAlloc(4 * 4 * 4);
			for (int pixelIndex = 0; pixelIndex < 4 * 4; ++pixelIndex)
			{
				unsigned char *pPixel = pFlatNormalData + pixelIndex * 4;
				pPixel[0] = 127;
				pPixel[1] = 127;
				pPixel[2] = 255;
				pPixel[3] = 255;
			}
			noesisTex_t *pFlatNormalTex = pRapi->Noesis_TextureAlloc("__flat_normal", 4, 4, pFlatNormalData, NOESISTEX_RGBA32);
			mFlatNormalIndex = mTextures.Num();
			pFlatNormalTex->shouldFreeData = true;
			mTextures.Append(pFlatNormalTex);

			unsigned char *pFlatSpecData = (unsigned char *)pRapi->Noesis_UnpooledAlloc(4 * 4 * 4);
			memset(pFlatSpecData, 0xFF, 4 * 4 * 4);
			noesisTex_t *pFlatSpecTex = pRapi->Noesis_TextureAlloc("__flat_spec", 4, 4, pFlatSpecData, NOESISTEX_RGBA32);
			mFlatSpecIndex = mTextures.Num();
			pFlatSpecTex->shouldFreeData = true;
			mTextures.Append(pFlatSpecTex);
		}

		noesisTex_t *pTex = pRapi->Noesis_TextureAllocEx(scopedTexName, pTexHdr->mWidth, pTexHdr->mHeight, pTexData, texDataSize, texType, 0, 0);
		pTex->shouldFreeData = true;
		const int baseTexIndex = mTextures.Num();

		const int defaultMtlFlags = (gpFF11Opts && gpFF11Opts->forceCull) ? 0 : NMATFLAG_TWOSIDED;

		noesisMaterial_t *pMat = pRapi->Noesis_GetMaterialList(1, true);
		pMat->name = pRapi->Noesis_PooledString(scopedTexName);
		pMat->texIdx = baseTexIndex;
		pMat->flags = defaultMtlFlags;
		// Non-map models still use the base material directly, so preserve their
		// historical alpha-test default. Map geometry selects an explicit variant.
		pMat->noDefaultBlend = true;
		pMat->alphaTest = 0.5f;

		char mtlVariantName[skTexNameLength + skMaterialNamePad];

		//add a material variant for shiny stuff
		if (mFlatNormalIndex >= 0 && mFlatSpecIndex >= 0)
		{
			noesisMaterial_t *pMatShiny = pRapi->Noesis_GetMaterialList(1, true);
		sprintf_s(mtlVariantName, "%s%s", scopedTexName, skpShinySuffix);
			pMatShiny->name = pRapi->Noesis_PooledString(mtlVariantName);
			pMatShiny->texIdx = baseTexIndex;
			pMatShiny->flags = defaultMtlFlags;
			pMatShiny->noDefaultBlend = true;
			pMatShiny->alphaTest = 0.5f;
			pMatShiny->normalTexIdx = mFlatNormalIndex;
			pMatShiny->specularTexIdx = mFlatSpecIndex;
			pMatShiny->specular[0] = 0.5f;
			pMatShiny->specular[1] = 0.5f;
			pMatShiny->specular[2] = 0.5f;
			pMatShiny->specular[3] = 64.0f;
			mMaterials.Append(pMatShiny);
		}

		// Zone draw records carry both blend and cull state. Material variants preserve
		// that state until the D3D9 renderer consumes the constructed submesh.
		noesisMaterial_t *pMatSoftBlend = pRapi->Noesis_GetMaterialList(1, true);
		sprintf_s(mtlVariantName, "%s%s", scopedTexName, skpSoftBlendSuffix);
		pMatSoftBlend->name = pRapi->Noesis_PooledString(mtlVariantName);
		pMatSoftBlend->texIdx = baseTexIndex;
		pMatSoftBlend->flags = defaultMtlFlags;
		pMatSoftBlend->noDefaultBlend = false;
		mMaterials.Append(pMatSoftBlend);
		noesisMaterial_t *pMatNoBlend = pRapi->Noesis_GetMaterialList(1, true);
		sprintf_s(mtlVariantName, "%s%s", scopedTexName, skpNoBlendSuffix);
		pMatNoBlend->name = pRapi->Noesis_PooledString(mtlVariantName);
		pMatNoBlend->texIdx = baseTexIndex;
		pMatNoBlend->flags = defaultMtlFlags;
		pMatNoBlend->noDefaultBlend = true;
		mMaterials.Append(pMatNoBlend);
		noesisMaterial_t *pMatHardAlpha = pRapi->Noesis_GetMaterialList(1, true);
		sprintf_s(mtlVariantName, "%s%s", scopedTexName, skpHardAlphaSuffix);
		pMatHardAlpha->name = pRapi->Noesis_PooledString(mtlVariantName);
		pMatHardAlpha->texIdx = baseTexIndex;
		pMatHardAlpha->flags = defaultMtlFlags;
		pMatHardAlpha->noDefaultBlend = true;
		pMatHardAlpha->alphaTest = 0.375f;
		mMaterials.Append(pMatHardAlpha);
		noesisMaterial_t *pMatStrictHardAlpha = pRapi->Noesis_GetMaterialList(1, true);
		sprintf_s(mtlVariantName, "%s%s", scopedTexName, skpStrictHardAlphaSuffix);
		pMatStrictHardAlpha->name = pRapi->Noesis_PooledString(mtlVariantName);
		pMatStrictHardAlpha->texIdx = baseTexIndex;
		pMatStrictHardAlpha->flags = defaultMtlFlags;
		pMatStrictHardAlpha->noDefaultBlend = true;
		pMatStrictHardAlpha->alphaTest = 0.375f;
		mMaterials.Append(pMatStrictHardAlpha);

		noesisMaterial_t *pMatSoftBlendCullBack = pRapi->Noesis_GetMaterialList(1, true);
		sprintf_s(mtlVariantName, "%s%s", scopedTexName, skpSoftBlendCullBackSuffix);
		pMatSoftBlendCullBack->name = pRapi->Noesis_PooledString(mtlVariantName);
		pMatSoftBlendCullBack->texIdx = baseTexIndex;
		pMatSoftBlendCullBack->flags = 0;
		pMatSoftBlendCullBack->noDefaultBlend = false;
		mMaterials.Append(pMatSoftBlendCullBack);
		noesisMaterial_t *pMatNoBlendCullBack = pRapi->Noesis_GetMaterialList(1, true);
		sprintf_s(mtlVariantName, "%s%s", scopedTexName, skpNoBlendCullBackSuffix);
		pMatNoBlendCullBack->name = pRapi->Noesis_PooledString(mtlVariantName);
		pMatNoBlendCullBack->texIdx = baseTexIndex;
		pMatNoBlendCullBack->flags = 0;
		pMatNoBlendCullBack->noDefaultBlend = true;
		mMaterials.Append(pMatNoBlendCullBack);
		noesisMaterial_t *pMatHardAlphaCullBack = pRapi->Noesis_GetMaterialList(1, true);
		sprintf_s(mtlVariantName, "%s%s", scopedTexName, skpHardAlphaCullBackSuffix);
		pMatHardAlphaCullBack->name = pRapi->Noesis_PooledString(mtlVariantName);
		pMatHardAlphaCullBack->texIdx = baseTexIndex;
		pMatHardAlphaCullBack->flags = 0;
		pMatHardAlphaCullBack->noDefaultBlend = true;
		pMatHardAlphaCullBack->alphaTest = 0.375f;
		mMaterials.Append(pMatHardAlphaCullBack);

		mTextures.Append(pTex);
		mMaterials.Append(pMat);

		return true;
	}

	CArrayList<noesisTex_t *> &Textures() { return mTextures; }
	CArrayList<noesisMaterial_t *> &Materials() { return mMaterials; }

protected:
	CArrayList<noesisTex_t *> mTextures;
	CArrayList<noesisMaterial_t *> mMaterials;
	int mFlatNormalIndex;
	int mFlatSpecIndex;
};

const char *CFFXITextureHandler::skpShinySuffix = "_explicitshiny";
const char *CFFXITextureHandler::skpSoftBlendSuffix = "_explicitsoftblend";
const char *CFFXITextureHandler::skpNoBlendSuffix = "_explicitnoblend";
const char *CFFXITextureHandler::skpHardAlphaSuffix = "_explicithardalpha";
const char *CFFXITextureHandler::skpStrictHardAlphaSuffix = "_explicitstricthardalpha";
const char *CFFXITextureHandler::skpSoftBlendCullBackSuffix = "_explicitsoftblend_cullback";
const char *CFFXITextureHandler::skpNoBlendCullBackSuffix = "_explicitnoblend_cullback";
const char *CFFXITextureHandler::skpHardAlphaCullBackSuffix = "_explicithardalpha_cullback";

//========================================================================================

#pragma pack(pop)
