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

//========================================================================================
