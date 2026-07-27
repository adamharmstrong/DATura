/*========================================================================================
 Noesis SDK shim — noeRAPI_t implementation
 Geometry accumulator, D3D9 buffer factory, texture upload, utility helpers.
========================================================================================*/

#include "stdafx.h"
#include "noesis_rapi.h"
#include <stdarg.h>
#include <algorithm>

//========================================================================================
// Construction / destruction
//========================================================================================

noeRAPI_t::noeRAPI_t(IDirect3DDevice9 *pDevice)
    : mpDevice(pDevice)
    , mTextureCompressionEnabled(true)
    , mPrimType(RPGEO_TRIANGLE)
    , mInPrimitive(false)
    , mTriWindBackward(false)
    , mHasTransform(false)
    , mpStagedBones(nullptr)
    , mStagedBoneCount(0)
    , mpStagedAnim(nullptr)
    , mpStagedMatData(nullptr)
{
    mCurrentFilePath[0] = '\0';
}

noeRAPI_t::~noeRAPI_t()
{
    // Models and material-data containers are allocated by this RAPI and share
    // the texture/material objects owned by the pools below. Release the model
    // shells and pointer arrays first, while those referenced objects still live.
    std::vector<noesisMatData_t *> matDataPool;
    for (noesisModel_t *model : mModelPool)
    {
        if (!model)
            continue;
        model->ReleaseD3DBuffers();
        if (model->pMatData &&
            std::find(matDataPool.begin(), matDataPool.end(), model->pMatData) == matDataPool.end())
        {
            matDataPool.push_back(model->pMatData);
        }
        delete model;
    }
    for (noesisMatData_t *matData : matDataPool)
    {
        delete[] matData->mats;
        delete[] matData->textures;
        delete matData;
    }

    // Free unmanaged allocations
    for (void *p : mAllocs)
        free(p);

    // Free pooled strings
    for (char *s : mStringPool)
        free(s);

    // Textures (release D3D resources too)
    for (noesisTex_t *t : mTexPool)
    {
        if (t->pD3DTex) { t->pD3DTex->Release(); t->pD3DTex = nullptr; }
        if (t->shouldFreeData && t->data) free(t->data);
        if (t->name) free(t->name);
        delete t;
    }

    // Materials
    for (noesisMaterial_t *m : mMatPool)
        delete m;
}

//========================================================================================
// Current-file tracking
//========================================================================================

void noeRAPI_t::SetCurrentFilePath(const char *path)
{
    strncpy_s(mCurrentFilePath, path, MAX_NOESIS_PATH - 1);
}

//========================================================================================
// Memory management
//========================================================================================

void *noeRAPI_t::Noesis_UnpooledAlloc(int size)
{
    void *p = malloc(size);
    mAllocs.push_back(p);
    return p;
}

void noeRAPI_t::Noesis_UnpooledFree(void *ptr)
{
    if (!ptr) return;
    auto it = std::find(mAllocs.begin(), mAllocs.end(), ptr);
    if (it != mAllocs.end())
        mAllocs.erase(it);
    free(ptr);
}

char *noeRAPI_t::Noesis_PooledString(const char *str)
{
    if (!str) return nullptr;
    size_t len = strlen(str) + 1;
    char *copy = (char *)malloc(len);
    memcpy(copy, str, len);
    mStringPool.push_back(copy);
    return copy;
}

modelBone_t *noeRAPI_t::Noesis_AllocBones(int count)
{
    modelBone_t *pBones = new modelBone_t[count]();
    mAllocs.push_back(pBones);
    return pBones;
}

//========================================================================================
// Texture helpers — software pixel decoding
//========================================================================================

// Expand a single B8G8R8A8 palette entry to RGBA32 (output order R G B A).
static unsigned int DecodeB8G8R8A8(const unsigned char *p)
{
    // Input: B G R A  →  Output as DWORD: 0xAARRGGBB (D3DCOLOR)
    return D3DCOLOR_ARGB(p[3], p[2], p[1], p[0]);
}

// Expand a single B5G5R5A1 palette entry to RGBA32.
static unsigned int DecodeB5G5R5A1(unsigned short raw)
{
    const unsigned char b = (unsigned char)(( raw        & 0x1F) << 3);
    const unsigned char g = (unsigned char)(((raw >>  5) & 0x1F) << 3);
    const unsigned char r = (unsigned char)(((raw >> 10) & 0x1F) << 3);
    const unsigned char a = (unsigned char)(((raw >> 15) & 0x01) ? 255 : 0);
    return D3DCOLOR_ARGB(a, r, g, b);
}

unsigned char *noeRAPI_t::Noesis_ImageDecodeRaw(unsigned char *data, int dataSize,
                                                  int w, int h, int format)
{
    const int pixelCount = w * h;
    unsigned int *pOut = (unsigned int *)malloc(pixelCount * 4);
    mAllocs.push_back(pOut);

    if (format == b8g8r8a8)
    {
        const unsigned char *pSrc = data;
        for (int i = 0; i < pixelCount; ++i, pSrc += 4)
            pOut[i] = DecodeB8G8R8A8(pSrc);
    }
    else if (format == b5g5r5a1)
    {
        const unsigned short *pSrc = (const unsigned short *)data;
        for (int i = 0; i < pixelCount; ++i)
            pOut[i] = DecodeB5G5R5A1(pSrc[i]);
    }
    else
    {
        memset(pOut, 0xFF, pixelCount * 4);
    }

    return (unsigned char *)pOut;
}

//--- DXT software decompressor ---------------------------------------------------

