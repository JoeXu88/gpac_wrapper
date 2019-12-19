#pragma once

#include "MP4Writer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* AFMP4Handle;

#define AF_MP4_INVALID_HANDLE ((AFMP4Handle)0)

AFMP4Handle MP4WriterInit();
int MP4WriterExit(AFMP4Handle hMP4File);

int MP4WriterCreateFile(AFMP4Handle hMP4File, char *strName, MP4_ENC_TYPE nEncType, int nWidth, int nHeight, int nFps);
int MP4WriterSaveFile(AFMP4Handle hMP4File);

int MP4WriterWriteFile(AFMP4Handle hMP4File, unsigned char *pData, int nSize, uint64_t nTimeStamp);


#ifdef __cplusplus
}
#endif
