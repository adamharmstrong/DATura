/*========================================================================================
 Noesis FF11 support - header
 Life regrettably wasted by Rich Whitehouse
 (c) Never, all rights to be taken to the grave.
========================================================================================*/
#pragma once

#ifndef _MODEL_FF11_H
#define _MODEL_FF11_H

// NOTE: This header relies on the Noesis plugin SDK being included first.
// The SDK provides: noeRAPI_t, noesisModel_t, CRefCountedPtr<T>, CRefCountedObject,
//                   NoeAssert, and the rpg* / Noesis_* RAPI methods.
// Include pluginshare.h (or the precompiled header that pulls it in) before this file.

#include <vector>
#include <map>
#include <algorithm>
#include <string>

//========================================================================================
// Constants
//========================================================================================

#ifndef MAX_NOESIS_PATH
#define MAX_NOESIS_PATH 512
#endif

// File-header sentinel for the text-based DAT set format.
// A .dat set file begins with this exact string, followed by command tokens:
//   setPathKey  <hive> <keyName> <valueName>  — read FFXI root path from the registry
//   setPathRel  <relPath>                      — set path relative to the set file
//   setPathAbs  <absPath>                      — set an absolute path
//   dat         <name> <filename>             — load a DAT; name "__skeleton" / "__animation"
//                                               has special routing behaviour
#define NOESIS_FF11_DAT_SET "NOESIS_FF11_DAT_SET"

//========================================================================================
// ff11Opts_t — per-load options for FFXI DAT parsing
//
// Passed via gpFF11Opts; NULL means "use all defaults".
//========================================================================================

struct ff11Opts_t
{
    int  fixColorShift;           // texture color channel left-shift  (default 0 when not explicit)
    int  fixAlphaShift;           // texture alpha left-shift          (default 2 when not explicit)
    int  fixVertColorShift;       // vertex colour left-shift
    int  fixVertAlphaShift;       // vertex alpha left-shift
    bool explicitColorShift;      // true if fixColorShift  was set by the user
    bool explicitAlphaShift;      // true if fixAlphaShift  was set by the user
    bool explicitVertColorShift;  // true if fixVertColorShift was set by the user
    bool explicitVertAlphaShift;  // true if fixVertAlphaShift was set by the user
    bool noShinyMaterials;        // suppress specular / shiny material generation
    bool noVertColors;            // discard vertex colour data
    bool forceCull;               // enable backface culling on all meshes
    bool renderUnreferenced;      // also render map geometry not referenced by any map object
    bool renderEnvironment;       // also render unplaced sky / weather / celestial map geometry
    bool renderWater;             // render supported generator-owned world water surfaces
    bool renderEffectMeshes;       // render decoded 0x1F/0x21 geometry and the base pose of 0x25 morph meshes
    bool collectCollision;         // collect map triangles for runtime collision
    bool collectCollisionUnreferenced; // include unreferenced collision/helper map geometry
    bool keepNames;               // retain chunk / object names on materials etc.
    bool optimizeGeo;             // call rpgOptimize() before constructing the final model
    bool preferPaletteOverDxt;     // for 0x81 combo textures, prefer palette source over client-like DXT
};

// Global options pointer; set by the caller before loading.  NULL = all defaults.
extern ff11Opts_t *gpFF11Opts;

struct ff11MapObjectDebug_t
{
    char displayName[512]; // includes directory-qualified generated object identity
    char objectName[17];
    int mapRecordIndex;
    int mapGeoIndex;
    bool referencedByMap;
    bool roomObject; // separate DAT; not indexed by the outdoor MZB visibility tree
    bool replacedByRoom; // diagnostic retained; CPU/GPU draw geometry has been removed
    size_t visualCollisionStart;
    size_t visualCollisionCount;
    unsigned int objectFlags[2];
    unsigned int data2[8];
    float vec[4];
    float trans[3];
    float rot[3];
    float scale[3];
};

extern std::vector<ff11MapObjectDebug_t> gFF11LastMapObjects;

