#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "model_ff11_decrypt.h"

#pragma warning(disable: 4996)
#pragma warning(disable: 4267)
#pragma pack(push, 1)

#include "model_ff11_internal.h"
#include "model_ff11_texture_handler.h"
#include "model_ff11_effect_handler.h"
#include "model_ff11_animation_handlers.h"
#include "model_ff11_geometry_handler.h"
#include "model_ff11_map_handlers.h"

bool Model_FF11_CheckDAT(BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi)
{
	CFFXIDat dat(fileBuffer, bufferLen, rapi);
	CFFXIDefaultHandlerSet datHandlers(&dat);

	return dat.ParseChunksOfInterest();
}

static int Model_FF11_WeatherRootLength(const char *path)
{
	if (!path)
		return 0;
	for (const char *cursor = path; *cursor; ++cursor)
	{
		if (cursor[0] == '/' && _strnicmp(cursor, "/weat/", 6) == 0)
		{
			const char *rootEnd = cursor + 6;
			while (*rootEnd && *rootEnd != '/')
				++rootEnd;
			return (int)(rootEnd - path);
		}
	}
	return 0;
}

static bool Model_FF11_SameWeatherRoot(const char *a, const char *b)
{
	const int aLength = Model_FF11_WeatherRootLength(a);
	const int bLength = Model_FF11_WeatherRootLength(b);
	if (aLength <= 0 || aLength != bLength)
		return false;
	return _strnicmp(a, b, aLength) == 0;
}

static void Model_FF11_FindEnvironmentGenerators(
	const CFFXIMapGeoHandler::SMapGeoData &mapGeoData,
	std::vector<const ff11GeneratorRecord_t *> &matches)
{
	matches.clear();
	if (!Model_FF11_IsWeatherDirectory(mapGeoData.mDirectoryPath) ||
		!mapGeoData.mResourceName[0])
		return;

	for (const ff11GeneratorRecord_t &generator : gFF11LastGeneratorRecords)
	{
		if (generator.hasStandardParticleSetup && generator.linkedResource[0] &&
			_stricmp(generator.linkedResource, mapGeoData.mResourceName) == 0 &&
			Model_FF11_SameWeatherRoot(generator.directoryPath, mapGeoData.mDirectoryPath))
		{
			matches.push_back(&generator);
		}
	}
}

static RichMat43 Model_FF11_BuildEnvironmentTransform(const ff11GeneratorRecord_t &generator)
{
	RichMat43 transform;
	float scale[3] = { 1.0f, 1.0f, 1.0f };
	if (generator.hasScale)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::isfinite(generator.scale[axis]) && fabsf(generator.scale[axis]) > 0.00001f)
				scale[axis] = generator.scale[axis];
		}
	}
	if (generator.hasSpawnPosition)
	{
		transform[3] = RichVec3(generator.spawnPosition[0],
			generator.spawnPosition[1], generator.spawnPosition[2]);
	}

	transform[0] *= scale[0];
	// Generator-owned MapGeo already uses the zone's FFXI coordinate system:
	// positive Y is down and the upper half of a weather shell is negative Y.
	// Reflecting this axis turns the authored dome into a bowl under the zone.
	transform[1] *= scale[1];
	transform[2] *= scale[2];
	return transform;
}

static void Model_FF11_CopyObjectName(char *dst, const int dstSize, const char *src)
{
	const int copyLen = (dstSize - 1 < CFFXIMapHandler::skObjectNameLength) ? dstSize - 1 : CFFXIMapHandler::skObjectNameLength;
	memcpy(dst, src, copyLen);
	dst[copyLen] = 0;
}

