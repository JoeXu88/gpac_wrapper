// #include "MP4Writer.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

// #define _V1

#ifdef _V1
#include "MP4Writer_v1.h"
#else
#include "MP4WriterInter.h"
#endif

const uint32_t MAX_BUF_SIZE = 500000;


#define FILE_PREFIX "./test_files/"
// #define FILE_PREFIX "/root/windev/thirdparty/test_files/"
static char* IN_SIZE_FILE_NAME = FILE_PREFIX"stream2_h265_size.txt";
static char* IN_DATA_FILE_NAME_265 = FILE_PREFIX"stream2.h265";
static char* IN_DATA_FILE_NAME_264 = FILE_PREFIX"stream1.h264";
static char* AIN_SIZE_FILE_NAME = FILE_PREFIX"audio_aac_pktsize_16k_pkt.txt";
static char* IN_DATA_FILE_NAME_AAC = FILE_PREFIX"audio-h265_16k.aac";

static char* OUT_FILE_NAME_265 = FILE_PREFIX"gpac_265_test.mp4";
static char* OUT_FILE_NAME_264 = FILE_PREFIX"gpac_264_test.mp4";

int test_h265()
{
    uint8_t *buf = new uint8_t[MAX_BUF_SIZE];

#ifdef _V1
    void* mp4hdl = MP4_Init();
    if(mp4hdl == NULL)
#else
    AFMP4Handle mp4hdl = MP4WriterInit();
    if(mp4hdl == 0)
#endif
    {
        printf("can not create mp4 writer\n");
        return -1;
    }

#ifdef _V1
    if(MP4_CreatFile(mp4hdl, OUT_FILE_NAME))
#else
    if(MP4WriterCreateFile(mp4hdl, OUT_FILE_NAME_265, AF_MP4_VIDEO_H265, 1920, 960, 25))
#endif
    {
        printf("cant not create mp4 write file\n");
#ifdef _V1
        MP4_Exit(mp4hdl);
#else
        MP4WriterExit(mp4hdl);
#endif
        return -1;
    }

#ifdef _V1
    if(MP4_InitVideo265(mp4hdl, 1000))
    {
        printf("cant not init 265\n");
        MP4_Exit(mp4hdl);
        return -1;
    }
#endif

    FILE* sizein_file = fopen(IN_SIZE_FILE_NAME, "rb");
    if(sizein_file == 0)
    {
        printf("can not open size file for reading\n");
        return -1;
    }

    FILE* infile = fopen(IN_DATA_FILE_NAME_265, "rb");
    if(infile == 0)
    {
        printf("can not open 265 file for reading\n");
        return -1;
    }

    int pts = 0;
    const int LINE_LEN = 100;
    char line_str[LINE_LEN] = {0};
    while(true)
    {
        fgets(line_str, LINE_LEN, sizein_file);
        int len = atoi(line_str);

        // printf("get data size: I(%d), key:%d, s->%s", len, bkey, line_str);
        int ret = fread(buf, 1, len, infile);
        if(ret != len || len <= 0)
        {
            printf("read src file finish\n");
            break;
        }

#ifdef _V1
        if(MP4_Write265Sample(mp4hdl, buf, len, pts))
#else
        if(MP4WriterWriteFile(mp4hdl, buf, len, pts))
#endif
        {
            printf("write sample err\n");
#ifdef _V1
            MP4_Exit(mp4hdl);
#else
            MP4WriterExit(mp4hdl);
#endif
            return -1;
        }
        pts += 40;
    }

    printf("exit while, going to close file\n");

#ifdef _V1
    MP4_CloseFile(mp4hdl);
    printf("close file done\n");
    MP4_Exit(mp4hdl);
    printf("exit done\n");
#else
    MP4WriterSaveFile(mp4hdl);
    MP4WriterExit(mp4hdl);
#endif
    return 0;
}

