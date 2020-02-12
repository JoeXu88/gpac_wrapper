#include "MP4Writer.h"

const uint64_t DEFAULT_TIME_SCALE_V = 1000;


const static int g_HEVC_NaluMap[AF_NALU_MAX] = {
	GF_HEVC_NALU_VID_PARAM,
	GF_HEVC_NALU_SEQ_PARAM,
	GF_HEVC_NALU_PIC_PARAM
};

struct videoTrackHelper
{
	static int ParseNalu(unsigned char *pData, int nSize, int *pStart, int *pEnd)
	{
		int i = 0;
		*pStart = 0;
		*pEnd = 0;

		while ((pData[i] != 0x00 || pData[i + 1] != 0x00 || pData[i + 2] != 0x01)
			&& (pData[i] != 0x00 || pData[i + 1] != 0x00 || pData[i + 2] != 0x00 || pData[i + 3] != 0x01))
		{
			i++;
			if (i + 4 >= nSize)
				return 0;
		}

		if (pData[i] != 0x00 || pData[i + 1] != 0x00 || pData[i + 2] != 0x01)
			i++;

		if (pData[i] != 0x00 || pData[i + 1] != 0x00 || pData[i + 2] != 0x01)
			return 0;
		
		i += 3;
		*pStart = i;

		while ((pData[i] != 0x00 || pData[i + 1] != 0x00 || pData[i + 2] != 0x00)
			&& (pData[i] != 0x00 || pData[i + 1] != 0x00 || pData[i + 2] != 0x01))
		{
			i++;
			if (i + 3 >= nSize)
			{ 
				*pEnd = nSize; 
				return (*pEnd - *pStart);
			}
		}

		*pEnd = i;
		return (*pEnd - *pStart);
	}
};

struct audioTrackHelper
{
	static s8 GetAACSampleRateID(u32 SamplRate)
	{
		switch (SamplRate)
		{
			case 96000: return  0;
			case 88200: return  1;
			case 64000: return  2;
			case 48000: return  3;
			case 44100: return  4;
			case 32000: return  5;
			case 24000: return  6;
			case 22050: return  7;
			case 16000: return  8;
			case 12000: return  9;
			case 11025: return 10;
			case 8000 : return 11;
			case 7350 : return 12;
			default:    return -1;
		}
	}


	//gf_m4a_get_profile
	static u8 GetAACProfile(u8 AudioType, u32 SampleRate, u8 Channel)
	{
		switch (AudioType)
		{
			case 2: /* AAC LC */
			{
				if (Channel <= 2)  return (SampleRate <= 24000) ? 0x28 : 0x29; /* LC@L1 or LC@L2 */
				if (Channel <= 5)  return (SampleRate <= 48000) ? 0x2A : 0x2B; /* LC@L4 or LC@L5 */
								return (SampleRate <= 48000) ? 0x50 : 0x51; /* LC@L4 or LC@L5 */
			}
			case 5: /* HE-AAC - SBR */
			{
				if (Channel <= 2)  return (SampleRate <= 24000) ? 0x2C : 0x2D; /* HE@L2 or HE@L3 */
				if (Channel <= 5)  return (SampleRate <= 48000) ? 0x2E : 0x2F; /* HE@L4 or HE@L5 */
								return (SampleRate <= 48000) ? 0x52 : 0x53; /* HE@L6 or HE@L7 */
			}
			case 29: /*HE-AACv2 - SBR+PS*/
			{
				if (Channel <= 2)  return (SampleRate <= 24000) ? 0x30 : 0x31; /* HE-AACv2@L2 or HE-AACv2@L3 */
				if (Channel <= 5)  return (SampleRate <= 48000) ? 0x32 : 0x33; /* HE-AACv2@L4 or HE-AACv2@L5 */
								return (SampleRate <= 48000) ? 0x54 : 0x55; /* HE-AACv2@L6 or HE-AACv2@L7 */
			}
			default: /* default to HQ */
			{
				if (Channel <= 2)  return (SampleRate <  24000) ? 0x0E : 0x0F; /* HQ@L1 or HQ@L2 */
								return 0x10; /* HQ@L3 */
			}
		}
	}

	static void GetAudioSpecificConfig(u8 AudioType, u8 SampleRateID, u8 Channel, u8 *pHigh, u8 *pLow)
	{
		u16 Config;

		Config = (AudioType & 0x1f);
		Config <<= 4;
		Config |= SampleRateID & 0x0f;
		Config <<= 4;
		Config |= Channel & 0x0f;
		Config <<= 3;

		*pLow  = Config & 0xff;
		Config >>= 8;
		*pHigh = Config & 0xff;
	}