static float Model_FF11_VecLength(const RichVec3 &v)
{
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static void Model_FF11_AddMapObjectDebug(const CFFXIMapHandler::SInterpretedMapObject &mapObject)
{
	ff11MapObjectDebug_t dbg = {};
	Model_FF11_CopyObjectName(dbg.objectName, sizeof(dbg.objectName), mapObject.mObjectName);
	sprintf_s(dbg.displayName, "%03d: %s", mapObject.mObjectIndex, dbg.objectName);
	dbg.mapRecordIndex = mapObject.mObjectIndex;
	dbg.mapGeoIndex = -1;
	dbg.referencedByMap = true;
	dbg.objectFlags[0] = mapObject.mObjectFlags[0];
	dbg.objectFlags[1] = mapObject.mObjectFlags[1];
	memcpy(dbg.data2, mapObject.mData2, sizeof(dbg.data2));
	dbg.vec[0] = mapObject.mVec[0];
	dbg.vec[1] = mapObject.mVec[1];
	dbg.vec[2] = mapObject.mVec[2];
	dbg.vec[3] = mapObject.mVec[3];
	dbg.trans[0] = mapObject.mTrans[0];
	dbg.trans[1] = mapObject.mTrans[1];
	dbg.trans[2] = mapObject.mTrans[2];
	dbg.rot[0] = mapObject.mRawAngles[0];
	dbg.rot[1] = mapObject.mRawAngles[1];
	dbg.rot[2] = mapObject.mRawAngles[2];
	dbg.scale[0] = mapObject.mScale[0];
	dbg.scale[1] = mapObject.mScale[1];
	dbg.scale[2] = mapObject.mScale[2];
	gFF11LastMapObjects.push_back(dbg);
}

static void Model_FF11_AddEnvironmentDebug(const CFFXIMapGeoHandler::SMapGeoData &mapGeoData, const RichMat43 &transform, const int mapGeoIndex)
{
	ff11MapObjectDebug_t dbg = {};
	Model_FF11_CopyObjectName(dbg.objectName, sizeof(dbg.objectName), mapGeoData.mpMapGeoHdr->mObjectName);
	sprintf_s(dbg.displayName, "env: %s", dbg.objectName);
	dbg.mapRecordIndex = -1;
	dbg.mapGeoIndex = mapGeoIndex;
	dbg.referencedByMap = false;
	dbg.trans[0] = transform[3][0];
	dbg.trans[1] = transform[3][1];
	dbg.trans[2] = transform[3][2];
	dbg.scale[0] = Model_FF11_VecLength(transform[0]);
	dbg.scale[1] = Model_FF11_VecLength(transform[1]);
	dbg.scale[2] = Model_FF11_VecLength(transform[2]);
	gFF11LastMapObjects.push_back(dbg);
}

static noesisModel_t *Model_FF11_ConstructModelFromHandlerSet(noeRAPI_t *pRapi, CFFXIDefaultHandlerSet &datHandlers, bool promptForExternalSkel)
{
	noesisMatData_t *pMd = NULL;

	CFFXITextureHandler *pTextureHandler = datHandlers.TextureHandler();
	if (pTextureHandler->Textures().Num() > 0)
	{
		pMd = pRapi->Noesis_GetMatDataFromLists(pTextureHandler->Materials(), pTextureHandler->Textures());
	}

	noesisModel_t *pMdl = NULL;

	//possible todo - can we have more than 1 skeleton in a dat with skinned geo and/or anims
	const CFFXISkelHandler::SInterpretedSkel *pSkel = NULL;
	CFFXISkelHandler *pSkelHandler = datHandlers.SkelHandler();
	if (pSkelHandler->Skeletons().size() > 0)
	{
		//just pick the first skeleton for now
		pSkel = &pSkelHandler->Skeletons()[0];
	}

	noesisAnim_t *pAnim = NULL;
	const CFFXIAnimHandler *pAnimHandler = datHandlers.AnimHandler();
	const CFFXIGeoHandler *pGeoHandler = datHandlers.GeoHandler();

	CFFXIDat *pSkelDat = NULL;
	unsigned char *pSkelDatBuffer = NULL;
	if (promptForExternalSkel &&
		!pSkel &&
		(pAnimHandler->AnimDataIsPresent() || pGeoHandler->GeoDataIsPresent()))
	{
		//prompt to load skeleton from another dat
		int skelDatSize = 0;
		pSkelDatBuffer = pRapi->Noesis_LoadPairedFile("FFXI Skeleton DAT", ".dat", skelDatSize, NULL);
		if (pSkelDatBuffer)
		{
			pSkelDat = new CFFXIDat(pSkelDatBuffer, skelDatSize, pRapi);
			//register the existing handlers with the new dat and just load the skeleton (and possibly animations) into the existing handlers.
			datHandlers.RegisterHandlersWithDat(pSkelDat);
			if (pSkelDat->ParseChunksOfInterest() &&
				pSkelDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_Skeleton))
			{
				if (pSkelHandler->Skeletons().size() > 0)
				{
					pSkel = &pSkelHandler->Skeletons()[0];
					if (!pAnimHandler->AnimDataIsPresent())
					{ //if there are no animations in the dat being loaded, try loading them from the skeleton dat.
						pSkelDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_Animation);
					}
				}
			}
		}
	}

	if (pAnimHandler->AnimDataIsPresent())
	{
		if (!pSkel)
		{
			pRapi->LogOutput("WARNING: Discarding animation data, because no skeleton is present.\n");
		}
		else
		{
			pAnim = pAnimHandler->ConstructAnimations(pRapi, pSkel);
		}
	}

	CFFXIMapHandler *pMapHandler = datHandlers.MapHandler();
	CFFXIMapGeoHandler *pMapGeoHandler = datHandlers.MapGeoHandler();
	CFFXIEffectHandler *pEffectModelHandler = datHandlers.EffectModelHandler();
	CFFXIEffectHandler *pEffectAnimatedHandler = datHandlers.EffectAnimatedHandler();
	CFFXIEffectHandler *pEffectMorphHandler = datHandlers.EffectMorphHandler();
	const CFFXIMapGeoHandler::TMapGeoList &mapGeoList = pMapGeoHandler->GetMapGeoList();
	const int mapGeoCount = mapGeoList.size();
	const int effectMeshCount =
		(pEffectModelHandler ? (int)pEffectModelHandler->EffectMeshes().size() : 0) +
		(pEffectAnimatedHandler ? (int)pEffectAnimatedHandler->EffectMeshes().size() : 0) +
		(pEffectMorphHandler ? (int)pEffectMorphHandler->EffectMeshes().size() : 0);

	const bool shouldRenderEffectMeshes = (gpFF11Opts && gpFF11Opts->renderEffectMeshes && effectMeshCount > 0);
	const bool anyGeoDataIsPresent = (mapGeoCount > 0 || pGeoHandler->GeoDataIsPresent() || shouldRenderEffectMeshes);

	void *pCtx = NULL;
	if (anyGeoDataIsPresent)
	{
		pCtx = pRapi->rpgCreateContext();
		pRapi->rpgSetOption(RPGOPT_TRIWINDBACKWARD, true);
	}

	//render any character data
	if (pGeoHandler->GeoDataIsPresent())
	{
		pGeoHandler->RenderGeoData(pRapi, pSkel);
	}

	/*check for map geo, and toss it into the same rpg context (for now - if these things are often present at once, throwing into separate
	models probably makes sense)*/
	if (mapGeoCount > 0)
	{
		gFF11LastMapObjects.clear();
		gFF11LastMapGeoDrawBatches.clear();
		CFFXIMapHandler::TMapObjectList &mapObjects = pMapHandler->MapObjects();
		if (mapObjects.size() > 0)
		{
			for (CFFXIMapHandler::TMapObjectList::const_iterator it = mapObjects.begin(); it != mapObjects.end(); ++it)
			{
				const CFFXIMapHandler::SInterpretedMapObject &mapObject = *it;
				Model_FF11_AddMapObjectDebug(mapObject);
				pMapGeoHandler->RenderMapObjectGeoForMapObject(pRapi, mapObject);
			}

			if (gpFF11Opts && gpFF11Opts->collectCollision && gpFF11Opts->collectCollisionUnreferenced)
			{
				RichMat43 collisionTransform;
				for (int mapGeoIndex = 0; mapGeoIndex < mapGeoCount; ++mapGeoIndex)
				{
					const CFFXIMapGeoHandler::SMapGeoData &mapGeoData = mapGeoList[mapGeoIndex];
					bool isReferenced = false;
					for (CFFXIMapHandler::TMapObjectList::const_iterator it = mapObjects.begin(); it != mapObjects.end(); ++it)
					{
						const CFFXIMapHandler::SInterpretedMapObject &mapObject = *it;
						if (memcmp(mapGeoData.mpMapGeoHdr->mObjectName, mapObject.mObjectName, CFFXIMapHandler::skObjectNameLength) == 0)
						{
							isReferenced = true;
							break;
						}
					}

					if (!isReferenced)
						pMapGeoHandler->RenderMapObjectGeo(pRapi, mapGeoIndex, collisionTransform, false, NULL, false);
				}
			}

			if (gpFF11Opts && (gpFF11Opts->renderUnreferenced || gpFF11Opts->renderEnvironment))
			{
				RichMat43 unreferencedTransform;
				//run through and manually render allowed geometry that wasn't referenced by a map object
				for (int mapGeoIndex = 0; mapGeoIndex < mapGeoCount; ++mapGeoIndex)
				{
					//not particularly concerned about speed here, it's not a default option
					const CFFXIMapGeoHandler::SMapGeoData &mapGeoData = mapGeoList[mapGeoIndex];
					bool isReferenced = false;
					for (CFFXIMapHandler::TMapObjectList::const_iterator it = mapObjects.begin(); it != mapObjects.end(); ++it)
					{
						const CFFXIMapHandler::SInterpretedMapObject &mapObject = *it;
						if (memcmp(mapGeoData.mpMapGeoHdr->mObjectName, mapObject.mObjectName, CFFXIMapHandler::skObjectNameLength) == 0)
						{
							isReferenced = true;
							break;
						}
					}

					if (!isReferenced)
					{
						std::vector<const ff11GeneratorRecord_t *> environmentGenerators;
						Model_FF11_FindEnvironmentGenerators(mapGeoData, environmentGenerators);
						if (!environmentGenerators.empty())
						{
							for (const ff11GeneratorRecord_t *generator : environmentGenerators)
							{
								RichMat43 environmentTransform =
									Model_FF11_BuildEnvironmentTransform(*generator);
								Model_FF11_AddEnvironmentDebug(mapGeoData, environmentTransform, mapGeoIndex);
								pMapGeoHandler->RenderMapObjectGeo(pRapi, mapGeoIndex,
									environmentTransform, false, NULL, true, generator);
							}
						}
						else if (gpFF11Opts->renderUnreferenced)
						{
							Model_FF11_AddEnvironmentDebug(mapGeoData, unreferencedTransform, mapGeoIndex);
							pMapGeoHandler->RenderMapObjectGeo(pRapi, mapGeoIndex,
								unreferencedTransform, false, NULL);
						}
					}
				}
			}
		}
		else
		{
			//if no map data exists alongside the map geo, just render the raw data at identity
			RichMat43 defaultTransform;
			for (int mapGeoIndex = 0; mapGeoIndex < mapGeoCount; ++mapGeoIndex)
			{
				const CFFXIMapGeoHandler::SMapGeoData &mapGeoData = mapGeoList[mapGeoIndex];
				Model_FF11_AddEnvironmentDebug(mapGeoData, defaultTransform, mapGeoIndex);
				pMapGeoHandler->RenderMapObjectGeo(pRapi, mapGeoIndex, defaultTransform, false, NULL);
			}
		}
	}

	if (shouldRenderEffectMeshes)
	{
		if (pEffectModelHandler)
			pEffectModelHandler->RenderEffectMeshes(pRapi);
		if (pEffectAnimatedHandler)
			pEffectAnimatedHandler->RenderEffectMeshes(pRapi);
		if (pEffectMorphHandler)
			pEffectMorphHandler->RenderEffectMeshes(pRapi);
	}

	//construct the model from the combined rendering
	if (pCtx)
	{
		if (pAnim)
		{
			pRapi->rpgSetExData_Anims(pAnim);
		}
		if (pMd)
		{
			pRapi->rpgSetExData_Materials(pMd);
		}

		NoeAssert(anyGeoDataIsPresent);
		if (gpFF11Opts && gpFF11Opts->optimizeGeo)
		{
			pRapi->rpgOptimize();
			pMdl = pRapi->rpgConstructModel();
		}
		else
		{
			pMdl = pRapi->rpgConstructModelAndSort();
		}
		pRapi->rpgDestroyContext(pCtx);
	}

	//if a model wasn't constructed, create a container for any anims and/or textures we loaded
	if ((pAnim || pMd) && !pMdl)
	{
		pMdl = pRapi->Noesis_AllocModelContainer(pMd, pAnim, (pAnim) ? 1 : 0);
	}

	//free second skeleton dat if it was created
	if (pSkelDatBuffer)
	{
		pRapi->Noesis_UnpooledFree(pSkelDatBuffer);
		if (pSkelDat)
		{
			delete pSkelDat;
		}
	}

	return pMdl;
}

