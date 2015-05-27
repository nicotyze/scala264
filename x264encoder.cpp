#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "x264encoder.h"

extern "C"{

#include "inttypes.h"
#include "x264.h"
}

#define BILLION 1000000000L
#define FPS_RATIO 2 // 1-> one quality layer, 2-> two quality layers

#if FROM_CAM
#include "libcam.h"
#endif

static void fast_yuyv2yuv420(unsigned char * source, unsigned char * dest, int xsize, int ysize)
{
	int i, j, w2;
	unsigned char * y, * u, * v;

    w2 = xsize / 2;

    // I420
	y = dest;
	u = dest + xsize * ysize;
	v = dest + xsize * ysize * 5 / 4;

	for(i = 0; i < ysize; i += 2)
	{
		for (j = 0; j < w2; j++)
		{
			/* YUYV The byte order is CbY'CrY' */
                        *y++ = *source++;
			*u++ = *source++; 
                        *y++ = *source++;
			*v++ = *source++;
		}
		for (j = 0; j < w2; j++)
		{
			/* YUYV The byte order is CbY'CrY' */
                        *y++ = *source++;
                        source++;
                        *y++ = *source++;
                        source++;
		}
         }
}

typedef struct
{
    x264_t *h;
    x264_param_t param;
    x264_picture_t pic;
    x264_picture_t pic_out;

    x264_nal_t *nal;
    int i_nal;
} x264;

x264Encoder::x264Encoder()
{
    x264 *enc;
    enc = (x264 *) malloc(sizeof(x264));
    enc->h = NULL;
    encoder = (void *) enc;
    isFrameEncoded = false;
}

x264Encoder::~x264Encoder()
{
    x264 *enc = (x264 *) encoder;
    if (enc->h != NULL) {
	  x264_encoder_close(enc->h);
//	  x264_picture_clean(&(enc->pic));
    }
    free(enc);
}

bool x264Encoder::init(int w, int h, int bps, int fps)
{
    x264 *enc = (x264 *) encoder;
    x264_param_t *param = &(enc->param);
    x264_param_default(param);
    // the speed preset here should probably be an option
    x264_param_default_preset(param, "ultrafast", "zerolatency");

    param->i_log_level = 0;//X264_LOG_DEBUG;
    // necessary stuff
    // * seting rate control
    //param->rc.i_bitrate = bps;
    param->i_threads=1;
    param->i_lookahead_threads = 1;
    param->rc.i_rc_method = X264_RC_CQP;
    param->rc.i_qp_constant=10;
    param->rc.i_qp_min=1;
    param->rc.i_qp_max=51;
    param->rc.f_ip_factor=0;
    param->rc.i_qp_step=20;
    param->rc.b_mb_tree = 0;

    param->i_width = w;
    param->i_height = h;

    param->i_keyint_max =30;// !! must be even... !!
    param->i_keyint_min =30;// !! must be even... !!
    param->i_scenecut_threshold=0;

    param->i_fps_num = fps * 1000;
    param->i_fps_den = 1000;

    param->i_timebase_den = fps * 1000;
    param->i_timebase_num = 1000;


    param->b_annexb = 1;

    //param->b_intra_refresh = 1;
    //param->i_slice_max_size = 1300;

    param->i_bframe = 0;
    param->i_frame_reference=2;

#if 1
    x264_picture_alloc(&(enc->pic), X264_CSP_I420, param->i_width,
                       param->i_height);
#else
    x264_picture_init(&(enc->pic));
    enc->pic.img.i_csp = X264_CSP_I420;
    enc->pic.img.i_plane = 3;
    for(int i=0; i<3;i++){
      enc->pic.img.i_stride[i] = w;
    }
    enc->pic.img.plane[0] = (uint8*)malloc(w*h);
    enc->pic.img.plane[1] = (uint8*)malloc(w*h/4);
    enc->pic.img.plane[2] = (uint8*)malloc(w*h/4);
#endif

    x264_t *handle = x264_encoder_open(param);

    if (handle != NULL) {
        enc->h = handle;
        return true;
    }
    else {
        return false;
    }
}

bool x264Encoder::encodeFrame(uint8 *buf)
{
    x264 *enc = (x264 *) encoder;
    x264_param_t *param = &(enc->param);

    int frame_size = param->i_width * param->i_height;

    //refresh
    enc->i_nal = 0;
    enc->pic.img.plane[0] = buf;
    enc->pic.img.plane[1] = buf + frame_size;
    enc->pic.img.plane[2] = buf + frame_size*5/4;

    int result = x264_encoder_encode(enc->h, &(enc->nal), &(enc->i_nal), &(enc->pic), &(enc->pic_out));

    if (result < 0) {
	  isFrameEncoded = false;
	  return false;
    }
    else {
	  isFrameEncoded = true;
	  return true;
    }
}

int x264Encoder::numNAL()
{
    if (isFrameEncoded) {
	x264 *enc = (x264 *) encoder;
	return enc->i_nal;
    }
    else
	return 0;
}