	static int AdtsDemux(unsigned char * data, unsigned int size, unsigned char** raw, int* raw_size)
	{
		int ret = 1;
		if (size < 7) {
			printf("adts: demux size too small");
			return 0;
		}
		unsigned char* p = data;
		unsigned char* pend = data + size;
		//unsigned char* startp = 0;

		while (p < pend) {

			// decode the ADTS.
			// @see aac-iso-13818-7.pdf, page 26
			//      6.2 Audio Data Transport Stream, ADTS
			// @see https://github.com/ossrs/srs/issues/212#issuecomment-64145885
			// byte_alignment()

			// adts_fixed_header:
			//      12bits syncword,
			//      16bits left.
			// adts_variable_header:
			//      28bits
			//      12+16+28=56bits
			// adts_error_check:
			//      16bits if protection_absent
			//      56+16=72bits
			// if protection_absent:
			//      require(7bytes)=56bits
			// else
			//      require(9bytes)=72bits

			//startp = p;

			// for aac, the frame must be ADTS format.
			/*if (p[0] != 0xff || (p[1] & 0xf0) != 0xf0) {
				ttf_human_trace("adts: not this format.");
				return 0;
			}*/

			// syncword 12 bslbf
			p++;
			// 4bits left.
			// adts_fixed_header(), 1.A.2.2.1 Fixed Header of ADTS
			// ID 1 bslbf
			// layer 2 uimsbf
			// protection_absent 1 bslbf
			int8_t pav = (*p++ & 0x0f);
			//int8_t id = (pav >> 3) & 0x01;
			/*int8_t layer = (pav >> 1) & 0x03;*/
			int8_t protection_absent = pav & 0x01;

			/**
			* ID: MPEG identifier, set to '1' if the audio data in the ADTS stream are MPEG-2 AAC (See ISO/IEC 13818-7)
			* and set to '0' if the audio data are MPEG-4. See also ISO/IEC 11172-3, subclause 2.4.2.3.
			*/
			//if (id != 0x01) {
			//	//ttf_human_trace("adts: id must be 1(aac), actual 0(mp4a).");

			//	// well, some system always use 0, but actually is aac format.
			//	// for example, houjian vod ts always set the aac id to 0, actually 1.
			//	// we just ignore it, and alwyas use 1(aac) to demux.
			//	id = 0x01;
			//}
			//else {
			//	//ttf_human_trace("adts: id must be 1(aac), actual 1(mp4a).");
			//}

			//int16_t sfiv = (*p << 8) | (*(p + 1));
			p += 2;
			// profile 2 uimsbf
			// sampling_frequency_index 4 uimsbf
			// private_bit 1 bslbf
			// channel_configuration 3 uimsbf
			// original/copy 1 bslbf
			// home 1 bslbf
			//int8_t profile = (sfiv >> 14) & 0x03;
			//int8_t sampling_frequency_index = (sfiv >> 10) & 0x0f;
			/*int8_t private_bit = (sfiv >> 9) & 0x01;*/
			//int8_t channel_configuration = (sfiv >> 6) & 0x07;
			/*int8_t original = (sfiv >> 5) & 0x01;*/
			/*int8_t home = (sfiv >> 4) & 0x01;*/
			//int8_t Emphasis; @remark, Emphasis is removed, @see https://github.com/ossrs/srs/issues/212#issuecomment-64154736
			// 4bits left.
			// adts_variable_header(), 1.A.2.2.2 Variable Header of ADTS
			// copyright_identification_bit 1 bslbf
			// copyright_identification_start 1 bslbf
			/*int8_t fh_copyright_identification_bit = (fh1 >> 3) & 0x01;*/
			/*int8_t fh_copyright_identification_start = (fh1 >> 2) & 0x01;*/
			// frame_length 13 bslbf: Length of the frame including headers and error_check in bytes.
			// use the left 2bits as the 13 and 12 bit,
			// the frame_length is 13bits, so we move 13-2=11.
			//int16_t frame_length = (sfiv << 11) & 0x1800;

			//int32_t abfv = ((*p) << 16)
			//	| ((*(p + 1)) << 8)
			//	| (*(p + 2));
			p += 3;
			// frame_length 13 bslbf: consume the first 13-2=11bits
			// the fh2 is 24bits, so we move right 24-11=13.
			//frame_length |= (abfv >> 13) & 0x07ff;
			// adts_buffer_fullness 11 bslbf
			/*int16_t fh_adts_buffer_fullness = (abfv >> 2) & 0x7ff;*/
			// number_of_raw_data_blocks_in_frame 2 uimsbf
			/*int16_t number_of_raw_data_blocks_in_frame = abfv & 0x03;*/
			// adts_error_check(), 1.A.2.2.3 Error detection
			if (!protection_absent) {
				if (size < 9) {
					printf("adts: protection_absent disappare.");
					return 0;
				}
				// crc_check 16 Rpchof
				/*int16_t crc_check = */p += 2;
			}

			// TODO: check the sampling_frequency_index
			// TODO: check the channel_configuration

			// raw_data_blocks
			/*int adts_header_size = p - startp;
			int raw_data_size = frame_length - adts_header_size;
			if (raw_data_size > pend - p) {
				ttf_human_trace("adts: raw data size too little.");
				return 0;
			}*/

			//adts_codec_.protection_absent = protection_absent;
			//adts_codec_.aac_object = srs_codec_aac_ts2rtmp((SrsAacProfile)profile);
			//adts_codec_.sampling_frequency_index = sampling_frequency_index;
			//adts_codec_.channel_configuration = channel_configuration;
			//adts_codec_.frame_length = frame_length;

			//// @see srs_audio_write_raw_frame().
			//// TODO: FIXME: maybe need to resample audio.
			//adts_codec_.sound_format = 10; // AAC
			//if (sampling_frequency_index <= 0x0c && sampling_frequency_index > 0x0a) {
			//	adts_codec_.sound_rate = SrsCodecAudioSampleRate5512;
			//}
			//else if (sampling_frequency_index <= 0x0a && sampling_frequency_index > 0x07) {
			//	adts_codec_.sound_rate = SrsCodecAudioSampleRate11025;
			//}
			//else if (sampling_frequency_index <= 0x07 && sampling_frequency_index > 0x04) {
			//	adts_codec_.sound_rate = SrsCodecAudioSampleRate22050;
			//}
			//else if (sampling_frequency_index <= 0x04) {
			//	adts_codec_.sound_rate = SrsCodecAudioSampleRate44100;
			//}
			//else {
			//	adts_codec_.sound_rate = SrsCodecAudioSampleRate44100;
			//	//srs_warn("adts invalid sample rate for flv, rate=%#x", sampling_frequency_index);
			//}
			//adts_codec_.sound_type = srs_max(0, srs_min(1, channel_configuration - 1));
			//// TODO: FIXME: finger it out the sound size by adts.
			//adts_codec_.sound_size = 1; // 0(8bits) or 1(16bits).

										// frame data.
			*raw = p;
			*raw_size = pend - p;

			break;
		}

		return ret;
	}

};


