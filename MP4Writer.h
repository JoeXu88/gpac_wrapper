#ifndef __MP4_WRITER_H__
#define __MP4_WRITER_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "gpac/isomedia.h"
#include "gpac/internal/media_dev.h"
#include "gpac/constants.h"
#ifdef __cplusplus
}
#endif

typedef enum _MP4_VIDEO_ENC_TYPE
{
	AF_MP4_ENC_NONE = 0,
	//video use low 4bits 0x0f, xtype | AF_MP4_WITH_VIDEO
	AF_MP4_WITH_VIDEO = 0x01,
	AF_MP4_VIDEO_H264 = 0x03,
	AF_MP4_VIDEO_H265 = 0x05,

	//audio use high 4bits 0xf0, xtype | AF_MP4_WITH_AUDIO
	AF_MP4_WITH_AUDIO = 0x80,
	AF_MP4_AUDIO_AAC = 0x90,
}MP4_ENC_TYPE;

typedef enum tag_AF_NALU_TYPE
{
	AF_NALU_VPS = 0,
	AF_NALU_SPS,
	AF_NALU_PPS,
	
	AF_NALU_MAX
}AF_NALU_TYPE;


class MP4Writer
{
public:
	MP4Writer();
	virtual ~MP4Writer();

	int Create(char *strName, MP4_ENC_TYPE nEncType, int nWidth, int nHeight, int nFps = 25);
	int Save();
	int WriteVideo(unsigned char *pData, int nSize, uint64_t nTimeStamp);
	int WriteAudio(unsigned char *pData, int nSize, uint64_t nTimeStamp){ /*TODO*/ }

private:
	int WriteH264MetaData(unsigned char **ppNaluData, int *pNaluLength);
	int WriteH265MetaData(unsigned char **ppNaluData, int *pNaluLength);

	int WriteFrame(unsigned char *pData, int nSize, bool bKey, uint64_t nTimeStamp);

	int WriteH264(unsigned char *pData, int nSize, uint64_t nTimeStamp);
	int WriteH265(unsigned char *pData, int nSize, uint64_t nTimeStamp);

	int NewVideoTrack();

private:
	MP4_ENC_TYPE m_enEncType;
	int m_nWidth;
	int m_nHeight;
	int m_nFps;
	
	uint64_t m_VTimeStamp;
	uint64_t m_ATimeStamp;

	bool m_VbReady;
	bool m_AbReady;

	GF_ISOFile *m_pFile;

	int m_VnTrackID; //video new track id
	int m_AnTrackID; //audio new track id
	unsigned int m_nStreamIndex;
	
	unsigned char *m_pNaluData[AF_NALU_MAX];
	int m_pNaluLength[AF_NALU_MAX];
};


#endif

