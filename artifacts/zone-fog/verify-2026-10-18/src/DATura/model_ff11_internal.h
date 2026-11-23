#pragma once

template<typename T>
static const T *get_and_incr_offset(const unsigned char *pBuffer, int &bufferOfs, const int count = 1)
{
	const T *pCmd = (const T *)(pBuffer + bufferOfs);
	bufferOfs += sizeof(T) * count;
	return pCmd;
}

static void align_offset(int &bufferOfs, const int alignment)
{
	//assumes power of 2
	const int alignmentMinusOne = alignment - 1;
	bufferOfs = ((bufferOfs + alignmentMinusOne) & ~alignmentMinusOne);
}

inline void Model_FF11_SetPreviewOffset(noeRAPI_t *pRapi)
{
	float modelAngleOffset[3] = { 0.0f, 180.0f, 270.0f };
	pRapi->SetPreviewAngOfs(modelAngleOffset);
}

//========================================================================================