MP4Writer::MP4Writer()
{
	m_enEncType = AF_MP4_ENC_NONE;

	memset(&m_vParam, 0, sizeof(m_vParam));
	memset(&m_aParam, 0, sizeof(m_aParam));

	m_VTimeStamp = -1;
	m_ATimeStamp = -1;

	m_VbReady = false;
	m_AbReady = false;

	m_pFile = NULL;
	
	m_VnTrackID = -1;
	m_AnTrackID = -1;
	m_VnStreamIndex = 0;
	
	memset(m_pNaluData, 0, sizeof(m_pNaluData));
	memset(m_pNaluLength, 0, sizeof(m_pNaluLength));
    // gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_DEBUG);

}

MP4Writer::~MP4Writer()
{
	Save();
}

int MP4Writer::Create(char *strName)
{
	if ((strName == NULL) || strlen(strName) == 0)
	{
		return -1;
	}

	m_pFile = gf_isom_open(strName, GF_ISOM_OPEN_WRITE, NULL);
	if (m_pFile == NULL)
	{
		return -1;
	}

	gf_isom_set_brand_info(m_pFile, GF_ISOM_BRAND_MP42, 0);

	// bool do_flat = false;
    // gf_isom_set_storage_mode(m_pFile, (do_flat==true) ? GF_ISOM_STORE_FLAT : GF_ISOM_STORE_STREAMABLE);
    gf_isom_set_storage_mode(m_pFile, GF_ISOM_STORE_STREAMABLE);

	return 0;
}

void MP4Writer::AddVideoTrack(MP4_ENC_TYPE nEncType, int nWidth, int nHeight, int nFps)
{
	m_enEncType = (MP4_ENC_TYPE)(m_enEncType|nEncType);
	m_vParam.width = nWidth;
	m_vParam.height = nHeight;
	m_vParam.fps = nFps;
}

void MP4Writer::AddAudioTrack(MP4_ENC_TYPE nEncType, int samplerate, int channel, int bit_depth, int profile)
{
	m_enEncType = (MP4_ENC_TYPE)(m_enEncType|nEncType);
	m_aParam.samplerate = samplerate;
	m_aParam.channel = channel;
	m_aParam.bit_depth = bit_depth;
	m_aParam.profile = profile;
}