static void UnpackDXT1Block(const unsigned char *pBlock,
                             unsigned int *pOut, int outW, int outH,
                             int blockX, int blockY, int imageW,
                             bool allowOneBitAlpha = true)
{
    unsigned short c0raw = *(const unsigned short *)(pBlock);
    unsigned short c1raw = *(const unsigned short *)(pBlock + 2);
    unsigned int   bits  = *(const unsigned int  *)(pBlock + 4);

    // Unpack 5-6-5 colours
    float r0 = ((c0raw >> 11) & 0x1F) / 31.0f;
    float g0 = ((c0raw >>  5) & 0x3F) / 63.0f;
    float b0 = ( c0raw        & 0x1F) / 31.0f;
    float r1 = ((c1raw >> 11) & 0x1F) / 31.0f;
    float g1 = ((c1raw >>  5) & 0x3F) / 63.0f;
    float b1 = ( c1raw        & 0x1F) / 31.0f;

    for (int py = 0; py < 4; ++py)
    {
        for (int px = 0; px < 4; ++px)
        {
            const int dstX = blockX + px;
            const int dstY = blockY + py;
            if (dstX >= imageW || dstY >= outH) continue;

            const int code = (bits >> ((py * 4 + px) * 2)) & 0x3;
            float r, g, b;
            unsigned char a = 255;

            if (c0raw > c1raw || !allowOneBitAlpha)
            {
                switch (code)
                {
                case 0: r=r0; g=g0; b=b0; break;
                case 1: r=r1; g=g1; b=b1; break;
                case 2: r=r0*2/3+r1*1/3; g=g0*2/3+g1*1/3; b=b0*2/3+b1*1/3; break;
                default:r=r0*1/3+r1*2/3; g=g0*1/3+g1*2/3; b=b0*1/3+b1*2/3; break;
                }
            }
            else
            {
                switch (code)
                {
                case 0: r=r0; g=g0; b=b0; break;
                case 1: r=r1; g=g1; b=b1; break;
                case 2: r=(r0+r1)*0.5f; g=(g0+g1)*0.5f; b=(b0+b1)*0.5f; break;
                default:r=g=b=0; a=0; break; // transparent black
                }
            }

            pOut[dstY * imageW + dstX] = D3DCOLOR_ARGB(a,
                (unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255));
        }
    }
}

static void UnpackDXT3Block(const unsigned char *pBlock,
                              unsigned int *pOut, int outW, int outH,
                              int blockX, int blockY, int imageW)
{
    // First 8 bytes: 4-bit explicit alpha
    for (int py = 0; py < 4; ++py)
        for (int px = 0; px < 4; px += 2)
        {
            const unsigned char b = pBlock[py * 2 + px / 2];
            const int dstX0 = blockX + px,   dstY = blockY + py;
            const int dstX1 = blockX + px + 1;
            if (dstX0 < imageW && dstY < outH)
                pOut[dstY*imageW+dstX0] = (b & 0x0F) * 17; // just alpha channel for now
            if (dstX1 < imageW && dstY < outH)
                pOut[dstY*imageW+dstX1] = ((b >> 4) & 0x0F) * 17;
        }
    // Decode colour block into temp, then merge alpha
    unsigned int colBuf[16] = {};
    int bH = std::min(4, outH - blockY);
    int bW = std::min(4, imageW - blockX);
    UnpackDXT1Block(pBlock + 8, colBuf, bW, bH, 0, 0, 4, false);
    for (int py = 0; py < bH; ++py)
        for (int px = 0; px < bW; ++px)
        {
            const int dstX = blockX + px, dstY = blockY + py;
            if (dstX >= imageW || dstY >= outH) continue;
            const unsigned int a = pOut[dstY*imageW+dstX] & 0xFF;
            const unsigned int c = colBuf[py*4+px] & 0x00FFFFFF;
            pOut[dstY*imageW+dstX] = (a << 24) | c;
        }
}

static void UnpackDXT5Block(const unsigned char *pBlock,
                              unsigned int *pOut, int outW, int outH,
                              int blockX, int blockY, int imageW)
{
    // First 8 bytes: interpolated alpha
    unsigned char a0 = pBlock[0], a1 = pBlock[1];
    unsigned char alphaTable[8];
    alphaTable[0] = a0; alphaTable[1] = a1;
    if (a0 > a1)
    {
        for (int i = 2; i < 8; ++i)
            alphaTable[i] = (unsigned char)((a0*(8-i) + a1*(i-1)) / 7);
    }
    else
    {
        for (int i = 2; i < 6; ++i)
            alphaTable[i] = (unsigned char)((a0*(6-i) + a1*(i-1)) / 5);
        alphaTable[6] = 0; alphaTable[7] = 255;
    }

    // 48-bit alpha index table (6 bytes starting at pBlock[2])
    unsigned long long alphaBits = 0;
    for (int i = 0; i < 6; ++i) alphaBits |= ((unsigned long long)pBlock[2+i]) << (i*8);

    // Decode colour block into temp buffer
    unsigned int colBuf[16] = {};
    int bH = std::min(4, outH - blockY);
    int bW = std::min(4, imageW - blockX);
    UnpackDXT1Block(pBlock + 8, colBuf, bW, bH, 0, 0, 4, false);

    for (int py = 0; py < bH; ++py)
        for (int px = 0; px < bW; ++px)
        {
            const int dstX = blockX + px, dstY = blockY + py;
            if (dstX >= imageW || dstY >= outH) continue;
            const int    idx = py * 4 + px;
            const int    aIdx = (int)((alphaBits >> (idx * 3)) & 0x7);
            const unsigned int a = alphaTable[aIdx];
            const unsigned int c = colBuf[idx] & 0x00FFFFFF;
            pOut[dstY*imageW+dstX] = (a << 24) | c;
    }
}

static bool DecompressDXTToBuffer(int w, int h, const unsigned char *data,
                                  noesisTexType_e texType, unsigned int *pOut)
{
    if (!data || !pOut || w <= 0 || h <= 0 ||
        (texType != NOESISTEX_DXT1 && texType != NOESISTEX_DXT3 &&
         texType != NOESISTEX_DXT5))
    {
        return false;
    }

    memset(pOut, 0, (size_t)w * (size_t)h * 4);
    const int blockSize = (texType == NOESISTEX_DXT1) ? 8 : 16;
    const unsigned char *pSrc = data;
    for (int by = 0; by < h; by += 4)
    {
        for (int bx = 0; bx < w; bx += 4)
        {
            switch (texType)
            {
            case NOESISTEX_DXT1: UnpackDXT1Block(pSrc, pOut, w, h, bx, by, w); break;
            case NOESISTEX_DXT3: UnpackDXT3Block(pSrc, pOut, w, h, bx, by, w); break;
            case NOESISTEX_DXT5: UnpackDXT5Block(pSrc, pOut, w, h, bx, by, w); break;
            default: break;
            }
            pSrc += blockSize;
        }
    }
    return true;
}

