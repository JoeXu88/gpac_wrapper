#include "MP4WriterInter.h"

AFMP4Handle MP4WriterInit()
{
	MP4Writer *pCMP4Writer = new MP4Writer();
	return (AFMP4Handle)pCMP4Writer;
}

int MP4WriterExit(AFMP4Handle hMP4File)
{
	MP4Writer *pCMP4Writer = (MP4Writer *)hMP4File;

	if (pCMP4Writer)
	{
		delete pCMP4Writer;
		pCMP4Writer = NULL;
	}

	return 0;
}

int MP4WriterCreateFile(AFMP4Handle hMP4File, char *strName, MP4_ENC_TYPE nEncType, int nWidth, int nHeight, int nFps)
{
	MP4Writer *pCMP4Writer = (MP4Writer *)hMP4File;
	int ret = pCMP4Writer->Create(strName);
	if(!ret) pCMP4Writer->AddVideoTrack(nEncType, nWidth, nHeight, nFps);

	return ret;
}

int MP4WriterSaveFile(AFMP4Handle hMP4File)
{
	MP4Writer *pCMP4Writer = (MP4Writer *)hMP4File;
	return pCMP4Writer->Save();
}

int MP4WriterWriteFile(AFMP4Handle hMP4File, unsigned char *pData, int nSize, uint64_t nTimeStamp)
{
	MP4Writer *pCMP4Writer = (MP4Writer *)hMP4File;

	return pCMP4Writer->WriteVideo(pData, nSize, nTimeStamp);
}