struct ff11DatChunkDebug_t
{
    char name[8];
    char directoryPath[128];
    int type;
    int dataOffset;
    int size;
    bool supported;
    bool isShadow;
    bool isExtracted;
    unsigned char version;
    bool isVirtual;
    bool isDirectoryOpen;
    bool isDirectoryClose;
    bool hasEffectMetadata;
    char effectMaterialName[17];
    unsigned short effectHeaderWords[8];
    bool hasGeneratorMetadata;
    unsigned int generatorSectionOffsets[4];
    unsigned short generatorAttachFlags;
    unsigned short generatorEmissionVariance;
    unsigned short generatorFramesPerEmission;
    unsigned char generatorParticlesPerEmission;
    unsigned char generatorFlags;
    unsigned char generatorMoreFlags;
    char generatorEnvironmentId[8];
    char generatorLinkedResource[8];
    unsigned char generatorLinkedDataType;
    bool hasKeyframeMetadata;
    int keyframePairCount;
    bool hasEnvironmentMetadata;
    unsigned short environmentHeaderWords[8];
    bool hasSoundPointer;
    unsigned int soundId;
    char soundPath[64];
};

extern std::vector<ff11DatChunkDebug_t> gFF11LastDatChunks;

struct ff11EnvironmentLightConfig_t
{
    unsigned int sunColor;
    unsigned int moonColor;
    unsigned int ambientColor;
    unsigned int fogColor;
    float fogFar;
    float fogNear;
    float diffuseMultiplier;
};

struct ff11EnvironmentRecord_t
{
    char name[8];
    char directoryPath[128];
    unsigned int indoorFlag;
    ff11EnvironmentLightConfig_t modelLight;
    ff11EnvironmentLightConfig_t terrainLight;
    unsigned int clearColor;
    float drawDistance;
    unsigned short selector;
    unsigned short sphereSpokeCount;
    unsigned int horizonColor;
    float skyBoxRadius;
    unsigned int skyDomeRingColors[8];
    float skyDomeElevations[8];
    int skyDomeRingCount;
    int minuteOfDay;
};

extern std::vector<ff11EnvironmentRecord_t> gFF11LastEnvironmentRecords;

struct ff11GeneratorRecord_t
{
    char name[8];
    char directoryPath[128];
    unsigned int sourceDataOffset; // payload offset within the source DAT; distinguishes reused generator names
    unsigned short attachFlags;
    unsigned short emissionVariance;
    unsigned short framesPerEmission;
    unsigned char particlesPerEmission;
    unsigned char generatorFlags;
    unsigned char moreFlags;
    char environmentId[8];
    char linkedResource[8];
    unsigned char linkedDataType;
    bool hasStandardParticleSetup;
    unsigned int standardParticleFlags;
    bool hasSpawnPosition;
    float spawnPosition[3];
    bool hasScale;
    float scale[3];
    bool hasRotation;
    float rotation[3];
    bool hasRotationVelocity;
    float rotationVelocity[3];
    bool hasColor;
    unsigned int colorBgra;
    bool hasBlendMode;
    unsigned short blendMode;
    bool hasCullDistance;
    float cullDistance;
    char redKeyframe[8];
    char greenKeyframe[8];
    char blueKeyframe[8];
    char alphaKeyframe[8];
    bool hasUvScrollU;
    bool hasUvScrollV;
    float uvScrollU;
    float uvScrollV;
    unsigned short particleLifetimeFrames;
    bool hasLinearVelocity;
    float linearVelocity[3];
    bool hasLinearAcceleration;
    float linearAcceleration[3];
    bool hasVelocityVariance;
    float velocityVariance[3];
    bool hasPositionVariance;
    float spawnRadius;
    float spawnAxisScale[3];
    char lifetimeAlphaKeyframe[8];
    bool updateLifetimeAlpha;
    bool animateSprite;
    bool hasParticleDistanceFade;
    float particleFadeNear;
    float particleFadeFar;
    bool hasAnimatedScale; // unsupported moving shoreline geometry; do not flatten into the water surface pass
};

extern std::vector<ff11GeneratorRecord_t> gFF11LastGeneratorRecords;