unsigned char *noeRAPI_t::Noesis_ConvertDXT(int w, int h, unsigned char *data,
                                              noesisTexType_e texType)
{
    if (!data || w <= 0 || h <= 0 ||
        (texType != NOESISTEX_DXT1 && texType != NOESISTEX_DXT3 && texType != NOESISTEX_DXT5))
    {
        return nullptr;
    }

    const size_t pixelCount = (size_t)w * (size_t)h;
    if (pixelCount > (size_t)INT_MAX / 4)
        return nullptr;

    unsigned int *pOut = (unsigned int *)malloc(pixelCount * 4);
    if (!pOut)
        return nullptr;
    mAllocs.push_back(pOut);
    DecompressDXTToBuffer(w, h, data, texType, pOut);
    return (unsigned char *)pOut;
}

//========================================================================================
// Texture / material factory
//========================================================================================

noesisTex_t *noeRAPI_t::Noesis_TextureAlloc(const char *name, int w, int h,
                                              unsigned char *data,
                                              noesisTexType_e texType)
{
    return Noesis_TextureAllocEx(name, w, h, data, w * h * 4, texType, 0, 0);
}

noesisTex_t *noeRAPI_t::Noesis_TextureAllocEx(const char *name, int w, int h,
                                                unsigned char *data, int dataLen,
                                                noesisTexType_e texType,
                                                int /*flags*/, int /*flags2*/)
{
    noesisTex_t *pTex = new noesisTex_t();
    pTex->name          = _strdup(name ? name : "");
    pTex->w             = w;
    pTex->h             = h;
    pTex->data          = data;
    pTex->dataLen       = dataLen;
    pTex->texType       = texType;
    pTex->pD3DTex       = nullptr;
    pTex->shouldFreeData = true;

    // Transfer ownership of data from mAllocs to the texture object so the
    // destructor doesn't free it twice (once via mAllocs, once via shouldFreeData).
    auto it = std::find(mAllocs.begin(), mAllocs.end(), (void *)data);
    if (it != mAllocs.end())
        mAllocs.erase(it);

    mTexPool.push_back(pTex);

    // Upload to D3D9 immediately if we have a device
    if (mpDevice && data && w > 0 && h > 0)
        UploadTexture(pTex);

    return pTex;
}

// Upload a noesisTex_t to a D3D9 texture.  Called lazily if no device at alloc time.
void noeRAPI_t::UploadTexture(noesisTex_t *pTex)
{
    if (!mpDevice || !pTex || pTex->pD3DTex || !pTex->data ||
        pTex->w <= 0 || pTex->h <= 0 || pTex->dataLen <= 0)
        return;

    D3DFORMAT sourceFmt = D3DFMT_A8R8G8B8;
    switch (pTex->texType)
    {
    case NOESISTEX_DXT1: sourceFmt = D3DFMT_DXT1; break;
    case NOESISTEX_DXT3: sourceFmt = D3DFMT_DXT3; break;
    case NOESISTEX_DXT5: sourceFmt = D3DFMT_DXT5; break;
    default:              sourceFmt = D3DFMT_A8R8G8B8; break;
    }

    const bool sourceIsCompressed = sourceFmt != D3DFMT_A8R8G8B8;
    const D3DFORMAT fmt =
        (sourceIsCompressed && !mTextureCompressionEnabled) ? D3DFMT_A8R8G8B8 : sourceFmt;

    size_t requiredDataSize = 0;
    if (!sourceIsCompressed)
    {
        const size_t pixelCount = (size_t)pTex->w * (size_t)pTex->h;
        if (pixelCount > (size_t)INT_MAX / 4)
            return;
        requiredDataSize = pixelCount * 4;
    }
    else
    {
        const size_t blockSize = (sourceFmt == D3DFMT_DXT1) ? 8 : 16;
        requiredDataSize = (((size_t)pTex->w + 3) / 4) *
                           (((size_t)pTex->h + 3) / 4) * blockSize;
    }

    if (requiredDataSize == 0 || requiredDataSize > (size_t)pTex->dataLen)
    {
        OutputDebugStringA("WARNING: Texture pixel buffer is truncated; upload skipped.\n");
        return;
    }

    const unsigned char *uploadData = pTex->data;
    std::vector<unsigned int> expandedPixels;
    if (sourceIsCompressed && !mTextureCompressionEnabled)
    {
        const size_t pixelCount = (size_t)pTex->w * (size_t)pTex->h;
        expandedPixels.resize(pixelCount);
        if (!DecompressDXTToBuffer(pTex->w, pTex->h, pTex->data,
                                   pTex->texType, expandedPixels.data()))
        {
            OutputDebugStringA("WARNING: DXT texture decompression failed.\n");
            return;
        }
        uploadData = reinterpret_cast<const unsigned char *>(expandedPixels.data());
    }

    IDirect3DTexture9 *pTex9 = nullptr;
    bool autoGenerateMips = true;
    HRESULT hr = mpDevice->CreateTexture(pTex->w, pTex->h, 0,
                                          D3DUSAGE_AUTOGENMIPMAP,
                                          fmt, D3DPOOL_MANAGED, &pTex9, nullptr);
    if (FAILED(hr))
    {
        // Some older drivers reject automatic generation for compressed formats.
        // Preserve the validated top-level texture as a one-level fallback.
        autoGenerateMips = false;
        hr = mpDevice->CreateTexture(pTex->w, pTex->h, 1, 0,
                                     fmt, D3DPOOL_MANAGED, &pTex9, nullptr);
    }
    if (FAILED(hr))
    {
        OutputDebugStringA("WARNING: CreateTexture failed\n");
        return;
    }

    D3DLOCKED_RECT lr;
    if (FAILED(pTex9->LockRect(0, &lr, nullptr, 0)))
    {
        OutputDebugStringA("WARNING: Texture LockRect failed\n");
        pTex9->Release();
        return;
    }

    if (fmt == D3DFMT_A8R8G8B8)
    {
            // RGBA32: each texel = 4 bytes.
            // Source data from Noesis is D3DCOLOR (ARGB) — matches D3DFMT_A8R8G8B8 directly.
		const unsigned char *pSrc = uploadData;
		unsigned char       *pDst = (unsigned char *)lr.pBits;
		for (int y = 0; y < pTex->h; ++y)
		{
			memcpy(pDst, pSrc, pTex->w * 4);
			pSrc += pTex->w * 4;
			pDst += lr.Pitch;
		}
    }
    else
    {
            // DXT: rows of 4-texel blocks, each row is ceil(w/4) * blockSize bytes.
		const int blockSize = (fmt == D3DFMT_DXT1) ? 8 : 16;
		const int rowBytes  = ((pTex->w + 3) / 4) * blockSize;
		const unsigned char *pSrc = uploadData;
		unsigned char       *pDst = (unsigned char *)lr.pBits;
		const int blockRows = (pTex->h + 3) / 4;
		for (int by = 0; by < blockRows; ++by)
		{
			memcpy(pDst, pSrc, rowBytes);
			pSrc += rowBytes;
			pDst += lr.Pitch;
		}
    }
    pTex9->UnlockRect(0);

    if (autoGenerateMips && pTex9->GetLevelCount() > 1)
    {
        pTex9->SetAutoGenFilterType(D3DTEXF_LINEAR);
        pTex9->GenerateMipSubLevels();
    }

    pTex->pD3DTex = pTex9;
}