void x264Encoder::writeNALPacket(int idx, FILE *str_file)
{
    x264 *enc = (x264 *) encoder;
    if (!isFrameEncoded || idx >= enc->i_nal)
	return ;

    int packetSize;

    packetSize = enc->nal[idx].i_payload;
    //f->write( (char*)enc->nal[idx].p_payload, packetSize );
    fwrite(enc->nal[idx].p_payload,1 , packetSize,str_file);
    fflush(stdout);

}

void x264Encoder::setGOP(int gop)
{
    x264 *enc = (x264 *) encoder;
    x264_param_t *param = &(enc->param);
    param->i_keyint_max = 2*gop;
    param->i_keyint_min = gop;
}

void x264Encoder::setBitRate(int br)
{
    x264 *enc = (x264 *) encoder;
    x264_param_t *param = &(enc->param);
    param->rc.i_bitrate = br;
}

void x264Encoder::setFPS(int fps)
{
    x264 *enc = (x264 *) encoder;
    x264_param_t *param = &(enc->param);
    param->i_fps_num = fps * 1000;
    param->i_fps_den = 1000;
}

void x264Encoder::setQP(int qp)
{
    x264 *enc = (x264 *) encoder;
    enc->pic.i_qpplus1 = qp+1;
}

void x264Encoder::setPicType(int pic_type)
{
    x264 *enc = (x264 *) encoder;
    enc->pic.i_type = pic_type;
}

bool x264Encoder::isInitialized()
{
    x264 *enc = (x264 *) encoder;
    if (enc->h == NULL)
	return false;
    else
	return true;
}


int main(int argc, char *argv[])
{

  int fps = 15;
  int N =  50;
  int frame_size; 
  uint8 *yuv_frame;
  int fr_cnt=0;
  int in_cnt=0;
  int read_ret=0;
  int width = 640;
  int height = 480;
  int QP0 = 40;
  int delta_qp=15;
  int qp=QP0;
  int duration;

  if( argc != 4){
    //printf("Expect input YUV file name as argument \n");
    printf("Usage: %s input_filename  output_filename duration\n",argv[0]);
    printf("Formats:  input_filename -> YUV4:2:0, output_filename -> H.264/AVC, duration->seconds (0 => infinite loop) \n");
    printf("Example: %s pattern_640x480_15fps.yuv  out.264 10\n",argv[0]);
    return 0;
  }
  
  FILE * str_out = fopen(argv[2], "wb");
  duration = atoi(argv[3]);

#if FROM_CAM
  Camera c(argv[1], width, height, fps);
#else
  FILE * raw_in = fopen(argv[1], "rb");
#endif

  ////////////// timer inits ////////////////////////////////////
  struct timespec curr_frame, last_frame, wait_time;
  uint64_t diff,enc_time;
  uint64_t frame_dur = BILLION /fps/FPS_RATIO;//in nano seconds
  
  wait_time.tv_sec = 0;
  clock_gettime(CLOCK_MONOTONIC, &curr_frame);
  last_frame = curr_frame ;
  enc_time = 0;
  //////////////////////////////////////////////////////////////

  x264Encoder enc;
  
  frame_size = width * height*3/2 ;
  yuv_frame = (uint8*)malloc(frame_size);

  enc.init(width,height,800,fps*FPS_RATIO);

  
  while( duration == 0 || fr_cnt < duration*fps){
    if( in_cnt%FPS_RATIO==0){
#if FROM_CAM
      c.Get();
      fast_yuyv2yuv420(c.data, yuv_frame,  width, height);
#else
      read_ret = fread( yuv_frame,1, frame_size,raw_in);
      if( read_ret != frame_size){
        fseek(raw_in , 0 , SEEK_SET);
        read_ret = fread( yuv_frame,1, frame_size,raw_in);
      }
#endif
    }
    qp = in_cnt%FPS_RATIO==0?QP0:QP0-delta_qp;
    enc.setQP(qp);

    for(int i=0; i<enc.numNAL() ;i++){//write encoded packets just after v4l2 frame release....
      enc.writeNALPacket(i, str_out);
    }

    if( enc.encodeFrame( yuv_frame )){
#if FROM_CAM
      // in this case, synchronisation is provided by v4l2 (no need to wait)
#else 
      // waiting loop to synchronise encoding rate on frame rate
      //
      ////////////// timer ////////////////////////////////////
      clock_gettime(CLOCK_MONOTONIC, &curr_frame);
      diff = BILLION *(curr_frame.tv_sec - last_frame.tv_sec) + curr_frame.tv_nsec - last_frame.tv_nsec;//computed in nano seconds
      enc_time = 0.01*diff+0.99*enc_time;
      if( (frame_dur - enc_time) > 0){
        wait_time.tv_nsec = (frame_dur - enc_time);
        nanosleep(&wait_time,NULL);
      }
      clock_gettime(CLOCK_MONOTONIC, &last_frame);
      ////////////////////////////////////////////////////////
#endif
      fr_cnt++;
    }
    in_cnt++;
  }

  free(yuv_frame);  
  fclose(str_out); 
#if FROM_CAM
//
#else 
  fclose(raw_in);
#endif
 
  return 0;
}







