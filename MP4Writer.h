#ifndef __MP4_WRITER_H__
#define __MP4_WRITER_H__

#include <vector>

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
	//video use low 4bits 0x0f
	AF_MP4_WITH_VIDEO = 0x01,
	AF_MP4_VIDEO_H264 = 0x02,
	AF_MP4_VIDEO_H265 = 0x04,

	//audio use high 4bits 0xf0
	AF_MP4_WITH_AUDIO = 0x10,
	AF_MP4_AUDIO_AAC = 0x20,
}MP4_ENC_TYPE;

typedef enum
{
	TRACK_NONE = 0,
	TRACK_AUDIO = 1,
	TRACK_VIDEO = 2
}Media_track_type;

typedef struct
{
	Media_track_type track_type;
	MP4_ENC_TYPE enc_type;
}Media_track;


typedef enum tag_AF_NALU_TYPE
{
	AF_NALU_VPS = 0,
	AF_NALU_SPS,
	AF_NALU_PPS,
	
	AF_NALU_MAX
}AF_NALU_TYPE;

typedef struct _videoparam
{
	int width;
	int height;
	int fps;
}ParamVideo;

typedef struct _audioparam
{
	int samplerate;
	int channel;
	int bit_depth;
	int profile;
}ParamAudio;



class MP4Writer
{
public:
	MP4Writer();
	virtual ~MP4Writer();

	int Create(char *strName);
	void AddVideoTrack(MP4_ENC_TYPE nEncType, int nWidth, int nHeight, int nFps = 25);
	void AddAudioTrack(MP4_ENC_TYPE nEncType, int samplerate, int channel, int bit_depth, int profile = GF_M4A_AAC_LC);
	int WriteVideo(unsigned char *pData, int nSize, uint64_t nTimeStamp);
	int WriteAudio(unsigned char *pData, int nSize, uint64_t nTimeStamp);
	int Save();

private:
	int WriteH264MetaData(unsigned char **ppNaluData, int *pNaluLength);
	int WriteH265MetaData(unsigned char **ppNaluData, int *pNaluLength);

	int WriteVFrame(unsigned char *pData, int nSize, bool bKey, uint64_t nTimeStamp);
	int WriteAFrame(unsigned char *pData, int nSize, uint64_t nTimeStamp);

	int WriteH264(unsigned char *pData, int nSize, uint64_t nTimeStamp);
	int WriteH265(unsigned char *pData, int nSize, uint64_t nTimeStamp);
	int WriteAAC(unsigned char *pData, int nSize, uint64_t nTimeStamp);

	int NewVideoTrack();
	int NewAudioTrack();

	inline void insertMediaTrack(MP4_ENC_TYPE enc_type, Media_track_type track_type);
	inline Media_track* getMediaTrack(Media_track_type track_type);

private:
	int ConfigAudioTrack(MP4_ENC_TYPE type);
	int ConfigAAC();

private:
	MP4_ENC_TYPE m_enEncType;
	ParamVideo m_vParam;
	ParamAudio m_aParam;
	
	uint64_t m_VTimeStamp;
	uint64_t m_VLastTimeStamp;
	uint64_t m_ATimeStamp;
	uint64_t m_ALastTimeStamp;

	bool m_VbReady;
	bool m_AbReady;

	GF_ISOFile *m_pFile;

	int32_t m_VnTrackID; //video new track id
	int32_t m_AnTrackID; //audio new track id
	uint32_t m_VnStreamIndex;
	uint32_t m_AnStreamIndex;
	
	uint8_t *m_pNaluData[AF_NALU_MAX];
	int32_t m_pNaluLength[AF_NALU_MAX];

	std::vector<Media_track> m_media_tracks;
};


#endif