void noeRAPI_t::SetTextureCompressionEnabled(bool enabled)
{
    if (mTextureCompressionEnabled == enabled)
        return;

    mTextureCompressionEnabled = enabled;
    for (noesisTex_t *pTex : mTexPool)
    {
        if (pTex && pTex->pD3DTex)
        {
            pTex->pD3DTex->Release();
            pTex->pD3DTex = nullptr;
        }
    }
    UploadPendingTextures();
}

// Upload any textures that didn't have a device at creation time.
void noeRAPI_t::UploadPendingTextures()
{
    for (noesisTex_t *pTex : mTexPool)
        if (!pTex->pD3DTex)
            UploadTexture(pTex);
}

noesisMaterial_t *noeRAPI_t::Noesis_GetMaterialList(int count, bool /*pooled*/)
{
    noesisMaterial_t *pMats = new noesisMaterial_t[count]();
    for (int i = 0; i < count; ++i)
        mMatPool.push_back(&pMats[i]);
    return pMats;
}

noesisMatData_t *noeRAPI_t::Noesis_GetMatDataFromLists(
    CArrayList<noesisMaterial_t *> &mats,
    CArrayList<noesisTex_t *>      &textures)
{
    noesisMatData_t *pMd = new noesisMatData_t();

    pMd->matCount = mats.Num();
    if (pMd->matCount > 0)
    {
        pMd->mats = new noesisMaterial_t*[pMd->matCount];
        for (int i = 0; i < pMd->matCount; ++i)
            pMd->mats[i] = mats[i];
    }

    pMd->texCount = textures.Num();
    if (pMd->texCount > 0)
    {
        pMd->textures = new noesisTex_t*[pMd->texCount];
        for (int i = 0; i < pMd->texCount; ++i)
            pMd->textures[i] = textures[i];
    }

    return pMd;
}

noesisModel_t *noeRAPI_t::Noesis_AllocModelContainer(noesisMatData_t *pMd,
                                                       noesisAnim_t    *pAnim,
                                                       int             /*animCount*/)
{
    noesisModel_t *pMdl = new noesisModel_t();
    pMdl->pMatData  = pMd;
    pMdl->pAnim     = pAnim;
    mModelPool.push_back(pMdl);
    return pMdl;
}

//========================================================================================
// File I/O
//========================================================================================

unsigned char *noeRAPI_t::Noesis_LoadPairedFile(const char *desc, const char *ext,
                                                  int &outSize, void * /*flags*/)
{
    // Open a file browser for the paired file (e.g. skeleton DAT)
    char filter[128];
    sprintf_s(filter, "%s (*%s)%c*%s%cAll Files (*.*)%c*.*%c",
              desc, ext, '\0', ext, '\0', '\0', '\0');

    char path[MAX_NOESIS_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_NOESIS_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&ofn))
        return nullptr;

    return Noesis_ReadFile(path, &outSize);
}

unsigned char *noeRAPI_t::Noesis_ReadFile(const char *path, int *outSize)
{
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return nullptr;

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize == 0)
    {
        CloseHandle(hFile);
        return nullptr;
    }

    unsigned char *pBuf = (unsigned char *)malloc(fileSize);
    DWORD bytesRead = 0;
    ReadFile(hFile, pBuf, fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);

    if (bytesRead != fileSize)
    {
        free(pBuf);
        return nullptr;
    }

    mAllocs.push_back(pBuf);
    if (outSize) *outSize = (int)fileSize;
    return pBuf;
}

void noeRAPI_t::Noesis_GetDirForFilePath(char *outDir, const char *filePath)
{
    if (!filePath || !outDir) { if (outDir) outDir[0]='\0'; return; }
    strncpy_s(outDir, MAX_NOESIS_PATH, filePath, MAX_NOESIS_PATH - 1);
    char *pLast = strrchr(outDir, '\\');
    if (!pLast) pLast = strrchr(outDir, '/');
    if (pLast)  pLast[1] = '\0';
    else        outDir[0] = '\0';
}

const char *noeRAPI_t::Noesis_GetLastCheckedName()
{
    return mCurrentFilePath;
}

//========================================================================================
// Text parser
//========================================================================================

textParser_t *noeRAPI_t::Parse_InitParser(char *text)
{
    textParser_t *p = new textParser_t();
    p->pText = text;
    p->pos   = 0;
    p->len   = text ? (int)strlen(text) : 0;
    return p;
}