int MP4Writer::Save()
{
	for (int i = 0; i < AF_NALU_MAX; i++)
	{
		if (m_pNaluData[i])
		{
			delete m_pNaluData[i];
			m_pNaluData[i] = NULL;
			m_pNaluLength[i] = 0;
		}
	}
	
	if (m_pFile)
	{
		gf_isom_close(m_pFile);
		m_pFile = NULL;
	}
	
	m_VTimeStamp = -1;
	m_ATimeStamp = -1;
	m_VbReady = false;
	m_AbReady = false;

	memset(&m_vParam, 0, sizeof(m_vParam));
	memset(&m_aParam, 0, sizeof(m_aParam));
	
	return 0;
}

int MP4Writer::WriteVideo(unsigned char *pData, int nSize, uint64_t nTimeStamp)
{
	if(m_pFile == NULL)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("need create file first, maybe call Create failed?\n"));
		return -1;
	}

	if (m_enEncType & AF_MP4_WITH_VIDEO)
	{
		MP4_ENC_TYPE type = MP4_ENC_TYPE(m_enEncType & 0x0f);
		if (type == AF_MP4_VIDEO_H265)
		{
			return WriteH265(pData, nSize, nTimeStamp);
		}
		else if (type == AF_MP4_VIDEO_H264)
		{
			return WriteH264(pData, nSize, nTimeStamp);
		}
	}

	GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("no video track to write\n"));
	return -1;
}

int MP4Writer::WriteAudio(unsigned char *pData, int nSize, uint64_t nTimeStamp)
{
	if(m_pFile == NULL)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("need create file first, maybe call Create failed?\n"));
		return -1;
	}

	if (m_enEncType & AF_MP4_WITH_AUDIO)
	{
		MP4_ENC_TYPE type = MP4_ENC_TYPE(m_enEncType & 0xf0);
		if (type == AF_MP4_AUDIO_AAC)
		{
			return WriteAAC(pData, nSize, nTimeStamp);
		}
	}

	GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("no audio track to write\n"));
	return -1;
}
int MP4Writer::WriteAAC(unsigned char *pData, int nSize, uint64_t nTimeStamp)
{
	if ((pData == NULL) || (nSize <= 0))
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("null data got\n"));
		return -1;
	}

	if (!m_AbReady)
	{
		if(NewAudioTrack()) return -1;
		m_AbReady = true;
	}

	return WriteAFrame(pData, nSize, nTimeStamp);
}

int MP4Writer::WriteH264(unsigned char *pData, int nSize, uint64_t nTimeStamp)
{
	if ((pData == NULL) || (nSize <= 0))
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("null data got\n"));
		return -1;
	}

	int lenIn = nSize;
	int lenOut = 0;
	unsigned char *pIn = pData;
	unsigned char *pOut = NULL;
	bool bKey = false;

	int nBufSize = 0;
	unsigned char *pBuf = (unsigned char *)malloc(nSize + 4);
	memset(pBuf, 0, nSize + 4);

	do {
		int nNalStart = 0;
		int nNalEnd = 0;
		lenOut = videoTrackHelper::ParseNalu(pIn, lenIn, &nNalStart, &nNalEnd);
		if (lenOut <= 0)
			break;
		pOut = pIn + nNalStart;
		if (pOut)
		{
			unsigned int nNalType = (pOut[0] & 0x1F);
			switch (nNalType)
			{
				case 0x07: // SPS
					if (!m_VbReady)
					{
						m_pNaluData[AF_NALU_SPS] = new unsigned char[lenOut];
						memcpy(m_pNaluData[AF_NALU_SPS], pOut, lenOut);
						m_pNaluLength[AF_NALU_SPS] = lenOut;
						// printf("sps frame, size:%d\n", lenOut);
					}
					break;
				case 0x08: // PPS
					if (!m_VbReady)
					{
						m_pNaluData[AF_NALU_PPS] = new unsigned char[lenOut];
						memcpy(m_pNaluData[AF_NALU_PPS], pOut, lenOut);
						m_pNaluLength[AF_NALU_PPS] = lenOut;
						// printf("pps frame, size:%d\n", lenOut);
					}
					break;
				case 0x06: // SEI
					// printf("sei frame, size:%d\n", lenOut);
					break;
				default :
					if(nNalType == 0x05) bKey = true; //I frame
					memcpy(pBuf + nBufSize, &lenOut, 4);
					pBuf[0] = (lenOut >> 24) & 0xff;
					pBuf[1] = (lenOut >> 16) & 0xff;
					pBuf[2] = (lenOut >> 8) & 0xff;
					pBuf[3] = lenOut & 0xff;
					nBufSize += 4;
					memcpy(pBuf + nBufSize, pOut, lenOut);
					nBufSize += lenOut;				
					// printf("I frame:%d, size:%d, datalen: %d, buf:%.2x %.2x %.2x %.2x\n", bKey, nBufSize, lenOut, pBuf[0], pBuf[1], pBuf[2], pBuf[3]);
					break;
			}
			
			lenIn -= (lenOut + (pOut - pIn));
			pIn = pOut + lenOut;
		}
	} while (1);

	if (!m_VbReady && m_pNaluData[AF_NALU_SPS] && m_pNaluData[AF_NALU_PPS])
	{
		int nPPSLen = m_pNaluLength[AF_NALU_PPS];
		int nZeroCount = 0;
		for (int ni = nPPSLen - 1; ni >= 0; ni--)
		{
			if (m_pNaluData[AF_NALU_PPS][ni])
				break;
			nZeroCount++;
		}

		m_pNaluLength[AF_NALU_PPS] -= nZeroCount;
		WriteH264MetaData(m_pNaluData, m_pNaluLength);
		m_VbReady = true;
	}

	if (m_VbReady && (nBufSize > 0))
		WriteVFrame(pBuf, nBufSize, bKey, nTimeStamp);

	if (pBuf)
	{
		free(pBuf);
		pBuf = NULL;
	}

	return 0;
}