noesisModel_t *Model_FF11_LoadDAT(BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi)
{
	CFFXIDat dat(fileBuffer, bufferLen, rapi);
	CFFXIDefaultHandlerSet datHandlers(&dat);
	dat.ParseChunksOfInterest();

	if (!dat.RunChunkHandlersForChunksOfInterest())
	{
		rapi->LogOutput("Error: Unrecoverable error during chunk handling.\n");
		return NULL;
	}

	noesisModel_t *pMdl = Model_FF11_ConstructModelFromHandlerSet(rapi, datHandlers, true);

	Model_FF11_SetPreviewOffset(rapi);

	numMdl = (pMdl) ? 1 : 0;
	return pMdl;
}

noesisModel_t *Model_FF11_LoadTextureDAT(BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi)
{
	numMdl = 0;
	if (!fileBuffer || bufferLen <= 0 || !rapi)
	{
		return NULL;
	}

	CFFXIDat dat(fileBuffer, bufferLen, rapi);
	CFFXIDefaultHandlerSet datHandlers(&dat);
	if (!dat.ParseChunksOfInterest() ||
		!dat.RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_Texture))
	{
		return NULL;
	}

	CFFXITextureHandler *pTextureHandler = datHandlers.TextureHandler();
	if (!pTextureHandler || pTextureHandler->Textures().Num() <= 0)
	{
		return NULL;
	}

	noesisMatData_t *pMd = rapi->Noesis_GetMatDataFromLists(
		pTextureHandler->Materials(), pTextureHandler->Textures());
	noesisModel_t *pMdl = rapi->Noesis_AllocModelContainer(pMd, NULL, 0);
	numMdl = pMdl ? 1 : 0;
	return pMdl;
}

ff11Opts_t *gpFF11Opts = NULL;
std::vector<ff11MapObjectDebug_t> gFF11LastMapObjects;
std::vector<ff11DatChunkDebug_t> gFF11LastDatChunks;
std::vector<ff11EnvironmentRecord_t> gFF11LastEnvironmentRecords;
std::vector<ff11GeneratorRecord_t> gFF11LastGeneratorRecords;
std::vector<ff11KeyframeRecord_t> gFF11LastKeyframeRecords;
ff11MapHeaderDebug_t gFF11LastMapHeader = {};
std::vector<ff11ZoneVisibilityLeaf_t> gFF11LastZoneVisibilityLeaves;
std::vector<ff11ZoneVisibilityRecord_t> gFF11LastZoneVisibilityRecords;
std::vector<ff11ZoneVisibilityTable_t> gFF11LastZoneVisibilityTables;
std::vector<ff11MapGeoDrawBatchDebug_t> gFF11LastMapGeoDrawBatches;
std::vector<ff11CollisionTriangle_t> gFF11LastCollisionTriangles;
std::vector<ff11CollisionMeshDebug_t> gFF11LastCollisionMeshes;

int Model_FF11_GetLastCollisionTriangleCount()
{
	return (int)gFF11LastCollisionTriangles.size();
}

const float *Model_FF11_GetLastCollisionTrianglePoints(int index)
{
	if (index < 0 || index >= (int)gFF11LastCollisionTriangles.size())
		return NULL;
	return &gFF11LastCollisionTriangles[index].p[0][0];
}

int Model_FF11_GetLastCollisionMeshCount()
{
	return (int)gFF11LastCollisionMeshes.size();
}

const ff11CollisionMeshDebug_t *Model_FF11_GetLastCollisionMesh(int index)
{
	if (index < 0 || index >= (int)gFF11LastCollisionMeshes.size())
		return NULL;
	return &gFF11LastCollisionMeshes[index];
}

int Model_FF11_GetLastMapObjectCount()
{
	return (int)gFF11LastMapObjects.size();
}

const char *Model_FF11_GetLastMapObjectDisplayName(int index)
{
	if (index < 0 || index >= (int)gFF11LastMapObjects.size())
		return "";
	return gFF11LastMapObjects[index].displayName;
}

bool Model_FF11_GetLastMapObjectTransform(int index, float trans[3], float scale[3], float rot[3])
{
	if (index < 0 || index >= (int)gFF11LastMapObjects.size())
		return false;

	const ff11MapObjectDebug_t &obj = gFF11LastMapObjects[index];
	if (trans)
		memcpy(trans, obj.trans, sizeof(obj.trans));
	if (scale)
		memcpy(scale, obj.scale, sizeof(obj.scale));
	if (rot)
		memcpy(rot, obj.rot, sizeof(obj.rot));
	return true;
}

bool Model_FF11_HasZoneVisibilityData()
{
	return !gFF11LastZoneVisibilityLeaves.empty() &&
		!gFF11LastZoneVisibilityRecords.empty() &&
		!gFF11LastZoneVisibilityTables.empty();
}