struct ff11KeyframeRecord_t
{
    static const int skMaxPairs = 32;
    char name[8];
    char directoryPath[128];
    int pairCount;
    float times[skMaxPairs];
    float values[skMaxPairs];
};

extern std::vector<ff11KeyframeRecord_t> gFF11LastKeyframeRecords;

// Weather folders intentionally reuse short resource names (for example the
// clod texture and clod_a01 shell occur independently under Clouds, Mist, and
// Thunder). Scope only those resources by their directory so normal character
// and terrain material names retain their original form.
inline bool Model_FF11_IsWeatherDirectory(const char *path)
{
    if (!path)
        return false;
    for (const char *cursor = path; *cursor; ++cursor)
    {
        if (cursor[0] == '/' &&
            (cursor[1] == 'w' || cursor[1] == 'W') &&
            (cursor[2] == 'e' || cursor[2] == 'E') &&
            (cursor[3] == 'a' || cursor[3] == 'A') &&
            (cursor[4] == 't' || cursor[4] == 'T') &&
            cursor[5] == '/')
            return true;
    }
    return false;
}

inline unsigned int Model_FF11_WeatherDirectoryHash(const char *path)
{
    unsigned int hash = 2166136261u;
    for (const unsigned char *cursor = (const unsigned char *)(path ? path : ""); *cursor; ++cursor)
    {
        unsigned char value = *cursor;
        if (value >= 'A' && value <= 'Z')
            value = (unsigned char)(value - 'A' + 'a');
        hash = (hash ^ value) * 16777619u;
    }
    return hash;
}

inline void Model_FF11_BuildScopedResourceName(char *dst, const size_t dstSize,
                                                const char *directoryPath,
                                                const char *resourceName)
{
    if (!dst || dstSize == 0)
        return;
    if (Model_FF11_IsWeatherDirectory(directoryPath))
        sprintf_s(dst, dstSize, "@%08X:%s", Model_FF11_WeatherDirectoryHash(directoryPath),
                  resourceName ? resourceName : "");
    else
        strcpy_s(dst, dstSize, resourceName ? resourceName : "");
}

struct ff11MapHeaderDebug_t
{
    bool valid;
    unsigned char headerData[4];
    unsigned int objectCount24;
    unsigned int unknown1;
    unsigned int unknown2[6];
    int objectTableOffset;
    int objectTableEndOffset;
    int parsedObjectCount;
    int trailingDataOffset;
    int trailingDataSize;
};

extern ff11MapHeaderDebug_t gFF11LastMapHeader;

struct ff11ZoneVisibilityLeaf_t
{
    float boundsMin[3];
    float boundsMax[3];
    std::vector<unsigned int> recordIndices;
};

struct ff11ZoneVisibilityRecord_t
{
    unsigned int cullingTableOffset;
};

struct ff11ZoneVisibilityTable_t
{
    unsigned int offset;
    std::vector<unsigned int> visibleRecordIndices;
};

extern std::vector<ff11ZoneVisibilityLeaf_t> gFF11LastZoneVisibilityLeaves;
extern std::vector<ff11ZoneVisibilityRecord_t> gFF11LastZoneVisibilityRecords;
extern std::vector<ff11ZoneVisibilityTable_t> gFF11LastZoneVisibilityTables;

struct ff11MapGeoDrawBatchDebug_t
{
    char displayName[96];
    char objectName[17];
    char mapGeoUnknownName[9];
    char materialName[64];
    int mapGeoIndex;
    int mapRecordIndex;
    unsigned char mapGeoHeaderData[4];
    unsigned int mapGeoUnknown1;
    int superIndex;
    int subIndex;
    int drawOffset;
    int vertexCount;
    int indexCount;
    int vertexStride;
    int indexMode;
    unsigned int objectFlags[2];
    unsigned short blendFlags;
    unsigned short flags2;
    int superFlag;
    int subFlag;
    bool runtimeFlag4000;
    bool runtimeFlag1000;
    bool galkaReeveUseAlpha;
    bool galkaReeveWouldAlphaBlend;
    bool daturaHardAlpha;
    bool daturaSoftBlend;
    char daturaRenderMode[32];
    char daturaRenderReason[128];
    float superBounds[6];
    float subBounds[6];
};