bool noeRAPI_t::Parse_GetNextToken(textParser_t *parser, parseToken_t *tok)
{
    if (!parser || !tok) return false;

    for (;;)
    {
        while (parser->pos < parser->len &&
               (parser->pText[parser->pos] == ' '  || parser->pText[parser->pos] == '\t' ||
                parser->pText[parser->pos] == '\r'  || parser->pText[parser->pos] == '\n'))
            ++parser->pos;

        if (parser->pos < parser->len && parser->pText[parser->pos] == ';')
        {
            while (parser->pos < parser->len &&
                   parser->pText[parser->pos] != '\r' &&
                   parser->pText[parser->pos] != '\n')
                ++parser->pos;
            continue;
        }

        break;
    }

    if (parser->pos >= parser->len) return false;

    if (parser->pText[parser->pos] == '"')
    {
        ++parser->pos;
        int outLen = 0;
        while (parser->pos < parser->len && parser->pText[parser->pos] != '"')
        {
            if (outLen < (int)sizeof(tok->text) - 1)
                tok->text[outLen++] = parser->pText[parser->pos];
            ++parser->pos;
        }
        if (parser->pos < parser->len && parser->pText[parser->pos] == '"')
            ++parser->pos;
        tok->text[outLen] = '\0';
        return true;
    }

    int start = parser->pos;
    while (parser->pos < parser->len &&
           parser->pText[parser->pos] != ' '  && parser->pText[parser->pos] != '\t' &&
           parser->pText[parser->pos] != '\r'  && parser->pText[parser->pos] != '\n' &&
           parser->pText[parser->pos] != ';')
        ++parser->pos;

    int len = parser->pos - start;
    if (len >= (int)sizeof(tok->text)) len = (int)sizeof(tok->text) - 1;
    memcpy(tok->text, parser->pText + start, len);
    tok->text[len] = '\0';
    return true;
}

void noeRAPI_t::Parse_FreeParser(textParser_t *parser)
{
    delete parser;
}

//========================================================================================
// Logging
//========================================================================================

void noeRAPI_t::LogOutput(const char *fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
}

//========================================================================================
// RPG context
//========================================================================================

void *noeRAPI_t::rpgCreateContext()
{
    // Clear any leftover state from a previous load
    mSubmeshes.clear();
    mCurrentMaterial.clear();
    mCurrentName.clear();
    mPrimVerts.clear();
    mInPrimitive     = false;
    mForceNewSubmesh = false;
    mTriWindBackward = false;
    mHasTransform    = false;
    mTransform       = RichMat43();
    mpStagedBones    = nullptr;
    mStagedBoneCount = 0;
    mpStagedAnim     = nullptr;
    mpStagedMatData  = nullptr;
    mPending.Reset();
    return (void *)this; // non-null sentinel
}

void noeRAPI_t::rpgDestroyContext(void * /*ctx*/)
{
    mSubmeshes.clear();
    mPrimVerts.clear();
    mInPrimitive = false;
    mForceNewSubmesh = false;
}

void noeRAPI_t::rpgSetOption(int option, bool value)
{
    if (option == RPGOPT_TRIWINDBACKWARD)
        mTriWindBackward = value;
}

void noeRAPI_t::rpgSetName(char *name)
{
    mCurrentName = name ? name : "";
}

void noeRAPI_t::rpgSetMaterial(const char *matName)
{
    mCurrentMaterial = matName ? matName : "";
}

void noeRAPI_t::rpgForceNextSubmesh()
{
    mForceNewSubmesh = true;
}

void noeRAPI_t::rpgSetTransform(modelMatrix_t *pMat)
{
    if (!pMat)
    {
        mHasTransform = false;
        mTransform    = RichMat43();
    }
    else
    {
        mHasTransform = true;
        memcpy(mTransform.m, pMat, sizeof(float) * 4 * 3);
    }
}

//---- Per-vertex attribute setters -----------------------------------------------

void noeRAPI_t::rpgVertUV2f(float *uv, int /*channel*/)
{
    if (uv)
    {
        mPending.uv[0] = uv[0];
        mPending.uv[1] = uv[1];
    }
    else
    {
        mPending.uv[0] = mPending.uv[1] = 0.0f;
    }
}

void noeRAPI_t::rpgVertNormal3f(float *nrm)
{
    if (nrm) memcpy(mPending.nrm, nrm, sizeof(float)*3);
    else     memset(mPending.nrm, 0,   sizeof(float)*3);
}

void noeRAPI_t::rpgVertBoneIndexI(int *indices, int count)
{
    if (!indices) { mPending.hasBones = false; return; }
    memset(mPending.boneIdx, 0, sizeof(mPending.boneIdx));
    int n = (count < 4) ? count : 4;
    memcpy(mPending.boneIdx, indices, n * sizeof(int));
    mPending.hasBones = true;
}

void noeRAPI_t::rpgVertBoneWeightF(float *weights, int count)
{
    if (!weights) return;
    memset(mPending.boneWt, 0, sizeof(mPending.boneWt));
    int n = (count < 4) ? count : 4;
    memcpy(mPending.boneWt, weights, n * sizeof(float));
}

void noeRAPI_t::rpgVertColor4ub(unsigned char *rgba)
{
    if (rgba)
        mPending.diffuse = D3DCOLOR_ARGB(rgba[3], rgba[0], rgba[1], rgba[2]);
    else
        mPending.diffuse = 0xFFFFFFFF;
}

void noeRAPI_t::rpgVertColor4f(float *rgba)
{
    if (rgba)
    {
        const unsigned char r = (unsigned char)(rgba[0] * 255.0f);
        const unsigned char g = (unsigned char)(rgba[1] * 255.0f);
        const unsigned char b = (unsigned char)(rgba[2] * 255.0f);
        const unsigned char a = (unsigned char)(rgba[3] * 255.0f);
        mPending.diffuse = D3DCOLOR_ARGB(a, r, g, b);
    }
    else
    {
        mPending.diffuse = 0xFFFFFFFF;
    }
}

void noeRAPI_t::rpgSetPendingSkinData(const FFXISkinVertex *pSkin)
{
    if (pSkin)
        mPending.skin = *pSkin;
    else
        mPending.skin.Reset();
}

void noeRAPI_t::rpgVertex3f(float *pos)
{
    if (!pos) return;
    memcpy(mPending.pos, pos, sizeof(float)*3);

    // Apply current object transform if one is set
    if (mHasTransform)
    {
        const float *p = mPending.pos;
        const RichMat43 &M = mTransform;
        // row-vector: p' = p * M  →  p'[j] = sum_i p[i] * M.m[i][j]
        float tp[3];
        for (int j = 0; j < 3; ++j)
            tp[j] = p[0]*M.m[0].v[j] + p[1]*M.m[1].v[j] + p[2]*M.m[2].v[j] + M.m[3].v[j];
        memcpy(mPending.pos, tp, sizeof(tp));

        // Transform normal too (no translation, no scale correction for now)
        float *n = mPending.nrm;
        float tn[3];
        for (int j = 0; j < 3; ++j)
            tn[j] = n[0]*M.m[0].v[j] + n[1]*M.m[1].v[j] + n[2]*M.m[2].v[j];
        memcpy(mPending.nrm, tn, sizeof(tn));
    }

    if (mInPrimitive)
        mPrimVerts.push_back(mPending);

    mPending.Reset();
}