static void Model_FF11_FindZoneVisibilityRecords(const float point[3],
	std::vector<unsigned int> &recordIndices)
{
	const float epsilon = 0.05f;
	for (const ff11ZoneVisibilityLeaf_t &leaf : gFF11LastZoneVisibilityLeaves)
	{
		bool contains = true;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (point[axis] < leaf.boundsMin[axis] - epsilon ||
				point[axis] > leaf.boundsMax[axis] + epsilon)
			{
				contains = false;
				break;
			}
		}
		if (!contains)
			continue;
		for (unsigned int recordIndex : leaf.recordIndices)
		{
			if (std::find(recordIndices.begin(), recordIndices.end(), recordIndex) == recordIndices.end())
				recordIndices.push_back(recordIndex);
		}
	}
}

bool Model_FF11_GetZoneVisibleMapObjects(const float viewerPoint[3],
	std::vector<unsigned int> &mapObjectIndices)
{
	mapObjectIndices.clear();
	if (!viewerPoint || !Model_FF11_HasZoneVisibilityData())
		return false;

	std::vector<unsigned int> viewerRecords;
	Model_FF11_FindZoneVisibilityRecords(viewerPoint, viewerRecords);
	if (viewerRecords.empty())
		return false;

	bool foundViewerTable = false;
	for (unsigned int viewerRecord : viewerRecords)
	{
		if (viewerRecord >= gFF11LastZoneVisibilityRecords.size())
			continue;
		const unsigned int tableOffset =
			gFF11LastZoneVisibilityRecords[viewerRecord].cullingTableOffset;
		for (const ff11ZoneVisibilityTable_t &table : gFF11LastZoneVisibilityTables)
		{
			if (table.offset != tableOffset)
				continue;
			foundViewerTable = true;
			for (unsigned int mapObjectIndex : table.visibleRecordIndices)
			{
				if (std::find(mapObjectIndices.begin(), mapObjectIndices.end(), mapObjectIndex) ==
					mapObjectIndices.end())
					mapObjectIndices.push_back(mapObjectIndex);
			}
		}
	}
	return foundViewerTable;
}

bool Model_FF11_IsZonePointVisible(const float viewerPoint[3], const float subjectPoint[3])
{
	if (!viewerPoint || !subjectPoint || !Model_FF11_HasZoneVisibilityData())
		return true;

	std::vector<unsigned int> viewerRecords;
	std::vector<unsigned int> subjectRecords;
	Model_FF11_FindZoneVisibilityRecords(viewerPoint, viewerRecords);
	Model_FF11_FindZoneVisibilityRecords(subjectPoint, subjectRecords);
	if (viewerRecords.empty() || subjectRecords.empty())
		return true;

	bool foundViewerTable = false;
	for (unsigned int viewerRecord : viewerRecords)
	{
		if (viewerRecord >= gFF11LastZoneVisibilityRecords.size())
			continue;
		const unsigned int tableOffset =
			gFF11LastZoneVisibilityRecords[viewerRecord].cullingTableOffset;
		if (!tableOffset)
			continue;
		for (const ff11ZoneVisibilityTable_t &table : gFF11LastZoneVisibilityTables)
		{
			if (table.offset != tableOffset)
				continue;
			foundViewerTable = true;
			for (unsigned int subjectRecord : subjectRecords)
			{
				if (std::find(table.visibleRecordIndices.begin(), table.visibleRecordIndices.end(),
					subjectRecord) != table.visibleRecordIndices.end())
					return true;
			}
		}
	}

	// Unknown or incomplete records fail open. A decoded retail table which does
	// not include the subject cell is an explicit not-visible result.
	return !foundViewerTable;
}

int Model_FF11_GetLastMapGeoDrawBatchCount()
{
	return (int)gFF11LastMapGeoDrawBatches.size();
}

const ff11MapGeoDrawBatchDebug_t *Model_FF11_GetLastMapGeoDrawBatch(int index)
{
	if (index < 0 || index >= (int)gFF11LastMapGeoDrawBatches.size())
		return NULL;
	return &gFF11LastMapGeoDrawBatches[index];
}

static void Model_FF11_TryAnnotateEffectChunk(ff11DatChunkDebug_t &chunkDebug, const unsigned char *pChunkData, const int dataSize)
{
	if (dataSize < 16)
		return;

	if (chunkDebug.type != CFFXIDat::skChunkType_EffectModel &&
		chunkDebug.type != CFFXIDat::skChunkType_EffectAnimated &&
		chunkDebug.type != CFFXIDat::skChunkType_EffectMorph)
		return;

	const int materialOfs = (chunkDebug.type == CFFXIDat::skChunkType_EffectMorph) ? 0x10 :
		(chunkDebug.type == CFFXIDat::skChunkType_EffectModel ?
			((pChunkData[0] == 3) ? 0x10 : 0x0E) : 0x08);
	if (materialOfs + 16 > dataSize)
		return;

	for (int i = 0; i < 8 && (i * 2 + 2) <= dataSize; ++i)
	{
		unsigned short word = 0;
		memcpy(&word, pChunkData + i * 2, sizeof(word));
		chunkDebug.effectHeaderWords[i] = word;
	}

	memcpy(chunkDebug.effectMaterialName, pChunkData + materialOfs, 16);
	chunkDebug.effectMaterialName[16] = 0;
	chunkDebug.hasEffectMetadata = true;
}