extern std::vector<ff11MapGeoDrawBatchDebug_t> gFF11LastMapGeoDrawBatches;

struct ff11CollisionTriangle_t
{
    float p[3][3];
    unsigned char indexFlags[3];
};

extern std::vector<ff11CollisionTriangle_t> gFF11LastCollisionTriangles;

struct ff11CollisionMeshDebug_t
{
    char displayName[64];
    int gridX;
    int gridY;
    int transformOfs;
    int geometryOfs;
    int triStart;
    int triCount;
    unsigned int bucketFlags;
    unsigned int indexFlagValueMask;
    float boundsMin[3];
    float boundsMax[3];
};

extern std::vector<ff11CollisionMeshDebug_t> gFF11LastCollisionMeshes;

int         Model_FF11_GetLastCollisionTriangleCount();
const float *Model_FF11_GetLastCollisionTrianglePoints(int index);
int         Model_FF11_GetLastCollisionMeshCount();
const ff11CollisionMeshDebug_t *Model_FF11_GetLastCollisionMesh(int index);
int         Model_FF11_GetLastMapObjectCount();
const char *Model_FF11_GetLastMapObjectDisplayName(int index);
bool        Model_FF11_GetLastMapObjectTransform(int index, float trans[3], float scale[3], float rot[3] = nullptr);
bool        Model_FF11_HasZoneVisibilityData();
bool        Model_FF11_GetZoneVisibleMapObjects(const float viewerPoint[3],
                                                std::vector<unsigned int> &mapObjectIndices);
bool        Model_FF11_IsZonePointVisible(const float viewerPoint[3], const float subjectPoint[3]);
int         Model_FF11_GetLastMapGeoDrawBatchCount();
const ff11MapGeoDrawBatchDebug_t *Model_FF11_GetLastMapGeoDrawBatch(int index);

class CFFXIChunkHandler;

//========================================================================================

class CFFXIDat
{
public:
	static const int skChunkType_Texture = 0x20;
	static const int skChunkType_Skeleton = 0x29;
	static const int skChunkType_Geo = 0x2A;
	static const int skChunkType_Animation = 0x2B;
	static const int skChunkType_Map = 0x1C;
	static const int skChunkType_DirectoryClose = 0x00;
	static const int skChunkType_DirectoryOpen = 0x01;
	static const int skChunkType_Generator = 0x05;
	static const int skChunkType_Keyframe = 0x19;
	static const int skChunkType_EffectModel = 0x1F;
	static const int skChunkType_EffectAnimated = 0x21;
	static const int skChunkType_EffectMorph = 0x25;
	// Compatibility aliases for older DATura call sites and external users.
	static const int skChunkType_EffectSmall = skChunkType_EffectAnimated;
	static const int skChunkType_EffectMesh = skChunkType_EffectMorph;
	static const int skChunkType_Environment = 0x2F;
	static const int skChunkType_SoundPointer = 0x3D;
	static const int skChunkType_MapGeo = 0x2E;
	
	static const int skBinaryChunkSize = 16;

	enum EValidateChunkResult
	{
		kVCR_Invalid = 0,
		kVCR_Supported,
		kVCR_NotSupported
	};

	struct SChunk
	{
		explicit SChunk(const unsigned char *pData, const int chunkOffset)
		{
			memcpy(mName, pData, 4);
			mName[4] = 0;
			const unsigned int info = *(const unsigned int *)(pData + 4);
			mType = (info & 0x7F);
			mSize = ((info >> 3) & 0x7FFFF0);
			mIsShadow = ((info >> 26) & 1) != 0;
			mIsExtracted = ((info >> 27) & 1) != 0;
			mVersion = (unsigned char)((info >> 28) & 7);
			mIsVirtual = ((info >> 31) & 1) != 0;
			//are low 4 bits ever set?
			mDataOffset = chunkOffset + skBinaryChunkSize;
		}