int MP4Writer::WriteH265(unsigned char *pData, int nSize, uint64_t nTimeStamp)
{
	if ((pData == NULL) || (nSize <= 0))
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("null data got\n"));
		return -1;
	}

	int lenIn = nSize;
	int lenOut = 0;
	unsigned char *pIn = pData;
	unsigned char *pOut = NULL;
	bool bKey = false;

	int nBufSize = 0;
	unsigned char *pBuf = (unsigned char *)malloc(nSize + 4);
	memset(pBuf, 0, nSize + 4);

	do {
		int nNalStart = 0;
		int nNalEnd = 0;
		lenOut = videoTrackHelper::ParseNalu(pIn, lenIn, &nNalStart, &nNalEnd);
		if (lenOut <= 0)
			break;
		pOut = pIn + nNalStart;
		if (pOut)
		{
			unsigned int nNalType = ((pOut[0] >> 1) & 0x3F);
			switch (nNalType)
			{
				case 0x20: // VPS
					if (!m_VbReady)
					{
						m_pNaluData[AF_NALU_VPS] = new unsigned char[lenOut];
						memcpy(m_pNaluData[AF_NALU_VPS], pOut, lenOut);
						m_pNaluLength[AF_NALU_VPS] = lenOut;
						// printf("vps frame, size: %d\n", lenOut);
					}
					break;
				case 0x21: // SPS
					if (!m_VbReady)
					{
						m_pNaluData[AF_NALU_SPS] = new unsigned char[lenOut];
						memcpy(m_pNaluData[AF_NALU_SPS], pOut, lenOut);
						m_pNaluLength[AF_NALU_SPS] = lenOut;
						// printf("sps frame, size: %d\n", lenOut);
					}
					break;
				case 0x22: // PPS
					if (!m_VbReady)
					{
						m_pNaluData[AF_NALU_PPS] = new unsigned char[lenOut];
						memcpy(m_pNaluData[AF_NALU_PPS], pOut, lenOut);
						m_pNaluLength[AF_NALU_PPS] = lenOut;
						// printf("pps frame, size: %d\n", lenOut);
					}
					break;
				case 0x27:
				case 0x28: // SEI
					break;
				default :
					// printf("frame type: %.2x\n", nNalType);
					bool write_frame = false;
					if(nNalType >= 0x10 && nNalType <= 0x15)
					{
						write_frame = true;
						bKey = true;
					}
					else if(nNalType >= 0x00 && nNalType <= 0x09)
						write_frame = true;

					if ( write_frame ) // I/P
					{
						// printf("dump frame\n");
						memcpy(pBuf + nBufSize, &lenOut, 4);
						pBuf[0] = (lenOut >> 24) & 0xff;
						pBuf[1] = (lenOut >> 16) & 0xff;
						pBuf[2] = (lenOut >> 8) & 0xff;
						pBuf[3] = lenOut & 0xff;
						nBufSize += 4;
						memcpy(pBuf + nBufSize, pOut, lenOut);
						nBufSize += lenOut;
					}
					break;
			}
			
			lenIn -= (lenOut + (pOut - pIn));
			pIn = pOut + lenOut;
		}
	} while (1);

	if (!m_VbReady && m_pNaluData[AF_NALU_VPS] && m_pNaluData[AF_NALU_SPS] && m_pNaluData[AF_NALU_PPS])
	{
		int nPPSLen = m_pNaluLength[AF_NALU_PPS];
		int nZeroCount = 0;
		for (int ni = nPPSLen - 1; ni >= 0; ni--)
		{
			if (m_pNaluData[AF_NALU_PPS][ni])
				break;
			nZeroCount++;
		}

		m_pNaluLength[AF_NALU_PPS] -= nZeroCount;
		WriteH265MetaData(m_pNaluData, m_pNaluLength);
		m_VbReady = true;
	}

	if (m_VbReady && (nBufSize > 0))
	{
		// printf("write frame size: %d, key: %d\n", nBufSize, bKey);
		WriteVFrame(pBuf, nBufSize, bKey, nTimeStamp);
	}

	if (pBuf)
	{
		free(pBuf);
		pBuf = NULL;
	}

	return 0;
}