//---- Primitive begin/end --------------------------------------------------------

void noeRAPI_t::rpgBegin(rpgeoPrimType_e primType)
{
    mPrimType    = primType;
    mInPrimitive = true;
    mPrimVerts.clear();
}

// Convert accumulated primitive vertices to triangles and append to current submesh.
void noeRAPI_t::FlushPrimitive()
{
    if (mPrimVerts.size() < 3) return;

    ActiveSubmesh &sub = CurrentSubmesh();
    const int n = (int)mPrimVerts.size();

    // RPGOPT_TRIWINDBACKWARD means the DAT uses clockwise front faces.
    // We'll honour it at the D3D9 render state level; geometry is stored as-is.

    switch (mPrimType)
    {
    case RPGEO_TRIANGLE:
        for (int i = 0; i + 2 < n; i += 3)
            sub.AddTriangle(mPrimVerts[i], mPrimVerts[i+1], mPrimVerts[i+2]);
        break;

    case RPGEO_TRIANGLE_STRIP:
        for (int i = 0; i + 2 < n; ++i)
        {
            if (i % 2 == 0)
                sub.AddTriangle(mPrimVerts[i], mPrimVerts[i+1], mPrimVerts[i+2]);
            else
                sub.AddTriangle(mPrimVerts[i+1], mPrimVerts[i], mPrimVerts[i+2]);
        }
        break;

    case RPGEO_TRIANGLE_STRIP_FLIPPED:
        for (int i = 0; i + 2 < n; ++i)
        {
            if (i % 2 != 0)
                sub.AddTriangle(mPrimVerts[i], mPrimVerts[i+1], mPrimVerts[i+2]);
            else
                sub.AddTriangle(mPrimVerts[i+1], mPrimVerts[i], mPrimVerts[i+2]);
        }
        break;
    }
}

void noeRAPI_t::rpgEnd()
{
    // A rejected/empty primitive never reaches CurrentSubmesh. Do not let its
    // requested boundary leak into the following draw record.
    if (mPrimVerts.size() < 3)
        mForceNewSubmesh = false;
    FlushPrimitive();
    mPrimVerts.clear();
    mInPrimitive = false;
}

// Return the submesh for the current material, creating one if needed.
noeRAPI_t::ActiveSubmesh &noeRAPI_t::CurrentSubmesh()
{
    if (mForceNewSubmesh)
    {
        mForceNewSubmesh = false;
        mSubmeshes.emplace_back();
        mSubmeshes.back().materialName = mCurrentMaterial;
        mSubmeshes.back().objectName = mCurrentName;
        return mSubmeshes.back();
    }

    for (ActiveSubmesh &s : mSubmeshes)
        if (s.materialName == mCurrentMaterial && s.objectName == mCurrentName)
            return s;
    mSubmeshes.emplace_back();
    mSubmeshes.back().materialName = mCurrentMaterial;
    mSubmeshes.back().objectName = mCurrentName;
    return mSubmeshes.back();
}

//---- Extra data staging ---------------------------------------------------------

void noeRAPI_t::rpgSetExData_Bones(modelBone_t *pBones, int count)
{
    mpStagedBones    = pBones;
    mStagedBoneCount = count;
}

void noeRAPI_t::rpgSetExData_Anims(noesisAnim_t *pAnim)
{
    mpStagedAnim = pAnim;
}

void noeRAPI_t::rpgSetExData_Materials(noesisMatData_t *pMd)
{
    mpStagedMatData = pMd;
}

//---- Animation construction -----------------------------------------------------

noesisAnim_t *noeRAPI_t::rpgAnimFromBonesAndMatsFinish(modelBone_t  *pBones, int boneCount,
                                                         modelMatrix_t *pFrameMats,
                                                         int frameCount, float fps)
{
    noesisAnim_t *pAnim = new noesisAnim_t();
    pAnim->frameCount = frameCount;
    pAnim->fps        = fps;
    pAnim->boneCount  = boneCount;

    if (pBones && pFrameMats && boneCount > 0 && frameCount > 0)
    {
        const RichMat43 *pLocalMats = (const RichMat43 *)pFrameMats;
        pAnim->frameWorldMats.resize((size_t)boneCount * (size_t)frameCount);
        for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
        {
            RichMat43 *pFrameWorld = &pAnim->frameWorldMats[(size_t)frameIndex * (size_t)boneCount];
            const RichMat43 *pFrameLocal = pLocalMats + frameIndex * boneCount;
            for (int boneIndex = 0; boneIndex < boneCount; ++boneIndex)
            {
                pFrameWorld[boneIndex] = pFrameLocal[boneIndex];
                if (pBones[boneIndex].eData.parent)
                {
                    const int parentIndex = (int)(pBones[boneIndex].eData.parent - pBones);
                    if (parentIndex >= 0 && parentIndex < boneIndex)
                        pFrameWorld[boneIndex] = pFrameWorld[boneIndex] * pFrameWorld[parentIndex];
                }
            }
        }
    }

    return pAnim;
}

noesisAnim_t *noeRAPI_t::Noesis_AnimFromAnimsList(CArrayList<noesisAnim_t *> &anims, int count)
{
    (void)count;
    if (anims.Num() <= 0)
        return nullptr;

    // FF11 stores many small named animation chunks in the character DATs, and
    // the first chunk is not necessarily a useful full-body locomotion pose.
    // Until we expose named animation selection, prefer the movement clips that
    // match the temporary player camera controls.
    static const char *kPreferredNames[] =
    {
        "wlk",
        "wlk0",
        "wlk1",
        "run0",
        "run1",
        "idl0",
        "idl1",
    };

    for (int prefIndex = 0; prefIndex < (int)(sizeof(kPreferredNames) / sizeof(kPreferredNames[0])); ++prefIndex)
    {
        for (int animIndex = 0; animIndex < anims.Num(); ++animIndex)
        {
            noesisAnim_t *pAnim = anims[animIndex];
            if (pAnim && pAnim->filename && !strcmp(pAnim->filename, kPreferredNames[prefIndex]))
                return pAnim;
        }
    }

    return anims[0];
}