		char mName[8];
		int mType;
		int mDataOffset;
		int mSize;
		bool mIsShadow;
		bool mIsExtracted;
		unsigned char mVersion;
		bool mIsVirtual;
	};
	typedef std::vector<SChunk> TChunkList;

	const char *GetChunkDirectoryPath(const SChunk &chunk) const
	{
		for (std::vector<ff11DatChunkDebug_t>::const_reverse_iterator it = gFF11LastDatChunks.rbegin();
			 it != gFF11LastDatChunks.rend(); ++it)
		{
			if (it->dataOffset == chunk.mDataOffset)
				return it->directoryPath;
		}
		return "";
	}


	explicit CFFXIDat(unsigned char *pData, const int dataSize, noeRAPI_t *pRapi)
		: mpData(pData)
		, mDataSize(dataSize)
		, mpRapi(pRapi)
	{
	}

	EValidateChunkResult ValidateChunk(const SChunk &chunk) const;

	bool ParseChunksOfInterest();
	bool RunChunkHandlersForChunksOfInterest(const int forChunkType = -1) const;

	void RegisterChunkHandler(CFFXIChunkHandler *pChunkHandler);

	const unsigned char *GetData() const { return mpData; }
	int GetDataSize() const { return mDataSize; }

	noeRAPI_t *GetRAPI() const { return mpRapi; } //what a marine does after getting a girl drunk

protected:
	typedef std::map< int, CRefCountedPtr<CFFXIChunkHandler> > TChunkHandlerContainer;

	unsigned char *mpData;
	int mDataSize;
	noeRAPI_t *mpRapi;
	TChunkList mChunks; //only chunks of interest

	TChunkHandlerContainer mChunkHandlers;
};

//========================================================================================

class CFFXIChunkHandler : public CRefCountedObject
{
public:
	explicit CFFXIChunkHandler(const int chunkType)
		: mChunkType(chunkType)
	{
	}

	virtual CFFXIDat::EValidateChunkResult ValidateChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk, 
															const unsigned char *pChunkData, const int dataSize) const = 0;
	virtual bool HandleChunk(const CFFXIDat &dat, const CFFXIDat::SChunk &chunk,
								const unsigned char *pChunkData, const int dataSize) = 0;

	int GetChunkType() const { return mChunkType; }

private:
	int mChunkType;
};

//========================================================================================

class CFFXITextureHandler;
class CFFXISkelHandler;
class CFFXIAnimHandler;
class CFFXIGeoHandler;
class CFFXIMapHandler;
class CFFXIMapGeoHandler;
class CFFXIEffectHandler;

class CFFXIDefaultHandlerSet
{
public:
	explicit CFFXIDefaultHandlerSet(CFFXIDat *pDat = NULL);
	~CFFXIDefaultHandlerSet();

	void RegisterHandlersWithDat(CFFXIDat &dat);
	// Pointer convenience overload used when callers hold a CFFXIDat*.
	void RegisterHandlersWithDat(CFFXIDat *pDat) { if (pDat) RegisterHandlersWithDat(*pDat); }

	CFFXITextureHandler *TextureHandler() { return mpTextureHandler; }
	CFFXISkelHandler *SkelHandler() { return mpSkelHandler; }
	CFFXIAnimHandler *AnimHandler() { return mpAnimHandler; }
	CFFXIGeoHandler *GeoHandler() { return mpGeoHandler; }
	CFFXIMapHandler *MapHandler() { return mpMapHandler; }
	CFFXIMapGeoHandler *MapGeoHandler() { return mpMapGeoHandler; }
	CFFXIEffectHandler *EffectModelHandler() { return mpEffectModelHandler; }
	CFFXIEffectHandler *EffectAnimatedHandler() { return mpEffectAnimatedHandler; }
	CFFXIEffectHandler *EffectMorphHandler() { return mpEffectMorphHandler; }
	CFFXIEffectHandler *EffectMeshHandler() { return mpEffectMorphHandler; }
protected:
	CRefCountedPtr<CFFXITextureHandler> mpTextureHandler;
	CRefCountedPtr<CFFXISkelHandler> mpSkelHandler;
	CRefCountedPtr<CFFXIAnimHandler> mpAnimHandler;
	CRefCountedPtr<CFFXIGeoHandler> mpGeoHandler;
	CRefCountedPtr<CFFXIMapHandler> mpMapHandler;
	CRefCountedPtr<CFFXIMapGeoHandler> mpMapGeoHandler;
	CRefCountedPtr<CFFXIEffectHandler> mpEffectModelHandler;
	CRefCountedPtr<CFFXIEffectHandler> mpEffectAnimatedHandler;
	CRefCountedPtr<CFFXIEffectHandler> mpEffectMorphHandler;
};