static void Model_FF11_TryAnnotateGeneratorChunk(ff11DatChunkDebug_t &chunkDebug, const unsigned char *pChunkData, const int dataSize)
{
	if (chunkDebug.type != CFFXIDat::skChunkType_Generator || dataSize < 0x80)
		return;

	ff11GeneratorRecord_t generator = {};
	strcpy_s(generator.name, chunkDebug.name);
	strcpy_s(generator.directoryPath, chunkDebug.directoryPath);
	auto readU16 = [&](const int offset) -> unsigned short
	{
		unsigned short value = 0;
		if (offset >= 0 && offset + 2 <= dataSize)
			memcpy(&value, pChunkData + offset, sizeof(value));
		return value;
	};
	auto readU32 = [&](const int offset) -> unsigned int
	{
		unsigned int value = 0;
		if (offset >= 0 && offset + 4 <= dataSize)
			memcpy(&value, pChunkData + offset, sizeof(value));
		return value;
	};
	auto readF32 = [&](const int offset) -> float
	{
		float value = 0.0f;
		if (offset >= 0 && offset + 4 <= dataSize)
			memcpy(&value, pChunkData + offset, sizeof(value));
		return value;
	};

	generator.attachFlags = readU16(0x00);
	generator.emissionVariance = readU16(0x64);
	generator.framesPerEmission = readU16(0x66);
	generator.particlesPerEmission = pChunkData[0x68];
	generator.generatorFlags = pChunkData[0x69];
	generator.moreFlags = pChunkData[0x6b];
	memcpy(generator.environmentId, pChunkData + 0x54, 4);
	generator.environmentId[4] = 0;

	unsigned int streamOffsets[4] = {};
	unsigned int previousStart = 0x80;
	for (int i = 0; i < 4; ++i)
	{
		const unsigned int sectionStart = readU32(0x70 + i * 4);
		// The table is relative to the complete section, while pChunkData begins
		// after its 0x10-byte header.
		if (sectionStart < previousStart || sectionStart > (unsigned int)dataSize + 0x10)
			return;
		streamOffsets[i] = sectionStart;
		chunkDebug.generatorSectionOffsets[i] = sectionStart;
		previousStart = sectionStart;
	}

	// Several generator commands communicate through one of 64 scratch slots.
	// In particular, setup opcode 0x0B supplies an angular velocity vector and
	// update opcode 0x05 applies that slot to the element's authored rotation.
	// Keep the slot association instead of treating every float3 setup as the
	// same property.
	float generatorVectors[64][3] = {};
	bool generatorVectorPresent[64] = {};

	for (int streamIndex = 0; streamIndex < 4; ++streamIndex)
	{
		const int streamStart = (int)streamOffsets[streamIndex] - 0x10;
		const int streamEnd = (streamIndex + 1 < 4) ?
			(int)streamOffsets[streamIndex + 1] - 0x10 : dataSize;
		int cursor = streamStart;
		while (cursor >= 0 && cursor + 4 <= streamEnd)
		{
			const unsigned int config = readU32(cursor);
			const unsigned char opcode = (unsigned char)(config & 0xff);
			const int wordCount = (int)((config >> 8) & 0x1f);
			const int entrySize = std::max(1, wordCount) * 4;
			if (cursor + entrySize > streamEnd)
				break;

			if (streamIndex == 0 && opcode == 0x0a && entrySize >= 8)
			{
				generator.hasCullDistance = true;
				generator.cullDistance = readF32(cursor + 4);
			}
			else if (streamIndex == 1 && opcode == 0x01 && entrySize >= 34)
			{
				generator.hasStandardParticleSetup = true;
				generator.standardParticleFlags = readU32(cursor + 4);
				memcpy(generator.linkedResource, pChunkData + cursor + 12, 4);
				generator.linkedResource[4] = 0;
				generator.hasSpawnPosition = true;
				for (int axis = 0; axis < 3; ++axis)
					generator.spawnPosition[axis] = readF32(cursor + 20 + axis * 4);
				generator.linkedDataType = pChunkData[cursor + 33];
				if (entrySize >= 36)
					generator.particleLifetimeFrames = readU16(cursor + 34);
			}
			else if (streamIndex == 1 && opcode == 0x0f && entrySize >= 16)
			{
				generator.hasScale = true;
				for (int axis = 0; axis < 3; ++axis)
					generator.scale[axis] = readF32(cursor + 4 + axis * 4);
			}
			else if (streamIndex == 1 && opcode == 0x09 && entrySize >= 16)
			{
				generator.hasRotation = true;
				for (int axis = 0; axis < 3; ++axis)
					generator.rotation[axis] = readF32(cursor + 4 + axis * 4);
			}
			else if (streamIndex == 1 &&
				(opcode == 0x02 || opcode == 0x0b || opcode == 0x12) && entrySize >= 16)
			{
				const int slot = (int)((config >> 13) & 0x3f);
				generatorVectorPresent[slot] = true;
				for (int axis = 0; axis < 3; ++axis)
					generatorVectors[slot][axis] = readF32(cursor + 4 + axis * 4);
			}
			else if (streamIndex == 1 && opcode == 0x16 && entrySize >= 8)
			{
				generator.hasColor = true;
				generator.colorBgra = readU32(cursor + 4);
			}
			else if (streamIndex == 1 && opcode == 0x1e && entrySize >= 8)
			{
				// CMoElem::PrepDX consumes this byte-sized render mode. Cloud
				// stacks commonly mix ordinary alpha (0x44) with an additive
				// secondary layer (0x48), so flattening both to one blend state
				// exposes the UV-island boundaries as dark sky sectors.
				generator.hasBlendMode = true;
				generator.blendMode = readU16(cursor + 4);
			}
			else if (streamIndex == 1 && opcode >= 0x60 && opcode <= 0x63 && entrySize >= 8)
			{
				char *keyframeName = opcode == 0x60 ? generator.redKeyframe :
					opcode == 0x61 ? generator.greenKeyframe :
					opcode == 0x62 ? generator.blueKeyframe : generator.alphaKeyframe;
				memcpy(keyframeName, pChunkData + cursor + 4, 4);
				keyframeName[4] = 0;
			}
			else if (streamIndex == 2 && opcode == 0x27 && entrySize >= 8)
			{
				generator.hasUvScrollU = true;
				generator.uvScrollU = readF32(cursor + 4);
			}
			else if (streamIndex == 2 && opcode == 0x28 && entrySize >= 8)
			{
				generator.hasUvScrollV = true;
				generator.uvScrollV = readF32(cursor + 4);
			}
			else if (streamIndex == 2 && opcode == 0x05)
			{
				const int slot = (int)((config >> 13) & 0x3f);
				if (generatorVectorPresent[slot])
				{
					generator.hasRotationVelocity = true;
					for (int axis = 0; axis < 3; ++axis)
						generator.rotationVelocity[axis] = generatorVectors[slot][axis];
				}
			}
			else if (streamIndex == 2 && opcode == 0x02)
			{
				const int slot = (int)((config >> 13) & 0x3f);
				if (generatorVectorPresent[slot])
				{
					generator.hasLinearVelocity = true;
					for (int axis = 0; axis < 3; ++axis)
						generator.linearVelocity[axis] = generatorVectors[slot][axis];
				}
			}
			else if (streamIndex == 2 && opcode == 0x03 && entrySize >= 16)
			{
				generator.hasLinearAcceleration = true;
				for (int axis = 0; axis < 3; ++axis)
					generator.linearAcceleration[axis] = readF32(cursor + 4 + axis * 4);
			}

			cursor += entrySize;
			if (opcode == 0)
				break;
		}
	}

	chunkDebug.generatorAttachFlags = generator.attachFlags;
	chunkDebug.generatorEmissionVariance = generator.emissionVariance;
	chunkDebug.generatorFramesPerEmission = generator.framesPerEmission;
	chunkDebug.generatorParticlesPerEmission = generator.particlesPerEmission;
	chunkDebug.generatorFlags = generator.generatorFlags;
	chunkDebug.generatorMoreFlags = generator.moreFlags;
	strcpy_s(chunkDebug.generatorEnvironmentId, generator.environmentId);
	strcpy_s(chunkDebug.generatorLinkedResource, generator.linkedResource);
	chunkDebug.generatorLinkedDataType = generator.linkedDataType;
	chunkDebug.hasGeneratorMetadata = true;
	gFF11LastGeneratorRecords.push_back(generator);
}

static void Model_FF11_TryAnnotateKeyframeChunk(ff11DatChunkDebug_t &chunkDebug, const unsigned char *pChunkData, const int dataSize)
{
	if (chunkDebug.type != CFFXIDat::skChunkType_Keyframe || dataSize < 16 || (dataSize & 7) != 0)
		return;

	ff11KeyframeRecord_t keyframe = {};
	strcpy_s(keyframe.name, chunkDebug.name);
	strcpy_s(keyframe.directoryPath, chunkDebug.directoryPath);
	keyframe.pairCount = std::min(dataSize / 8, ff11KeyframeRecord_t::skMaxPairs);
	for (int pairIndex = 0; pairIndex < keyframe.pairCount; ++pairIndex)
	{
		memcpy(&keyframe.times[pairIndex], pChunkData + pairIndex * 8, sizeof(float));
		memcpy(&keyframe.values[pairIndex], pChunkData + pairIndex * 8 + 4, sizeof(float));
	}
	gFF11LastKeyframeRecords.push_back(keyframe);
	chunkDebug.keyframePairCount = keyframe.pairCount;
	chunkDebug.hasKeyframeMetadata = true;
}