//---- Bone hierarchy multiplication ----------------------------------------------

void noeRAPI_t::rpgMultiplyBones(modelBone_t *pBones, int count)
{
    // Walk the array in order (parents always appear before children in FFXI skeletons).
    // Accumulate world-space transform: world = local * parent_world
    for (int i = 0; i < count; ++i)
    {
        modelBone_t *pBone = &pBones[i];
        if (!pBone->eData.parent) continue; // root: local IS world

        RichMat43       &local  = (RichMat43 &)pBone->mat;
        const RichMat43 &parent = (const RichMat43 &)pBone->eData.parent->mat;
        local = local * parent;
    }
}

//---- Model construction ---------------------------------------------------------

void noeRAPI_t::rpgOptimize() { /* no-op */ }

noesisModel_t *noeRAPI_t::rpgConstructModelAndSort()
{
    return rpgConstructModel();
}

noesisModel_t *noeRAPI_t::rpgConstructModel()
{
    noesisModel_t *pMdl = new noesisModel_t();
    pMdl->pMatData  = mpStagedMatData;
    pMdl->pAnim     = mpStagedAnim;
    pMdl->pBones    = mpStagedBones;
    pMdl->boneCount = mStagedBoneCount;

    for (const ActiveSubmesh &src : mSubmeshes)
    {
        if (src.verts.empty()) continue;

        noesisModel_t::Submesh dst;
        dst.materialName = src.materialName;
        dst.objectName   = src.objectName;
        dst.cpuVerts     = src.verts;
        dst.cpuBindVerts = src.verts;
        dst.cpuSkinVerts = src.skinVerts;
        dst.cpuIndices   = src.indices;
        dst.vertCount    = (int)src.verts.size();
        dst.triCount     = (int)(src.indices.size() / 3);
        dst.pVB          = nullptr;
        dst.pIB          = nullptr;

        pMdl->submeshes.push_back(std::move(dst));
    }

    // Upload to D3D9 if we have a device
    if (mpDevice)
        pMdl->BuildD3DBuffers(mpDevice);

    mModelPool.push_back(pMdl);

    // Reset accumulator for the next model
    mSubmeshes.clear();
    mPrimVerts.clear();
    mInPrimitive     = false;
    mpStagedBones    = nullptr;
    mStagedBoneCount = 0;
    mpStagedAnim     = nullptr;
    mpStagedMatData  = nullptr;

    return pMdl;
}

//========================================================================================
// noesisModel_t — D3D9 buffer management
//========================================================================================

void noesisModel_t::BuildD3DBuffers(IDirect3DDevice9 *pDevice)
{
    for (Submesh &sm : submeshes)
    {
        if (sm.cpuVerts.empty() || sm.cpuIndices.empty()) continue;
        if (sm.pVB || sm.pIB) continue; // already built

        // Vertex buffer
        const UINT vbSize = (UINT)(sm.cpuVerts.size() * sizeof(FFXIVertex));
        HRESULT hr = pDevice->CreateVertexBuffer(vbSize, D3DUSAGE_WRITEONLY,
                                                  FFXI_VERTEX_FVF, D3DPOOL_MANAGED,
                                                  &sm.pVB, nullptr);
        if (FAILED(hr)) continue;

        void *pVBData = nullptr;
        if (SUCCEEDED(sm.pVB->Lock(0, 0, &pVBData, 0)))
        {
            memcpy(pVBData, sm.cpuVerts.data(), vbSize);
            sm.pVB->Unlock();
        }

        // Index buffer (32-bit indices; large zone material buckets can exceed 65k vertices).
        const UINT ibSize = (UINT)(sm.cpuIndices.size() * sizeof(DWORD));
        hr = pDevice->CreateIndexBuffer(ibSize, D3DUSAGE_WRITEONLY,
                                         D3DFMT_INDEX32, D3DPOOL_MANAGED,
                                         &sm.pIB, nullptr);
        if (FAILED(hr)) continue;

        void *pIBData = nullptr;
        if (SUCCEEDED(sm.pIB->Lock(0, 0, &pIBData, 0)))
        {
            memcpy(pIBData, sm.cpuIndices.data(), ibSize);
            sm.pIB->Unlock();
        }
    }
}

static bool FFXIAnimValueLooksSane(float v)
{
    return _finite(v) && fabsf(v) < 100.0f;
}

static void FFXIAccumulateBounds(const std::vector<FFXIVertex> &verts, bool &haveBounds, RichVec3 &mins, RichVec3 &maxs)
{
    for (const FFXIVertex &v : verts)
    {
        if (!haveBounds)
        {
            mins = RichVec3(v.pos);
            maxs = RichVec3(v.pos);
            haveBounds = true;
            continue;
        }

        for (int i = 0; i < 3; ++i)
        {
            if (v.pos[i] < mins[i]) mins[i] = v.pos[i];
            if (v.pos[i] > maxs[i]) maxs[i] = v.pos[i];
        }
    }
}

static bool FFXIBoundsLookSane(const RichVec3 &bindMins, const RichVec3 &bindMaxs,
                               const RichVec3 &animMins, const RichVec3 &animMaxs)
{
    for (int i = 0; i < 3; ++i)
    {
		const float bindSize = bindMaxs[i] - bindMins[i];
		const float animSize = animMaxs[i] - animMins[i];
		const float allowance = std::max(2.0f, bindSize * 2.5f);
		if (!FFXIAnimValueLooksSane(animMins[i]) || !FFXIAnimValueLooksSane(animMaxs[i]))
			return false;
		if (bindSize > 0.5f && animSize < bindSize * 0.2f)
			return false;
		if (animSize > allowance)
			return false;
        if (animMins[i] < bindMins[i] - allowance || animMaxs[i] > bindMaxs[i] + allowance)
            return false;
    }
    return true;
}

