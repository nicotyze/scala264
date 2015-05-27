#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#include "libcam.h"


static void errno_exit (const char *           s)
{
        fprintf (stderr, "%s error %d, %s\n",
                 s, errno, strerror (errno));

        exit (EXIT_FAILURE);
}

static int xioctl(int fd, int request, void *arg)
{
        int r,itt=0;

        do {
		r = ioctl (fd, request, arg);
		itt++;
	}
        while ((-1 == r) && (EINTR == errno) && (itt<100));

        return r;
}

Camera::Camera(const char *n, int w, int h, int f) {
  name=n;
  width=w;
  height=h;
  fps=f;

  w2=w/2;


  io=IO_METHOD_MMAP;

  data=(unsigned char *)malloc(w*h*4);

  this->Open();
  this->Init();
  this->Start();
  initialised = true;
}

Camera::~Camera() {
  this->StopCam();
}

void Camera::StopCam()
{
  if (initialised) {
    this->Stop();
    this->UnInit();
    this->Close();

    free(data);
    initialised = false;
  }
}

void Camera::Open() {
  struct stat st;
  if(-1==stat(name, &st)) {
    fprintf(stderr, "Cannot identify '%s' : %d, %s\n", name, errno, strerror(errno));
    exit(1);
  }

  if(!S_ISCHR(st.st_mode)) {
    fprintf(stderr, "%s is no device\n", name);
    exit(1);
  }

  fd=open(name, O_RDWR | O_NONBLOCK, 0);

  if(-1 == fd) {
    fprintf(stderr, "Cannot open '%s': %d, %s\n", name, errno, strerror(errno));
    exit(1);
  }

}

void Camera::Close() {
  if(-1==close(fd)) {
    errno_exit("close");
  }
  fd=-1;

}

void Camera::Init() {
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;
  unsigned int min;

  if(-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
    if (EINVAL == errno) {
      fprintf(stderr, "%s is no V4L2 device\n",name);
      exit(1);
    } else {
       errno_exit("VIDIOC_QUERYCAP");
    }
  }

  if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    fprintf(stderr, "%s is no video capture device\n", name);
    exit(1);
  }

  switch(io) {
    case IO_METHOD_READ:
      if(!(cap.capabilities & V4L2_CAP_READWRITE)) {
        fprintf(stderr, "%s does not support read i/o\n", name);
        exit (1);
      }

      break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
    if(!(cap.capabilities & V4L2_CAP_STREAMING)) {
      fprintf (stderr, "%s does not support streaming i/o\n", name);
      exit(1);
    }

    break;
  }

  CLEAR (cropcap);

  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if(0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect; /* reset to default */

    if(-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
      switch (errno) {
        case EINVAL:
          /* Cropping not supported. */
          break;
        default:
          /* Errors ignored. */
          break;
        }
      }
    } else {
      /* Errors ignored. */
    }

    CLEAR (fmt);

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;


  if(-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
    errno_exit ("VIDIOC_S_FMT");


  struct v4l2_streamparm p;
  p.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
  p.parm.capture.timeperframe.numerator=1;
  p.parm.capture.timeperframe.denominator=fps;
  p.parm.output.timeperframe.numerator=1;
  p.parm.output.timeperframe.denominator=fps;


  if(-1==xioctl(fd, VIDIOC_S_PARM, &p))
    errno_exit("VIDIOC_S_PARM");

  //default values, mins and maxes
  struct v4l2_queryctrl queryctrl;

  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_BRIGHTNESS;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      printf("brightness error\n");
    } else {
      printf("brightness is not supported\n");
    }
  } else if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("brightness is not supported\n");
  }

  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_CONTRAST;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      printf("contrast error\n");
    } else {
      printf("contrast is not supported\n");
    }
  } else if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("contrast is not supported\n");
  }

  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_SATURATION;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      printf("saturation error\n");
    } else {
      printf("saturation is not supported\n");
    }
  } else if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("saturation is not supported\n");
  }

  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_HUE;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      printf("hue error\n");
    } 
  } 

  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_HUE_AUTO;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      printf("hueauto error\n");
    } 
  } 

  memset(&queryctrl, 0, sizeof(queryctrl));
  queryctrl.id = V4L2_CID_SHARPNESS;
  if(-1 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if(errno != EINVAL) {
      printf("sharpness error\n");
    } else {
      printf("sharpness is not supported\n");
    }
  } else if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("sharpness is not supported\n");
  }

  switch(io) {
    case IO_METHOD_READ:
      break;

    case IO_METHOD_MMAP:
      init_mmap();
      break;

    case IO_METHOD_USERPTR:
      break;
    }

}

