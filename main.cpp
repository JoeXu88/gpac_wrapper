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


#define FILE_PREFIX "/root/windev/thirdparty/test_files/"
static char* IN_SIZE_FILE_NAME = FILE_PREFIX"stream2_h265_size.txt"; //"/root/windev/thirdparty/test_files/stream2_h265_size.txt";
static char* IN_DATA_FILE_NAME_265 = FILE_PREFIX"stream2.h265";
static char* IN_DATA_FILE_NAME_264 = FILE_PREFIX"stream1.h264";
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

    return 0;
}