void noesisModel_t::RestoreBindPose(IDirect3DDevice9 *pDevice)
{
    if (!pDevice)
        return;

    for (Submesh &sm : submeshes)
    {
        if (sm.cpuBindVerts.empty() || sm.cpuBindVerts.size() != sm.cpuVerts.size())
            continue;

        sm.cpuVerts = sm.cpuBindVerts;
        if (sm.pVB)
        {
            void *pVBData = nullptr;
            const UINT vbSize = (UINT)(sm.cpuVerts.size() * sizeof(FFXIVertex));
            if (SUCCEEDED(sm.pVB->Lock(0, 0, &pVBData, 0)))
            {
                memcpy(pVBData, sm.cpuVerts.data(), vbSize);
                sm.pVB->Unlock();
            }
        }
    }
}

void noesisModel_t::UpdateAnimation(float animTime, IDirect3DDevice9 *pDevice)
{
    if (!pDevice || !pAnim || pAnim->frameCount <= 0 || pAnim->boneCount <= 0 || pAnim->frameWorldMats.empty())
        return;

    const int frameIndex = (int)(animTime * pAnim->fps) % pAnim->frameCount;
    const RichMat43 *pFrameMats = &pAnim->frameWorldMats[(size_t)frameIndex * (size_t)pAnim->boneCount];

    std::vector<std::vector<FFXIVertex> > candidateVerts;
    candidateVerts.resize(submeshes.size());
    bool haveBindBounds = false;
    bool haveAnimBounds = false;
    RichVec3 bindMins, bindMaxs, animMins, animMaxs;

    for (size_t meshIndex = 0; meshIndex < submeshes.size(); ++meshIndex)
    {
        Submesh &sm = submeshes[meshIndex];
        if (sm.cpuVerts.empty() || sm.cpuSkinVerts.size() != sm.cpuVerts.size())
            continue;

        std::vector<FFXIVertex> candidate = sm.cpuBindVerts.empty() ? sm.cpuVerts : sm.cpuBindVerts;
        FFXIAccumulateBounds(candidate, haveBindBounds, bindMins, bindMaxs);
        for (size_t vertIndex = 0; vertIndex < sm.cpuVerts.size(); ++vertIndex)
        {
            const FFXISkinVertex &skin = sm.cpuSkinVerts[vertIndex];
            if (!skin.skinned)
                continue;

            RichVec4 transformedPos;
            RichVec3 transformedNrm;
            for (int weightIndex = 0; weightIndex < skin.weightCount; ++weightIndex)
            {
                const int boneIndex = skin.boneIdx[weightIndex];
                if (boneIndex < 0 || boneIndex >= pAnim->boneCount)
                    continue;

                RichMat44 skinMat = pFrameMats[boneIndex].ToMat44();
                if (skin.mirrorAxis[weightIndex] == 1)
                    skinMat = skinMat * RichMat44(-RichVec4(g_identityMatrix4x4.c1), RichVec4(g_identityMatrix4x4.c2), RichVec4(g_identityMatrix4x4.c3), RichVec4(g_identityMatrix4x4.c4));
                else if (skin.mirrorAxis[weightIndex] == 2)
                    skinMat = skinMat * RichMat44(RichVec4(g_identityMatrix4x4.c1), -RichVec4(g_identityMatrix4x4.c2), RichVec4(g_identityMatrix4x4.c3), RichVec4(g_identityMatrix4x4.c4));
                else if (skin.mirrorAxis[weightIndex] == 3)
                    skinMat = skinMat * RichMat44(RichVec4(g_identityMatrix4x4.c1), RichVec4(g_identityMatrix4x4.c2), -RichVec4(g_identityMatrix4x4.c3), RichVec4(g_identityMatrix4x4.c4));

                const float weight = skin.boneWt[weightIndex];
                const RichVec4 pos(skin.pos[weightIndex][0], skin.pos[weightIndex][1], skin.pos[weightIndex][2], weight);
                transformedPos += skinMat.TransformVec4(pos);
                const RichVec3 nrm(skin.nrm[weightIndex][0], skin.nrm[weightIndex][1], skin.nrm[weightIndex][2]);
                transformedNrm += skinMat.TransformNormal(nrm) * weight;
            }

            if (!FFXIAnimValueLooksSane(transformedPos[0]) ||
                !FFXIAnimValueLooksSane(transformedPos[1]) ||
                !FFXIAnimValueLooksSane(transformedPos[2]))
            {
                RestoreBindPose(pDevice);
                return;
            }

            candidate[vertIndex].pos[0] = transformedPos[0];
            candidate[vertIndex].pos[1] = transformedPos[1];
            candidate[vertIndex].pos[2] = transformedPos[2];
            transformedNrm.Normalize();
            candidate[vertIndex].nrm[0] = transformedNrm[0];
            candidate[vertIndex].nrm[1] = transformedNrm[1];
            candidate[vertIndex].nrm[2] = transformedNrm[2];
        }
        FFXIAccumulateBounds(candidate, haveAnimBounds, animMins, animMaxs);
        candidateVerts[meshIndex].swap(candidate);
    }

    if (haveBindBounds && haveAnimBounds && !FFXIBoundsLookSane(bindMins, bindMaxs, animMins, animMaxs))
    {
        RestoreBindPose(pDevice);
        return;
    }

    for (size_t meshIndex = 0; meshIndex < submeshes.size(); ++meshIndex)
    {
        Submesh &sm = submeshes[meshIndex];
        if (candidateVerts[meshIndex].empty())
            continue;

        sm.cpuVerts.swap(candidateVerts[meshIndex]);
        if (sm.pVB)
        {
            void *pVBData = nullptr;
            const UINT vbSize = (UINT)(sm.cpuVerts.size() * sizeof(FFXIVertex));
            if (SUCCEEDED(sm.pVB->Lock(0, 0, &pVBData, 0)))
            {
                memcpy(pVBData, sm.cpuVerts.data(), vbSize);
                sm.pVB->Unlock();
            }
        }
    }
}

void noesisModel_t::ReleaseD3DBuffers()
{
    for (Submesh &sm : submeshes)
    {
        if (sm.pVB) { sm.pVB->Release(); sm.pVB = nullptr; }
        if (sm.pIB) { sm.pIB->Release(); sm.pIB = nullptr; }
    }
}