static void Model_FF11_TrimChunkName(char *dst, const int dstSize, const char *src)
{
	if (!dst || dstSize <= 0)
		return;

	int writeOfs = 0;
	for (int i = 0; i < 4 && writeOfs < dstSize - 1; ++i)
	{
		const char c = src[i];
		if (c == 0)
			break;
		dst[writeOfs++] = c;
	}

	while (writeOfs > 0 && dst[writeOfs - 1] == ' ')
		--writeOfs;
	dst[writeOfs] = 0;
}

static void Model_FF11_BuildChunkDirectoryPath(char *dst, const int dstSize, const std::vector<std::string> &dirStack)
{
	if (!dst || dstSize <= 0)
		return;

	dst[0] = 0;
	size_t used = 0;
	for (size_t i = 0; i < dirStack.size(); ++i)
	{
		const char *part = dirStack[i].c_str();
		const size_t partLen = strlen(part);
		const size_t needed = partLen + (i ? 1 : 0);
		if (used + needed >= (size_t)dstSize)
			break;
		if (i)
			dst[used++] = '/';
		memcpy(dst + used, part, partLen);
		used += partLen;
		dst[used] = 0;
	}
}

static void Model_FF11_TryAnnotateEnvironmentChunk(ff11DatChunkDebug_t &chunkDebug, const unsigned char *pChunkData, const int dataSize)
{
	if (chunkDebug.type != CFFXIDat::skChunkType_Environment || dataSize < 16)
		return;

	for (int i = 0; i < 8 && (i * 2 + 2) <= dataSize; ++i)
	{
		unsigned short word = 0;
		memcpy(&word, pChunkData + i * 2, sizeof(word));
		chunkDebug.environmentHeaderWords[i] = word;
	}
	chunkDebug.hasEnvironmentMetadata = true;

	// Retain the authored environment after the DAT buffer is released. These
	// fields drive the runtime sky dome, clear color, and linear terrain fog.
	if (dataSize < 0x6c)
		return;

	ff11EnvironmentRecord_t env = {};
	strcpy_s(env.name, chunkDebug.name);
	strcpy_s(env.directoryPath, chunkDebug.directoryPath);
	auto readU16 = [&](int offset) -> unsigned short
	{
		unsigned short value = 0;
		if (offset >= 0 && offset + (int)sizeof(value) <= dataSize)
			memcpy(&value, pChunkData + offset, sizeof(value));
		return value;
	};
	auto readU32 = [&](int offset) -> unsigned int
	{
		unsigned int value = 0;
		if (offset >= 0 && offset + (int)sizeof(value) <= dataSize)
			memcpy(&value, pChunkData + offset, sizeof(value));
		return value;
	};
	auto readF32 = [&](int offset) -> float
	{
		float value = 0.0f;
		if (offset >= 0 && offset + (int)sizeof(value) <= dataSize)
			memcpy(&value, pChunkData + offset, sizeof(value));
		return value;
	};
	auto readLight = [&](int offset, ff11EnvironmentLightConfig_t &light)
	{
		light.sunColor = readU32(offset + 0x00);
		light.moonColor = readU32(offset + 0x04);
		light.ambientColor = readU32(offset + 0x08);
		light.fogColor = readU32(offset + 0x0c);
		light.fogFar = readF32(offset + 0x10);
		light.fogNear = readF32(offset + 0x14);
		light.diffuseMultiplier = readF32(offset + 0x18);
	};

	// Published 0x2F offsets are relative to the record at +8; pChunkData has
	// already advanced past that prefix, so fields below are shifted back by 8.
	env.indoorFlag = readU32(0x00);
	readLight(0x0c, env.modelLight);
	readLight(0x2c, env.terrainLight);
	env.clearColor = readU32(0x4c);
	env.drawDistance = readF32(0x58);
	env.selector = readU16(0x5c);
	env.sphereSpokeCount = readU16(0x5e);
	env.horizonColor = readU32(0x60);
	env.skyBoxRadius = readF32(0x68);
	for (int ring = 0; ring < 8; ++ring)
	{
		const int colorOffset = 0x6c + ring * 4;
		const int elevationOffset = 0x8c + ring * 4;
		if (colorOffset + 4 > dataSize || elevationOffset + 4 > dataSize)
			break;
		env.skyDomeRingColors[ring] = readU32(colorOffset);
		env.skyDomeElevations[ring] = readF32(elevationOffset);
		++env.skyDomeRingCount;
	}

	env.minuteOfDay = -1;
	if (strlen(env.name) == 4 &&
		env.name[0] >= '0' && env.name[0] <= '9' &&
		env.name[1] >= '0' && env.name[1] <= '9' &&
		env.name[2] >= '0' && env.name[2] <= '9' &&
		env.name[3] >= '0' && env.name[3] <= '9')
	{
		const int hour = (env.name[0] - '0') * 10 + env.name[1] - '0';
		const int minute = (env.name[2] - '0') * 10 + env.name[3] - '0';
		if (hour < 24 && minute < 60)
			env.minuteOfDay = hour * 60 + minute;
	}
	gFF11LastEnvironmentRecords.push_back(env);
}

static void Model_FF11_TryAnnotateSoundPointerChunk(ff11DatChunkDebug_t &chunkDebug, const unsigned char *pChunkData, const int dataSize)
{
	if (chunkDebug.type != CFFXIDat::skChunkType_SoundPointer || dataSize < 12)
		return;

	unsigned int soundId = 0;
	memcpy(&soundId, pChunkData + 8, sizeof(soundId));
	chunkDebug.soundId = soundId;
	sprintf_s(chunkDebug.soundPath, "se/se%03u/se%06u.spw", soundId / 1000, soundId);
	chunkDebug.hasSoundPointer = true;
}

#define FF11_LOCAL_DECL_OPTS(argRequired) \
	ff11Opts_t *pOpts = (ff11Opts_t *)store; \
	NoeAssert(storeSize == sizeof(ff11Opts_t)); \
	if (argRequired && !arg) \
	{ \
		return false; \
	}

bool Model_FF11_ShiftColorHandler(const char *arg, unsigned char *store, int storeSize)
{
	FF11_LOCAL_DECL_OPTS(true);
	pOpts->fixColorShift = atoi(arg);
	pOpts->explicitColorShift = true;
	return true;
}

bool Model_FF11_ShiftAlphaHandler(const char *arg, unsigned char *store, int storeSize)
{
	FF11_LOCAL_DECL_OPTS(true);
	pOpts->fixAlphaShift = atoi(arg);
	pOpts->explicitAlphaShift = true;
	return true;
}

bool Model_FF11_ShiftVertColorHandler(const char *arg, unsigned char *store, int storeSize)
{
	FF11_LOCAL_DECL_OPTS(true);
	pOpts->fixVertColorShift = atoi(arg);
	pOpts->explicitVertColorShift = true;
	return true;
}

bool Model_FF11_ShiftVertAlphaHandler(const char *arg, unsigned char *store, int storeSize)
{
	FF11_LOCAL_DECL_OPTS(true);
	pOpts->fixVertAlphaShift = atoi(arg);
	pOpts->explicitVertAlphaShift = true;
	return true;
}

bool Model_FF11_NoShinyHandler(const char *arg, unsigned char *store, int storeSize)
{
	FF11_LOCAL_DECL_OPTS(false);
	pOpts->noShinyMaterials = true;
	return true;
}