int MP4Writer::NewVideoTrack()
{
	if(m_VnTrackID != -1)
	{
		gf_isom_remove_track(m_pFile, m_VnTrackID);
		m_VnTrackID = -1;
	}

	m_VnTrackID = gf_isom_new_track(m_pFile, 0, GF_ISOM_MEDIA_VISUAL, DEFAULT_TIME_SCALE_V);
	gf_isom_set_track_enabled(m_pFile, m_VnTrackID, 1);
	// gf_isom_set_composition_offset_mode(m_pFile, m_VnTrackID, GF_FALSE);
	// gf_isom_rewrite_track_dependencies(m_pFile, 1);

	return 0;
}

int MP4Writer::NewAudioTrack()
{
	if(m_AnTrackID != -1)
	{
		gf_isom_remove_track(m_pFile, m_AnTrackID);
		m_AnTrackID = -1;
	}

	m_AnTrackID = gf_isom_new_track(m_pFile, 0, GF_ISOM_MEDIA_AUDIO, DEFAULT_TIME_SCALE_V);
	gf_isom_set_track_enabled(m_pFile, m_AnTrackID, 1);

	return  ConfigAudioTrack();
}

int MP4Writer::ConfigAudioTrack()
{
	MP4_ENC_TYPE type = MP4_ENC_TYPE(m_enEncType & 0xf0);
	switch (type)
	{
	case AF_MP4_AUDIO_AAC:
		return ConfigAAC();
	
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("error audio track type\n"));
		return -1;
	}
}

int MP4Writer::ConfigAAC()
{
    u16 AudioConfig = 0;
	u8 AACProfile = 0;
	int res = 0;

	/* config aac */
    s8 SampleRateID = audioTrackHelper::GetAACSampleRateID(m_aParam.samplerate);
    if (0 > SampleRateID)
        return -1;
    audioTrackHelper::GetAudioSpecificConfig(m_aParam.profile, (u8)SampleRateID, m_aParam.channel, 
	                                        (u8*)(&AudioConfig), ((u8*)(&AudioConfig))+1);

    GF_ESD *ptStreamDesc = gf_odf_desc_esd_new(SLPredef_MP4);
    ptStreamDesc->slConfig->timestampResolution = DEFAULT_TIME_SCALE_V;
    ptStreamDesc->decoderConfig->streamType = GF_STREAM_AUDIO;
    // ptStreamDesc->decoderConfig->bufferSizeDB = 20;
    ptStreamDesc->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_AAC_MPEG4;
    ptStreamDesc->decoderConfig->decoderSpecificInfo->dataLength = 2;
    ptStreamDesc->decoderConfig->decoderSpecificInfo->data = (char *)&AudioConfig;
    ptStreamDesc->ESID = gf_isom_get_track_id(m_pFile, m_AnTrackID);
    if (GF_OK != gf_isom_new_mpeg4_description(m_pFile, m_AnTrackID, ptStreamDesc, NULL, NULL, &m_AnStreamIndex))
    {
        res = -1;
        goto ERR;
    }

    if (gf_isom_set_audio_info(m_pFile, m_AnTrackID, m_AnStreamIndex, m_aParam.samplerate, 
		m_aParam.channel, m_aParam.bit_depth, GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_BS))
    {
        res = -1;
        goto ERR;
    }

    AACProfile = audioTrackHelper::GetAACProfile(m_aParam.profile, m_aParam.samplerate, m_aParam.channel);
    gf_isom_set_pl_indication(m_pFile, GF_ISOM_PL_AUDIO, AACProfile);

    ERR:
    ptStreamDesc->decoderConfig->decoderSpecificInfo->data = NULL;
    gf_odf_desc_del((GF_Descriptor *)ptStreamDesc);

	return res;
}