//========================================================================================
// ff11Opts_t option-handler callbacks
//
// All follow the Noesis option-handler signature:
//   bool handler(const char *arg, unsigned char *store, int storeSize)
//     arg       — option argument string; NULL for boolean (flag-only) options
//     store     — points to an ff11Opts_t cast to unsigned char*
//     storeSize — must equal sizeof(ff11Opts_t)
//   Returns true on success, false if a required argument was absent or malformed.
//========================================================================================

bool Model_FF11_ShiftColorHandler        (const char *arg, unsigned char *store, int storeSize);
bool Model_FF11_ShiftAlphaHandler        (const char *arg, unsigned char *store, int storeSize);
bool Model_FF11_ShiftVertColorHandler    (const char *arg, unsigned char *store, int storeSize);
bool Model_FF11_ShiftVertAlphaHandler    (const char *arg, unsigned char *store, int storeSize);
bool Model_FF11_NoShinyHandler           (const char *arg, unsigned char *store, int storeSize);
bool Model_FF11_NoVertColorHandler       (const char *arg, unsigned char *store, int storeSize);
bool Model_FF11_ForceCullHandler         (const char *arg, unsigned char *store, int storeSize);
bool Model_FF11_RenderUnreferencedHandler(const char *arg, unsigned char *store, int storeSize);
bool Model_FF11_KeepNamesHandler         (const char *arg, unsigned char *store, int storeSize);
bool Model_FF11_OptimizeGeoHandler       (const char *arg, unsigned char *store, int storeSize);

//========================================================================================
// Plugin entry points
//========================================================================================

// Binary DAT format:
bool            Model_FF11_CheckDAT    (BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi);
noesisModel_t  *Model_FF11_LoadDAT     (BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi);
// Decode only embedded texture chunks. This avoids constructing geometry or
// replacing the global map/debug inspection state used by the main viewer.
noesisModel_t  *Model_FF11_LoadTextureDAT(BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi);

// High-poly character creation mesh DATs ("RT..." / "SHAPE: TriStrip" records):
enum FFXICreationAlphaMode
{
    FFXI_CREATION_ALPHA_SOLID = 0,
    FFXI_CREATION_ALPHA_BODY_CUTOUT,
    FFXI_CREATION_ALPHA_HUMANOID_HEAD,
    FFXI_CREATION_ALPHA_ELVAAN_F_HEAD,
    FFXI_CREATION_ALPHA_TARU_HEAD,
    FFXI_CREATION_ALPHA_MITHRA_HEAD,
    FFXI_CREATION_ALPHA_GALKA_HEAD
};

bool            Model_FF11_CheckCreationDAT(BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi);
noesisModel_t  *Model_FF11_LoadCreationDAT (BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi);
noesisModel_t  *Model_FF11_LoadCreationDATList(BYTE **fileBuffers, int *bufferLens,
                                                BYTE **materialBuffers, int *materialLens,
                                                int *materialAlphaModes, float *meshOffsets,
                                                int fileCount, int &numMdl, noeRAPI_t *rapi);
bool            Model_FF11_GetDATBonePosition(BYTE *fileBuffer, int bufferLen,
                                               const char *boneName, float outPos[3],
                                               noeRAPI_t *rapi);

// Text-format DAT set (file begins with the NOESIS_FF11_DAT_SET sentinel string):
bool            Model_FF11_CheckDATSet (BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi);
noesisModel_t  *Model_FF11_LoadDATSet  (BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi);

#endif //_MODEL_FF11_H