bool Model_FF11_NoVertColorHandler(const char *arg, unsigned char *store, int storeSize)
{
	FF11_LOCAL_DECL_OPTS(false);
	pOpts->noVertColors = true;
	return true;
}

bool Model_FF11_ForceCullHandler(const char *arg, unsigned char *store, int storeSize)
{
	FF11_LOCAL_DECL_OPTS(false);
	pOpts->forceCull = true;
	return true;
}

bool Model_FF11_RenderUnreferencedHandler(const char *arg, unsigned char *store, int storeSize)
{
	FF11_LOCAL_DECL_OPTS(false);
	pOpts->renderUnreferenced = true;
	return true;
}

bool Model_FF11_KeepNamesHandler(const char *arg, unsigned char *store, int storeSize)
{
	FF11_LOCAL_DECL_OPTS(false);
	pOpts->keepNames = true;
	return true;
}

bool Model_FF11_OptimizeGeoHandler(const char *arg, unsigned char *store, int storeSize)
{
	FF11_LOCAL_DECL_OPTS(false);
	pOpts->optimizeGeo = true;
	return true;
}

static const char *skpDatSetHeader = NOESIS_FF11_DAT_SET;
static const int skDatSetHeaderSize = strlen(skpDatSetHeader);

bool Model_FF11_CheckDATSet(BYTE *fileBuffer, int bufferLen, noeRAPI_t *rapi)
{
	if (bufferLen < skDatSetHeaderSize || memcmp(fileBuffer, skpDatSetHeader, skDatSetHeaderSize) != 0)
	{
		return false;
	}
	return true;
}

noesisModel_t *Model_FF11_LoadDATSet(BYTE *fileBuffer, int bufferLen, int &numMdl, noeRAPI_t *rapi)
{
	std::vector<CFFXIDat *> dats;
	CFFXIDefaultHandlerSet datHandlers;

	char basePath[MAX_NOESIS_PATH];
	rapi->Noesis_GetDirForFilePath(basePath, rapi->Noesis_GetLastCheckedName());
	char loadPath[MAX_NOESIS_PATH];
	strcpy_s(loadPath, basePath);

	char currentDatName[MAX_NOESIS_PATH];
	char currentDatFilename[MAX_NOESIS_PATH];

	textParser_t *pParser = rapi->Parse_InitParser((char *)fileBuffer);
	parseToken_t tok;
	while (rapi->Parse_GetNextToken(pParser, &tok))
	{
		if (!stricmp(tok.text, "setPathKey"))
		{
			HKEY baseKey;
			rapi->Parse_GetNextToken(pParser, &tok);
			if (!stricmp(tok.text, "HKEY_LOCAL_MACHINE"))
			{
				baseKey = HKEY_LOCAL_MACHINE;
			}
			else if (!stricmp(tok.text, "HKEY_CURRENT_USER"))
			{
				baseKey = HKEY_CURRENT_USER;
			}
			else if (!stricmp(tok.text, "HKEY_CURRENT_CONFIG"))
			{
				baseKey = HKEY_CURRENT_CONFIG;
			}
			else if (!stricmp(tok.text, "HKEY_USERS"))
			{
				baseKey = HKEY_USERS;
			}
			else if (!stricmp(tok.text, "HKEY_CLASSES_ROOT"))
			{
				baseKey = HKEY_CLASSES_ROOT;
			}
			else
			{ //default to local machine
				baseKey = HKEY_LOCAL_MACHINE;
			}

			//grab the key name
			rapi->Parse_GetNextToken(pParser, &tok);
			HKEY key;
			if (RegOpenKeyExA(baseKey, tok.text, 0, KEY_READ, &key) == ERROR_SUCCESS)
			{
				//grab the value name
				rapi->Parse_GetNextToken(pParser, &tok);
				char keyData[MAX_NOESIS_PATH];
				DWORD keyDataSize = MAX_NOESIS_PATH;
				DWORD keyType;
				if (RegQueryValueExA(key, tok.text, NULL, &keyType, (LPBYTE)keyData, &keyDataSize) == ERROR_SUCCESS)
				{
					strcpy_s(loadPath, keyData);
				}
				RegCloseKey(key);
			}
		}
		else if (!stricmp(tok.text, "setPathRel"))
		{
			//set the path relative to this file's location
			rapi->Parse_GetNextToken(pParser, &tok);
			sprintf_s(loadPath, "%s%s", basePath, tok.text);
		}
		else if (!stricmp(tok.text, "setPathAbs"))
		{
			//set an absolute path
			rapi->Parse_GetNextToken(pParser, &tok);
			strcpy_s(loadPath, tok.text);
		}
		else if (!stricmp(tok.text, "dat"))
		{
			rapi->Parse_GetNextToken(pParser, &tok);
			strcpy_s(currentDatName, tok.text);
			//grab the filename
			rapi->Parse_GetNextToken(pParser, &tok);
			sprintf_s(currentDatFilename, "%s%s", loadPath, tok.text);

			int datBufferSize;
			unsigned char *pDatBuffer = rapi->Noesis_ReadFile(currentDatFilename, &datBufferSize);
			if (pDatBuffer)
			{
				CFFXIDat *pDat = new CFFXIDat(pDatBuffer, datBufferSize, rapi);
				datHandlers.RegisterHandlersWithDat(pDat);
				pDat->ParseChunksOfInterest();

				dats.push_back(pDat);
				if (!stricmp(currentDatName, "__skeleton"))
				{
					pDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_Skeleton);
				}
				else if (!stricmp(currentDatName, "__animation"))
				{
					pDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_Animation);
				}
				else
				{
					//else, name is currently unused. might be useful for future functionality.
					pDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_Texture);
					pDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_Geo);
					pDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_Map);
					pDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_MapGeo);
					pDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_EffectModel);
					pDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_EffectAnimated);
					pDat->RunChunkHandlersForChunksOfInterest(CFFXIDat::skChunkType_EffectMorph);
				}
			}
			else
			{
				rapi->LogOutput("Failed to load file '%s'\n", currentDatFilename);
			}
		}
	}
	rapi->Parse_FreeParser(pParser);

	if (!datHandlers.TextureHandler())
	{
		//if any of the handlers are null, we didn't encounter a single loadable dat.
		rapi->LogOutput("Error: No relevant data was loaded.\n");
		return NULL;
	}

	noesisModel_t *pMdl = Model_FF11_ConstructModelFromHandlerSet(rapi, datHandlers, false);

	Model_FF11_SetPreviewOffset(rapi);

	for (std::vector<CFFXIDat *>::iterator it = dats.begin(); it != dats.end(); ++it)
	{
		CFFXIDat *pDat = *it;
		rapi->Noesis_UnpooledFree((void *)pDat->GetData());
		delete pDat;
	}

	numMdl = (pMdl) ? 1 : 0;
	return pMdl;
}

//========================================================================================

CFFXIDat::EValidateChunkResult CFFXIDat::ValidateChunk(const CFFXIDat::SChunk &chunk) const
{
	TChunkHandlerContainer::const_iterator it = mChunkHandlers.find(chunk.mType);
	if (it == mChunkHandlers.end())
	{
		return kVCR_NotSupported;
	}

	const int dataSize = chunk.mSize - skBinaryChunkSize;
	return it->second->ValidateChunk(*this, chunk, mpData + chunk.mDataOffset, dataSize);
}