int MP4Writer::WriteH264MetaData(unsigned char **ppNaluData, int *pNaluLength)
{	
	NewVideoTrack();

	GF_AVCConfig *pstAVCConfig = gf_odf_avc_cfg_new();	

	gf_isom_avc_config_new(m_pFile, m_VnTrackID, pstAVCConfig, NULL, NULL, &m_VnStreamIndex);
	gf_isom_set_nalu_extract_mode(m_pFile, m_VnTrackID, GF_ISOM_NALU_EXTRACT_INSPECT);
	gf_isom_set_cts_packing(m_pFile, m_VnTrackID, GF_TRUE);
	
	pstAVCConfig->configurationVersion = 1;
	pstAVCConfig->AVCProfileIndication = ppNaluData[AF_NALU_SPS][1];
	pstAVCConfig->profile_compatibility = ppNaluData[AF_NALU_SPS][2];
	pstAVCConfig->AVCLevelIndication = ppNaluData[AF_NALU_SPS][3];

	GF_AVCConfigSlot stAVCConfig[AF_NALU_MAX];
	memset(stAVCConfig, 0, sizeof(stAVCConfig));

	for (int i = 0; i < AF_NALU_MAX; i++)
	{
		if (i == AF_NALU_VPS)
			continue;

		stAVCConfig[i].size = pNaluLength[i];
		stAVCConfig[i].data = (char *)ppNaluData[i];

		if (i == AF_NALU_SPS)
		{
			AVCState avc;
			s32 idx = gf_media_avc_read_sps((char*)ppNaluData[i], pNaluLength[i], &avc, 0, NULL);
			stAVCConfig[i].id = idx;
			if(m_vParam.width != avc.sps[idx].width)
				m_vParam.width = avc.sps[idx].width;
			if(m_vParam.height != avc.sps[idx].height)
				m_vParam.height = avc.sps[idx].height;

			gf_list_add(pstAVCConfig->sequenceParameterSets, &stAVCConfig[i]);
		}
		else if (i == AF_NALU_PPS)
		{
			gf_list_add(pstAVCConfig->pictureParameterSets, &stAVCConfig[i]);
		}
	}
	gf_isom_set_visual_info(m_pFile, m_VnTrackID, m_VnStreamIndex, m_vParam.width, m_vParam.height);
	gf_isom_avc_config_update(m_pFile, m_VnTrackID, 1, pstAVCConfig);

	//use local stack variable, so no need to free
	gf_list_del(pstAVCConfig->sequenceParameterSets);
	gf_list_del(pstAVCConfig->pictureParameterSets);
	pstAVCConfig->pictureParameterSets = NULL;
	pstAVCConfig->sequenceParameterSets = NULL;
	gf_odf_avc_cfg_del(pstAVCConfig);
	pstAVCConfig = NULL;

	return 0;
}

int MP4Writer::WriteH265MetaData(unsigned char **ppNaluData, int *pNaluLength)
{
	NewVideoTrack();
	
	GF_HEVCConfig *pstHEVCConfig = gf_odf_hevc_cfg_new();

	gf_isom_hevc_config_new(m_pFile, m_VnTrackID, pstHEVCConfig, NULL, NULL, &m_VnStreamIndex);
	gf_isom_set_nalu_extract_mode(m_pFile, m_VnTrackID, GF_ISOM_NALU_EXTRACT_INSPECT);
	gf_isom_set_cts_packing(m_pFile, m_VnTrackID, GF_TRUE);

	pstHEVCConfig->configurationVersion = 1;

	HEVCState hevc;
	memset(&hevc, 0 ,sizeof(HEVCState));

	GF_AVCConfigSlot stAVCConfig[AF_NALU_MAX];
	memset(stAVCConfig, 0, sizeof(stAVCConfig));
	GF_HEVCParamArray stHEVCNalu[AF_NALU_MAX];
	memset(stHEVCNalu, 0, sizeof(stHEVCNalu));

	int idx = 0;
	for (int i = 0; i < AF_NALU_MAX; i++)

	{
		switch (i)
		{
			case AF_NALU_SPS:
				idx = gf_media_hevc_read_sps((char*)ppNaluData[i], pNaluLength[i], &hevc);
				hevc.sps[idx].crc = gf_crc_32((char*)ppNaluData[i], pNaluLength[i]);
				pstHEVCConfig->profile_space = hevc.sps[idx].ptl.profile_space;
				pstHEVCConfig->tier_flag = hevc.sps[idx].ptl.tier_flag;
				pstHEVCConfig->profile_idc = hevc.sps[idx].ptl.profile_idc;
				break;
			case AF_NALU_PPS:
				idx = gf_media_hevc_read_pps((char*)ppNaluData[i], pNaluLength[i], &hevc);
				hevc.pps[idx].crc = gf_crc_32((char*)ppNaluData[i], pNaluLength[i]);
				break;
			case AF_NALU_VPS:
				idx = gf_media_hevc_read_vps((char*)ppNaluData[i], pNaluLength[i], &hevc);
				hevc.vps[idx].crc = gf_crc_32((char*)ppNaluData[i], pNaluLength[i]);
				pstHEVCConfig->avgFrameRate = hevc.vps[idx].rates[0].avg_pic_rate;
				pstHEVCConfig->constantFrameRate = hevc.vps[idx].rates[0].constand_pic_rate_idc;
				pstHEVCConfig->numTemporalLayers = hevc.vps[idx].max_sub_layers;
				pstHEVCConfig->temporalIdNested = hevc.vps[idx].temporal_id_nesting;
				break;
		}

		stHEVCNalu[i].nalus = gf_list_new();
		gf_list_add(pstHEVCConfig->param_array, &stHEVCNalu[i]);
		stHEVCNalu[i].array_completeness = 1;
		stHEVCNalu[i].type = g_HEVC_NaluMap[i];
		stAVCConfig[i].id = idx;
		stAVCConfig[i].size = pNaluLength[i];
		stAVCConfig[i].data = (char *)ppNaluData[i];
		gf_list_add(stHEVCNalu[i].nalus, &stAVCConfig[i]);
	}

	gf_isom_set_visual_info(m_pFile, m_VnTrackID, m_VnStreamIndex, hevc.sps[idx].width, hevc.sps[idx].height);
	gf_isom_hevc_config_update(m_pFile, m_VnTrackID, 1, pstHEVCConfig);

	for (int i = 0; i < AF_NALU_MAX; i++)

	{
		if (stHEVCNalu[i].nalus)
			gf_list_del(stHEVCNalu[i].nalus);
	}

	//use local stack variable, so no need to free
	gf_list_del(pstHEVCConfig->param_array);
	pstHEVCConfig->param_array = NULL;
	gf_odf_hevc_cfg_del(pstHEVCConfig);
	pstHEVCConfig = NULL;
	
	return 0;
}

