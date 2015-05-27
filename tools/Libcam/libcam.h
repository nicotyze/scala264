#ifndef __LIBCAM__H__
#define __LIBCAM__H__

struct buffer {
        void *                  start;
        size_t                  length;
};

typedef enum {
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR
} io_method;


class Camera {
private:
  void Open();
  void Close();

  void Init();
  void UnInit();

  void Start();
  void Stop();

  void init_mmap();

  bool initialised;


public:
  const char *name;  //dev name
  int width;
  int height;
  int fps;

  int w2;

  unsigned char *data;

  io_method io;
  int fd;
  buffer *buffers;
  int n_buffers;

  Camera(const char *name, int w, int h, int fps=30);
  ~Camera();

  unsigned char *Get();

  void StopCam();

};

#endif