bool CFFXIDat::ParseChunksOfInterest()
{
	mChunks.clear();
	gFF11LastDatChunks.clear();
	gFF11LastEnvironmentRecords.clear();
	gFF11LastGeneratorRecords.clear();
	gFF11LastKeyframeRecords.clear();
	std::vector<std::string> dirStack;

	int ofs = 0;
	while (ofs <= (mDataSize - skBinaryChunkSize))
	{
		SChunk chunk(mpData + ofs, ofs);
		if ((ofs + chunk.mSize) > mDataSize)
		{
			// Chunk extends past the end of the file.
			// Treat this as end-of-list rather than a parse error: FFXI character
			// model DATs (ROM/63/ etc.) have no explicit zero-size terminator â€”
			// the file just ends, and the last chunk's 'next' field naturally points
			// past EOF.  The FFXI Tool handles this identically (NextData returns
			// NULL on overrun).  Returning false here incorrectly rejects every
			// character body / face / animation DAT.
			break;
		}
		//consider a 0-sized chunk to be a terminator
		if (chunk.mSize <= 0)
		{
			break;
		}

		const EValidateChunkResult validateChunkResult = ValidateChunk(chunk);
		ff11DatChunkDebug_t chunkDebug = {};
		memcpy(chunkDebug.name, chunk.mName, sizeof(chunk.mName));
		chunkDebug.type = chunk.mType;
		chunkDebug.dataOffset = chunk.mDataOffset;
		chunkDebug.size = chunk.mSize;
		chunkDebug.supported = (validateChunkResult == kVCR_Supported);
		chunkDebug.isShadow = chunk.mIsShadow;
		chunkDebug.isExtracted = chunk.mIsExtracted;
		chunkDebug.version = chunk.mVersion;
		chunkDebug.isVirtual = chunk.mIsVirtual;

		char dirName[8] = {};
		Model_FF11_TrimChunkName(dirName, sizeof(dirName), chunk.mName);
		if (chunk.mType == skChunkType_DirectoryOpen && dirName[0])
		{
			dirStack.push_back(dirName);
			chunkDebug.isDirectoryOpen = true;
		}
		else if (chunk.mType == skChunkType_DirectoryClose)
		{
			chunkDebug.isDirectoryClose = true;
		}
		Model_FF11_BuildChunkDirectoryPath(chunkDebug.directoryPath, sizeof(chunkDebug.directoryPath), dirStack);
		Model_FF11_TryAnnotateEffectChunk(chunkDebug, mpData + chunk.mDataOffset, chunk.mSize - skBinaryChunkSize);
		Model_FF11_TryAnnotateGeneratorChunk(chunkDebug, mpData + chunk.mDataOffset, chunk.mSize - skBinaryChunkSize);
		Model_FF11_TryAnnotateKeyframeChunk(chunkDebug, mpData + chunk.mDataOffset, chunk.mSize - skBinaryChunkSize);
		Model_FF11_TryAnnotateEnvironmentChunk(chunkDebug, mpData + chunk.mDataOffset, chunk.mSize - skBinaryChunkSize);
		Model_FF11_TryAnnotateSoundPointerChunk(chunkDebug, mpData + chunk.mDataOffset, chunk.mSize - skBinaryChunkSize);
		gFF11LastDatChunks.push_back(chunkDebug);
		if (chunk.mType == skChunkType_DirectoryClose && !dirStack.empty())
			dirStack.pop_back();
		if (validateChunkResult == kVCR_Supported)
		{
			mChunks.push_back(chunk);
		}
		else if (validateChunkResult == kVCR_Invalid)
		{ //bad chunk data, abort parsing
			return false;
		}

		ofs += chunk.mSize;
	}

	return mChunks.size() > 0;
}

bool CFFXIDat::RunChunkHandlersForChunksOfInterest(const int forChunkType) const
{
	//run through the pre-built chunks of interest list, and run registered handlers for them.
	for (TChunkList::const_iterator it = mChunks.begin(); it != mChunks.end(); ++it)
	{
		const SChunk &chunk = *it;
		if (forChunkType >= 0 && chunk.mType != forChunkType)
		{
			continue;
		}

		TChunkHandlerContainer::const_iterator itHandler = mChunkHandlers.find(chunk.mType);
		if (itHandler != mChunkHandlers.end())
		{
			const int dataSize = chunk.mSize - skBinaryChunkSize;
			if (!itHandler->second->HandleChunk(*this, chunk, mpData + chunk.mDataOffset, dataSize))
			{
				mpRapi->LogOutput("WARNING: Chunk handler failed on chunk type %i at %i.\n", chunk.mType, chunk.mDataOffset);
			}
		}
	}

	return true;
}

void CFFXIDat::RegisterChunkHandler(CFFXIChunkHandler *pChunkHandler)
{
	const int chunkType = pChunkHandler->GetChunkType();
	TChunkHandlerContainer::iterator it = mChunkHandlers.lower_bound(chunkType);
	if (it != mChunkHandlers.end() && it->first == chunkType)
	{
		NoeAssert(!"Handler already registered to chunk type!");
		return;
	}
	mChunkHandlers.insert(it, TChunkHandlerContainer::value_type(chunkType, pChunkHandler));
}

//========================================================================================

#define FFXI_CREATE_AND_REGISTER_HANDLER(dat, handlerPointer, handlerType, ...) \
	if (!handlerPointer) \
	{ \
		handlerPointer = new handlerType(__VA_ARGS__); \
	} \
	dat.RegisterChunkHandler(handlerPointer);

CFFXIDefaultHandlerSet::CFFXIDefaultHandlerSet(CFFXIDat *pDat)
{
	if (pDat)
	{
		RegisterHandlersWithDat(*pDat);
	}
}

CFFXIDefaultHandlerSet::~CFFXIDefaultHandlerSet() = default;

void CFFXIDefaultHandlerSet::RegisterHandlersWithDat(CFFXIDat &dat)
{
	FFXI_CREATE_AND_REGISTER_HANDLER(dat, mpTextureHandler, CFFXITextureHandler);
	FFXI_CREATE_AND_REGISTER_HANDLER(dat, mpSkelHandler, CFFXISkelHandler);
	FFXI_CREATE_AND_REGISTER_HANDLER(dat, mpAnimHandler, CFFXIAnimHandler);
	FFXI_CREATE_AND_REGISTER_HANDLER(dat, mpGeoHandler, CFFXIGeoHandler);
	FFXI_CREATE_AND_REGISTER_HANDLER(dat, mpMapHandler, CFFXIMapHandler);
	FFXI_CREATE_AND_REGISTER_HANDLER(dat, mpMapGeoHandler, CFFXIMapGeoHandler);
	FFXI_CREATE_AND_REGISTER_HANDLER(dat, mpEffectModelHandler, CFFXIEffectHandler, CFFXIDat::skChunkType_EffectModel);
	FFXI_CREATE_AND_REGISTER_HANDLER(dat, mpEffectAnimatedHandler, CFFXIEffectHandler, CFFXIDat::skChunkType_EffectAnimated);
	FFXI_CREATE_AND_REGISTER_HANDLER(dat, mpEffectMorphHandler, CFFXIEffectHandler, CFFXIDat::skChunkType_EffectMorph);
}

#pragma pack(pop)