int test_file()
{
    uint8_t *buf = new uint8_t[MAX_BUF_SIZE];

    MP4Writer writer;
    if(writer.Create(OUT_FILE_NAME_265))
    {
        printf("cant not create mp4 file:%s\n", OUT_FILE_NAME_265);
        return -1;
    }

    writer.AddVideoTrack(AF_MP4_VIDEO_H265, 0, 0);
    writer.AddAudioTrack(AF_MP4_AUDIO_AAC, 16000, 1, 16);

    FILE* vsizein_file = fopen(IN_SIZE_FILE_NAME, "rb");
    if(vsizein_file == 0)
    {
        printf("can not open size file for reading\n");
        return -1;
    }

    FILE* asizein_file = fopen(AIN_SIZE_FILE_NAME, "rb");
    if(asizein_file == 0)
    {
        printf("can not open audio size file for reading\n");
        return -1;
    }

    FILE* vinfile = fopen(IN_DATA_FILE_NAME_265, "rb");
    if(vinfile == 0)
    {
        printf("can not open 265 file for reading\n");
        return -1;
    }

    FILE* ainfile = fopen(IN_DATA_FILE_NAME_AAC, "rb");
    if(ainfile == 0)
    {
        printf("can not open aac data file for reading\n");
        return -1;
    }

    int vpts = 0, apts = 0;
    const int LINE_LEN = 100;
    char line_str[LINE_LEN] = {0};
    int len = 0;
    int ret = -1;
    bool audio_finish = false;
    bool video_finish = false;
    while(true)
    {
        //write video
        #if 1
        if(!video_finish)
        {
            fgets(line_str, LINE_LEN, vsizein_file);
            len = atoi(line_str);

            // printf("get video data size: I(%d), key:%d, s->%s", len, bkey, line_str);
            ret = fread(buf, 1, len, vinfile);
            if(ret != len || len <= 0)
            {
                video_finish = true;
                printf("read video src file finish\n");
                if(audio_finish) break;
            }

            if(!video_finish && writer.WriteVideo(buf, len, vpts))
            {
                printf("wirte video frame error\n");
                return 0;
            }
            vpts += 40;
        }
        #endif

        if(!audio_finish)
        {
            fgets(line_str, LINE_LEN, asizein_file);
            sscanf(line_str, "pkt_size=\"%d\"", &len);

            // printf("get audio data size: %d, s->%s", len, line_str);
            ret = fread(buf, 1, len, ainfile);
            if(ret != len || len <= 0)
            {
                audio_finish = true;
                printf("read audio src file finish\n");
                if(video_finish) break;
            }

            if(!audio_finish && writer.WriteAudio(buf, len, apts))
            {
                printf("wirte audio frame error\n");
                return 0;
            }
            apts += 64;
        }
    }

    return 0;
}

int test_h264()
{
    uint8_t *buf = new uint8_t[MAX_BUF_SIZE];

#ifdef _V1
    void* mp4hdl = MP4_Init();
    if(mp4hdl == NULL)
#else
    AFMP4Handle mp4hdl = MP4WriterInit();
    if(mp4hdl == 0)
#endif
    {
        printf("can not create mp4 writer\n");
        return -1;
    }

#ifdef _V1
    if(MP4_CreatFile(mp4hdl, OUT_FILE_NAME))
#else
    if(MP4WriterCreateFile(mp4hdl, OUT_FILE_NAME_264, AF_MP4_VIDEO_H264, 640, 480, 25))
#endif
    {
        printf("cant not create mp4 write file\n");
#ifdef _V1
        MP4_Exit(mp4hdl);
#else
        MP4WriterExit(mp4hdl);
#endif
        return -1;
    }

#ifdef _V1
    if(MP4_InitVideo265(mp4hdl, 1000))
    {
        printf("cant not init 265\n");
        MP4_Exit(mp4hdl);
        return -1;
    }
#endif

    FILE* infile = fopen(IN_DATA_FILE_NAME_264, "rb");
    if(infile == 0)
    {
        printf("can not open 264 file for reading\n");
        return -1;
    }

    uint64_t pts = 0;
    int ret = 0;
    int len = 0;
    int result = 0;
    bool endOfFile = false;
    while(!endOfFile)
    {
        ret = fread(&len, sizeof(int), 1, infile);
		if (ret == 1)
		{
            uint64_t tmp_pts = 0;
			ret = fread(&tmp_pts, sizeof(uint64_t), 1, infile);
			if (ret == 1)
			{
				ret = fread(buf, sizeof(uint8_t), len, infile);
				if (ret == len)
                {
                    // printf("start to write: len:%d, pts:%lld\n", len, pts);
					result = MP4WriterWriteFile(mp4hdl, buf, len, pts);
                    // printf("MP4WriterWriteFile ret=> len:%d, pts:%lld, ret: %d\n", len, pts, result);
                    if(len > 25) pts += 40;
                    // if(pts == 154) break;
                }
				else
					result = -1;
			}
			else
				result = -1;
		}
		else
			endOfFile = true;

		if (result != 0)
        {
            printf("write sample err\n");
            MP4WriterExit(mp4hdl);
			return -1;
        }
    }

    printf("exit while, going to close file\n");

#ifdef _V1
    MP4_CloseFile(mp4hdl);
    printf("close file done\n");
    MP4_Exit(mp4hdl);
    printf("exit done\n");
#else
    MP4WriterSaveFile(mp4hdl);
    MP4WriterExit(mp4hdl);
#endif

    delete []buf;
    return 0;
}

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("usage: ./test 264 or 265\n");
        return -1;
    }

    const std::string option_264 = "264";
    const std::string option_265 = "265";
    const std::string option_file = "file";

    if(!strncmp(argv[1], option_264.c_str(), option_264.length()))
    {
        printf("option:%s, test 264 mp4 package\n", argv[1]);
        test_h264();
    }
    else if(!strncmp(argv[1], option_265.c_str(), option_265.length()))
    {
        printf("option:%s, test 265 mp4 package\n", argv[1]);
        test_h265();
    }
    else if(!strncmp(argv[1], option_file.c_str(), option_file.length()))
    {
        printf("option:%s, test file mp4 package\n", argv[1]);
        test_file();
    }

    return 0;
}