int MP4Writer::WriteVFrame(unsigned char *pData, int nSize, bool bKey, uint64_t nTimeStamp)
{		
	if (m_VTimeStamp == -1 && bKey)
	{
		m_VTimeStamp = nTimeStamp;
	}
	
	if (m_VTimeStamp != -1)
	{	
		GF_ISOSample *pISOSample = gf_isom_sample_new();
		pISOSample->IsRAP = (SAPType)bKey;
		pISOSample->dataLength = nSize;
		pISOSample->data = (char *)pData;
		pISOSample->DTS = nTimeStamp - m_VTimeStamp;
		pISOSample->CTS_Offset = 0; //ignore B frame here; FIXME: need to improve in future
		GF_Err gferr = gf_isom_add_sample(m_pFile, m_VnTrackID, m_VnStreamIndex, pISOSample);			
		if (gferr == -1)
		{
			pISOSample->DTS = nTimeStamp - m_VTimeStamp + 1000 / (m_vParam.fps * 2);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("readjust dts: %lld\n", pISOSample->DTS));
			gf_isom_add_sample(m_pFile, m_VnTrackID, m_VnStreamIndex, pISOSample);
		}
		// printf("dts: %lld, cts offset: %d\n", pISOSample->DTS, pISOSample->CTS_Offset);

		pISOSample->data = NULL;
		pISOSample->dataLength = 0;
		gf_isom_sample_del(&pISOSample);
		pISOSample = NULL;
	}
	
	return 0;
}

int MP4Writer::WriteAFrame(unsigned char *pData, int nSize, uint64_t nTimeStamp)
{
	int res = 0;	
	if (m_ATimeStamp == -1)
	{
		m_ATimeStamp = nTimeStamp;
	}
	
	if (m_ATimeStamp != -1)
	{	
		GF_ISOSample *pISOSample = gf_isom_sample_new();
		pISOSample->IsRAP = RAP;
		pISOSample->dataLength = nSize;
		pISOSample->data = (char *)pData;
		pISOSample->DTS = nTimeStamp - m_ATimeStamp;
		pISOSample->CTS_Offset = 0;
		// pISOSample->nb_pack = 0;
		GF_Err gferr = gf_isom_add_sample(m_pFile, m_AnTrackID, m_AnStreamIndex, pISOSample);			
		// printf("add sample[len:%d, dts:%ld] ret: %d\n", pISOSample->dataLength, pISOSample->DTS, gferr);
		if (gferr == -1)
		{
			res = -1;
		}
		// printf("dts: %lld, cts offset: %d\n", pISOSample->DTS, pISOSample->CTS_Offset);

		pISOSample->data = NULL;
		pISOSample->dataLength = 0;
		gf_isom_sample_del(&pISOSample);
		pISOSample = NULL;
	}
	
	return res;
}