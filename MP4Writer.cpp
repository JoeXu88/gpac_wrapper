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

MP4Writer::MP4Writer()
{
	m_enEncType = AF_MP4_ENC_NONE;
	m_nWidth = -1;
	m_nHeight = -1;

	m_VTimeStamp = -1;
	m_ATimeStamp = -1;

	m_VbReady = false;
	m_AbReady = false;

	m_pFile = NULL;
	
	m_VnTrackID = -1;
	m_AnTrackID = -1;
	m_nStreamIndex = 0;
	
	memset(m_pNaluData, 0, sizeof(m_pNaluData));
	memset(m_pNaluLength, 0, sizeof(m_pNaluLength));
    // gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_DEBUG);

}

MP4Writer::~MP4Writer()
{
	Save();
}

int MP4Writer::Create(char *strName, MP4_ENC_TYPE nEncType, int nWidth, int nHeight, int nFps)
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

	m_enEncType = nEncType;
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_nFps = nFps;
	return 0;
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
	m_VbReady = false;
	
	return 0;
}

int MP4Writer::WriteVideo(unsigned char *pData, int nSize, uint64_t nTimeStamp)
{
	if (m_pFile && m_enEncType & AF_MP4_WITH_VIDEO)
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

	return 0;
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
		WriteFrame(pBuf, nBufSize, bKey, nTimeStamp);

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
		WriteFrame(pBuf, nBufSize, bKey, nTimeStamp);
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
	m_VnTrackID = gf_isom_new_track(m_pFile, 0, GF_ISOM_MEDIA_VISUAL, DEFAULT_TIME_SCALE_V);
	gf_isom_set_track_enabled(m_pFile, m_VnTrackID, 1);
	// gf_isom_set_composition_offset_mode(m_pFile, m_VnTrackID, GF_FALSE);
	// gf_isom_rewrite_track_dependencies(m_pFile, 1);

	return 0;
}

int MP4Writer::WriteH264MetaData(unsigned char **ppNaluData, int *pNaluLength)
{	
	NewVideoTrack();

	GF_AVCConfig *pstAVCConfig = gf_odf_avc_cfg_new();	

	gf_isom_avc_config_new(m_pFile, m_VnTrackID, pstAVCConfig, NULL, NULL, &m_nStreamIndex);
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
			if(m_nWidth <= avc.sps[idx].width)
				m_nWidth = avc.sps[idx].width;
			if(m_nHeight <= avc.sps[idx].height)
				m_nHeight = avc.sps[idx].height;

			gf_list_add(pstAVCConfig->sequenceParameterSets, &stAVCConfig[i]);
		}
		else if (i == AF_NALU_PPS)
		{
			gf_list_add(pstAVCConfig->pictureParameterSets, &stAVCConfig[i]);
		}
	}
	gf_isom_set_visual_info(m_pFile, m_VnTrackID, m_nStreamIndex, m_nWidth, m_nHeight);
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

	gf_isom_hevc_config_new(m_pFile, m_VnTrackID, pstHEVCConfig, NULL, NULL, &m_nStreamIndex);
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

	gf_isom_set_visual_info(m_pFile, m_VnTrackID, m_nStreamIndex, hevc.sps[idx].width, hevc.sps[idx].height);
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

int MP4Writer::WriteFrame(unsigned char *pData, int nSize, bool bKey, uint64_t nTimeStamp)
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
		GF_Err gferr = gf_isom_add_sample(m_pFile, m_VnTrackID, m_nStreamIndex, pISOSample);			
		if (gferr == -1)
		{
			pISOSample->DTS = nTimeStamp - m_VTimeStamp + 1000 / (m_nFps * 2);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("readjust dts: %lld\n", pISOSample->DTS));
			gf_isom_add_sample(m_pFile, m_VnTrackID, m_nStreamIndex, pISOSample);
		}
		// printf("dts: %lld, cts offset: %d\n", pISOSample->DTS, pISOSample->CTS_Offset);

		pISOSample->data = NULL;
		pISOSample->dataLength = 0;
		gf_isom_sample_del(&pISOSample);
		pISOSample = NULL;
	}
	
	return 0;
}