void Camera::init_mmap() {
  struct v4l2_requestbuffers req;

  CLEAR (req);

  req.count               = 4;
  req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory              = V4L2_MEMORY_MMAP;

  if(-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
    if(EINVAL == errno) {
      fprintf (stderr, "%s does not support memory mapping\n", name);
      exit (1);
    } else {
      errno_exit ("VIDIOC_REQBUFS");
    }
  }

  if(req.count < 2) {
    fprintf (stderr, "Insufficient buffer memory on %s\n", name);
    exit(1);
  }

  buffers = (buffer *)calloc(req.count, sizeof (*buffers));

  if(!buffers) {
    fprintf(stderr, "Out of memory\n");
    exit(1);
  }

  for(n_buffers = 0; n_buffers < (int)req.count; ++n_buffers) {
    struct v4l2_buffer buf;

    CLEAR (buf);

    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = n_buffers;

    if(-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
      errno_exit ("VIDIOC_QUERYBUF");

    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start = mmap (NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.offset);

    if(MAP_FAILED == buffers[n_buffers].start)
      errno_exit ("mmap");
  }

}

void Camera::UnInit() {
  unsigned int i;

  switch(io) {
    case IO_METHOD_READ:
      free (buffers[0].start);
      break;

    case IO_METHOD_MMAP:
      for(i = 0; i < (unsigned int)n_buffers; ++i)
        if(-1 == munmap (buffers[i].start, buffers[i].length))
          errno_exit ("munmap");
      break;

    case IO_METHOD_USERPTR:
      for (i = 0; i < (unsigned int)n_buffers; ++i)
        free (buffers[i].start);
      break;
  }

  free (buffers);
}

void Camera::Start() {
  unsigned int i;
  enum v4l2_buf_type type;

  switch(io) {
    case IO_METHOD_READ:
      /* Nothing to do. */
      break;

    case IO_METHOD_MMAP:
      for(i = 0; i < (unsigned int)n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if(-1 == xioctl (fd, VIDIOC_QBUF, &buf))
          errno_exit ("VIDIOC_QBUF");
      }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if(-1 == xioctl (fd, VIDIOC_STREAMON, &type))
        errno_exit ("VIDIOC_STREAMON");

      break;

    case IO_METHOD_USERPTR:
      for(i = 0; i < (unsigned int)n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_USERPTR;
        buf.index       = i;
        buf.m.userptr	= (unsigned long) buffers[i].start;
        buf.length      = buffers[i].length;

        if(-1 == xioctl (fd, VIDIOC_QBUF, &buf))
          errno_exit ("VIDIOC_QBUF");
      }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if(-1 == xioctl (fd, VIDIOC_STREAMON, &type))
        errno_exit ("VIDIOC_STREAMON");

      break;
    }

}

void Camera::Stop() {
  enum v4l2_buf_type type;

  switch(io) {
    case IO_METHOD_READ:
      /* Nothing to do. */
      break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if(-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
        errno_exit ("VIDIOC_STREAMOFF");

      break;
  }

}

unsigned char *Camera::Get() {

  struct v4l2_buffer buf;
  fd_set fds;
  struct timeval tv;
  int r;

  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  /* Timeout. */
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  r = select(fd + 1, &fds, NULL, NULL, &tv);

  if (-1 == r) {
    if (EINTR == errno)
      return NULL;
    errno_exit("select");
  }

  if (0 == r) {
    fprintf(stderr, "select timeout\n");
    exit(EXIT_FAILURE);
  }

  switch(io) {
    case IO_METHOD_READ:
      break;

    case IO_METHOD_MMAP:
      CLEAR(buf);

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      if(-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
          case EAGAIN:
            return 0;
          case EIO:
          default:
            return 0; //errno_exit ("VIDIOC_DQBUF");
        }
      }

      assert(buf.index < (unsigned int)n_buffers);

      data = (unsigned char *)buffers[buf.index].start;

      if(-1 == xioctl (fd, VIDIOC_QBUF, &buf))
        return 0; //errno_exit ("VIDIOC_QBUF");

      return data;
      break;

    case IO_METHOD_USERPTR:
      break;
  }

  return 0;
}


