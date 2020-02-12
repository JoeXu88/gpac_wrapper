## gpac_wrapper
use gpac to wrapper mp4 container

gpac repo: https://github.com/gpac/gpac

this wrapper is refered to blog [使用gpac封装mp4](https://blog.csdn.net/weixin_43549602/article/details/84571906) [gpac录制h264/h265 + aac 的mp4文件](https://blog.csdn.net/tifentan/article/details/102807217)
Note: when add MP4 video sample data, its first 4 bytes is real data size, which will take off naul start code size(3 or 4 bytes).
for example: 00 00 00 01 realdata[real_size] is the frame data
what to do: replace '00 00 00 01' with real_size(must be big endian mode)
what to feed to mp4 box: sample.data = data_starts_with_4bytes_realsize;  sample.length = data_real_size + 4(first 4 byte for size);

But I try to make it more strong and fix some bugs.

Now it only supports h264 and h265, with AAC audio.
