#ifndef V4L2_HEADER
#define V4L2_HEADER
#include <linux/videodev2.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
#endif
#include <cstdio>

class V4L2
{
public:
    V4L2();
    ~V4L2();
    int get_width();
    int get_height();
    bool set_size(int mWidth, int mHeight);
    void get_size_info();
    int get_caps();
    bool init_dev(const char * devName, int mWidth, int mHeight);
    bool init_mmap();
    bool start_stream();
    bool read_frame(AVPicture & pPictureDes, AVPixelFormat FMT, int widht_des, int height_des);
    bool stop_stream();

private:
    struct buffer {
        void   *start;
        size_t  length;
    };

    int mFd;
    buffer* mBuffers;
    int mWidth;
    int mHeight;
    struct v4l2_format mV4l2Fmt;
};
#endif
