#include <fcntl.h>
extern "C" {
#include <libswscale/swscale.h>
}
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "v4l2.h"

#define INIT_MEM(x) memset(&(x), 0, sizeof(x))

V4L2::V4L2()
{
    mFd = -1;
    mBuffers = NULL;
    mWidth = 0;
    mHeight = 0;
    INIT_MEM(mV4l2Fmt);
    mV4l2Fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mV4l2Fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    mV4l2Fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
}

V4L2::~V4L2()
{
    close(mFd);
}

int V4L2::get_width()
{
    return mWidth;
}

int V4L2::get_height()
{
    return mHeight;
}

bool V4L2::set_size(int mWidth, int mHeight)
{
    mV4l2Fmt.fmt.pix.width = mWidth;
    mV4l2Fmt.fmt.pix.height = mHeight;
    if (ioctl(mFd, VIDIOC_S_FMT, &mV4l2Fmt) == -1)
    {
        printf("Can not VIDIOC_S_FMT\n");
        return false;
    }
    get_size_info();
    return true;
}

void V4L2::get_size_info()
{
    if (ioctl(mFd, VIDIOC_G_FMT, &mV4l2Fmt) == -1)
    {
        printf("Can not VIDIOC_G_FMT\n");
        return;
    }
    this->mWidth = mV4l2Fmt.fmt.pix.width;
    this->mHeight = mV4l2Fmt.fmt.pix.height;
}

int V4L2::get_caps()
{
    int ret = -1;
    struct v4l2_fmtdesc fmt;
    INIT_MEM(fmt);
    fmt.index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bool found = false;
    while ((ret = ioctl(mFd, VIDIOC_ENUM_FMT, &fmt)) == 0) 
    {
        fmt.index++;
        printf("pixelformat = ''%c%c%c%c'', description = ''%s''\n",
                fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF, (fmt.pixelformat >> 16) & 0xFF, 
                (fmt.pixelformat >> 24) & 0xFF, fmt.description);
        found = true;
    }

    return (found) ? 0 : -1;
}

bool V4L2::init_dev(const char * devName, int mWidth, int mHeight)
{
    v4l2_capability cap;

    mFd = open(devName, O_RDWR, 0); //打開設備
    if (mFd == -1)
    {
        printf("Can not open %s\n", devName);
        return false;
    }
    if (ioctl(mFd, VIDIOC_QUERYCAP, &cap) == -1) //查詢設備的功能
    {
        printf("Can not get Capability\n");
        return false;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        printf("Can not capture video\n");
        return false;
    }
    if (get_caps() < 0)
    {
        printf("Can not capture caps\n");
        return false;
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        printf("does not support streaming\n");
    }
    if (!set_size(mWidth, mHeight))
    {
        printf("can't set size\n");
        return false;
    }
    printf("mV4l2Fmt.fmt.pix.bytesperline:%d\n", mV4l2Fmt.fmt.pix.bytesperline);

    return init_mmap();
}

bool V4L2::init_mmap()
{
    struct v4l2_requestbuffers req;
    unsigned int n_buffers;
    INIT_MEM(req);

    req.count = 4; //buffer count
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == ioctl(mFd, VIDIOC_REQBUFS, &req))
    {
        if (EINVAL == errno)
        {
            printf("%s does not support memory mapping\n", "ss");
            exit(EXIT_FAILURE);
        } else
            printf("VIDIOC_REQBUFS\n");
    }

    mBuffers = (buffer *) calloc(req.count, sizeof(buffer));
    if (!mBuffers)
    {
        printf("Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; n_buffers++)
    {
        struct v4l2_buffer buf;// A frame buffer.
        INIT_MEM(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (-1 == ioctl(mFd, VIDIOC_QUERYBUF, &buf))
            printf("VIDIOC_QUERYBUF");

        mBuffers[n_buffers].length = buf.length;
        mBuffers[n_buffers].start = mmap(NULL, buf.length,
                PROT_READ | PROT_WRITE,
                MAP_SHARED, mFd, buf.m.offset);

        if (MAP_FAILED == mBuffers[n_buffers].start)
            printf("fail mmap\n");
    }
    return 1;
}

bool V4L2::start_stream()
{
    unsigned int n_buffers;
    enum v4l2_buf_type type;

    for (n_buffers = 0; n_buffers < 4; n_buffers++)
    {
        v4l2_buffer buf;
        INIT_MEM(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (-1 == ioctl(mFd, VIDIOC_QBUF, &buf))   //放入緩存
        {
            printf("fail VIDIOC_QBUF");
            return false;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == ioctl(mFd, VIDIOC_STREAMON, &type))   //打開視頻流
    {
        printf("fail VIDIOC_STREAMON\n");
        return false;
    } else
        printf("StreamOn success!\n");
    return true;
}

bool V4L2::read_frame(AVPicture & pPictureDes, AVPixelFormat FMT, int widht_des,
        int height_des)
{
    v4l2_buffer buf;
    AVPicture pPictureSrc;
    SwsContext * pSwsCtx;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (-1 == ioctl(mFd, VIDIOC_DQBUF, &buf))
    {
        printf("fail VIDIOC_DQBUF\n");
        return false;
    }
    pPictureSrc.data[0] = (unsigned char *) mBuffers[buf.index].start;
    pPictureSrc.data[1] = pPictureSrc.data[2] = pPictureSrc.data[3] = NULL;
    pPictureSrc.linesize[0] = mV4l2Fmt.fmt.pix.bytesperline;
    int i = 0;
    for (i = 1; i < 8; i++)
    {
        pPictureSrc.linesize[i] = 0;
    }
    pSwsCtx = sws_getContext(mWidth, mHeight, PIX_FMT_YUYV422, widht_des,
            height_des, FMT,
            SWS_BICUBIC, 0, 0, 0);
    int rs = sws_scale(pSwsCtx, pPictureSrc.data, pPictureSrc.linesize, 0,
            mHeight, pPictureDes.data, pPictureDes.linesize);
    if (rs == -1)
    {
        printf("Can open to change to des image");
        return false;
    }
    sws_freeContext(pSwsCtx);
    if (-1 == ioctl(mFd, VIDIOC_QBUF, &buf))
    {
        printf("fail VIDIOC_QBUF\n");
        return false;
    }
    return true;
}

bool V4L2::stop_stream()
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(mFd, VIDIOC_STREAMOFF, &type))
    {
        perror("Fail to ioctl 'VIDIOC_STREAMOFF'");
        return false;
    }
    return true;
}
