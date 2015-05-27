#include <stdio.h>
#include <stdlib.h>

#include "YUV420p_GL.h"

extern "C" {
#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif
}

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

extern "C"
{
#include <libavformat/avformat.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#include <libavutil/frame.h>
}

#include <stdbool.h>
#include <math.h>
#include <stdio.h>>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <sys/time.h>
#include <list>

#include <getopt.h>
#include <unistd.h>


#define BUF_LEN 3 // Display buffer lentgh (=>delay between decoding and display)
                  // !!! minimum value: 3 !!!

int layer_id;


#define BILLION 1000000000L

struct fill_th{
        AVFormatContext *pFormatCtx;
        AVCodecContext *pCodecCtx; 	
        int videoStream;
        AVFrame *pFrame;
        std::list<AVFrame *> ref_frame;
        bool b_new_frame;
        bool b_fill_buf;
        pthread_mutex_t new_frame_mutex;
        pthread_cond_t new_frame_cond;
        pthread_mutex_t fill_buff_mutex;
        pthread_cond_t fill_buff_cond;
        float frame_rate;
        uint64_t key_time;
};

struct fill_th *fill_th_data;
YUV420P_GL* player_ptr = NULL;

int GetNextFrame(AVFormatContext *pFormatCtx, AVCodecContext *pCodecCtx, 
    int videoStream, AVFrame *pFrame, uint64_t &key_time )
{
    static AVPacket packet;
    static int      bytesRemaining = 0;
    static uint8_t  *rawData;
    static bool     fFirstTime = true;
    int             bytesDecoded;
    int             frameFinished;
    int err;
    unsigned char *pixel;
     
    // First time we're called, set packet.data to NULL to indicate it
    // doesn't have to be freed
    if (fFirstTime) {
        fFirstTime = false;
        av_init_packet(&packet);
        packet.data = NULL;
    }
    
read_pkt:

    do{
      av_free_packet(&packet);
      err = av_read_frame(pFormatCtx,&packet);
    }
    while( packet.stream_index != videoStream );

        // Work on the current packet until we have decoded all of it
        if (packet.size > 0 and err>=0) {
        	
            // Decode the next chunk of data
            bytesDecoded = avcodec_decode_video2(pCodecCtx, pFrame,
                &frameFinished, &packet);
                pixel = pFrame->data[0];
                
            if (bytesDecoded < 0) {
                fprintf(stderr, "Error while decoding frame\n");
                return -1;
            }
            else{

              if(pFrame->linesize[0]!=0){
                if( pFrame->key_frame  ){
                  key_time = pFrame->pkt_pts;
                }
                return 1;// new frame available
              }
              else{
		return 0;// no decoding errors but no frame....
              }
               
            }
        }
        else{
#if 0 // restart input file reading
          av_seek_frame(pFormatCtx, videoStream, 0, AVSEEK_FLAG_BACKWARD);
          goto read_pkt;
#else
          goto loop_exit;
#endif
        }
    

loop_exit:

    // Decode the rest of the last frame
    bytesDecoded = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, 
       &packet);

    // Free last packet
    if (packet.data != NULL)
        av_free_packet(&packet);

    fprintf(stderr, "finished: %d\n", frameFinished);
    return -1;//frameFinished != 0;
}

void *fill_buffer(void *th_arg){

	struct fill_th *fill_th_data;
	fill_th_data = (fill_th*)th_arg;
        int pic_num;
	int err=0;
        int videoId = fill_th_data->videoStream;
        //struct timespec curr_frame, last_frame,wait_time;
        uint64_t diff,dec_time =0;
	uint64_t frame_dur = BILLION/fill_th_data->frame_rate;//in nano seconds
        int64_t time_stamp, pkt_dur;

        //clock_gettime(CLOCK_MONOTONIC, &curr_frame);
        //last_frame = curr_frame ;
        dec_time = 0;
        AVFrame  *pFrame_prev; 

	while(1){
          while(1){ 
            err = GetNextFrame(fill_th_data->pFormatCtx, fill_th_data->pCodecCtx, fill_th_data->videoStream, fill_th_data->pFrame, fill_th_data->key_time);
            if(err == 0){
              printf(".");
            }
            else{// new frame decoded 
              AVFrame *pFrame_ref = av_frame_alloc();
              av_frame_ref (pFrame_ref,fill_th_data->pFrame); // add a reference to the frame (for display)   
              time_stamp = pFrame_ref->pkt_pts;
              pkt_dur = pFrame_ref->pkt_duration;
              
              pthread_mutex_lock(&fill_th_data->fill_buff_mutex);

                if( time_stamp >= LLONG_MAX  || time_stamp <= LLONG_MIN ){// pic_num computation based on packet timestamps if available
                  pic_num = pFrame_ref->coded_picture_number;
                }
                else{
                  pic_num = ((time_stamp-fill_th_data->key_time)/pkt_dur);// assume that GOP size is even
                }
              //printf("Delta pkt_pts: %lld, Pic num: %i , key_frame: %i  \n", (time_stamp-fill_th_data->key_time)/pkt_dur,pic_num ,pFrame_ref->key_frame );// for debug

                pFrame_prev = fill_th_data->ref_frame.front();
                
                if( pic_num%2==1 ){//  high quality frame
                  if( layer_id == 1){ //  High quality is selected => low quality version is discarded
                    av_frame_unref (pFrame_prev); 	
                    av_frame_free(&pFrame_prev);
                    fill_th_data->ref_frame.pop_front();
                  }
                  else{ //  Low quality is selected => high quality version is discarded
                    av_frame_unref (pFrame_ref); 	
                    av_frame_free(&pFrame_ref);
                    pthread_mutex_unlock(&fill_th_data->fill_buff_mutex);
                    continue;
                  }
                }
                fill_th_data->ref_frame.push_front(pFrame_ref);

                fill_th_data->b_fill_buf = fill_th_data->ref_frame.size() < BUF_LEN; 
                while( fill_th_data->b_fill_buf==0 )
                  pthread_cond_wait(&fill_th_data->fill_buff_cond, &fill_th_data->fill_buff_mutex);
              pthread_mutex_unlock(&fill_th_data->fill_buff_mutex);

              pthread_mutex_lock(&fill_th_data->new_frame_mutex);
                if ( fill_th_data->ref_frame.size()>1){
                  pthread_cond_signal( &fill_th_data->new_frame_cond ); 
                  fill_th_data->b_new_frame=1;
                }
              pthread_mutex_unlock(&fill_th_data->new_frame_mutex);           		
              break;
            }
          }
          if(err == -1){
            break;
          }
        }
	pthread_exit(NULL);
	return 0;
}

///////////// openGL playback ////////////////////////
void display_callback(){
  //player_ptr->drawString("High Quality: press 'p' \n Low Quality: press 'm'");
  player_ptr->display("High Quality: press `p' | Low Quality: press `m' | Exit: press `esc'");
}

void timeFunc(int value){
    int err;
    AVFrame  *pFrame_ref=NULL;
    AVFrame  *pFrame_old=NULL;

    pthread_mutex_lock(&fill_th_data->new_frame_mutex);
      if( fill_th_data->b_new_frame == 0 && fill_th_data->ref_frame.size() <= 1 ){
        printf("############ Display Drop #############\n");// decoding is not real-time
      }
      fill_th_data->b_new_frame =  fill_th_data->ref_frame.size() > 2;
      if(fill_th_data->b_new_frame==0){
            // Timer
            glutTimerFunc(1000/player_ptr->fps, timeFunc, 0);
            glutPostRedisplay();
            pthread_mutex_unlock(&fill_th_data->new_frame_mutex);
            return;
      }
      pFrame_old= fill_th_data->ref_frame.back();
      fill_th_data->ref_frame.pop_back();
      pFrame_ref = (fill_th_data->ref_frame.back());
    pthread_mutex_unlock(&fill_th_data->new_frame_mutex);
//
    pthread_mutex_lock(&fill_th_data->fill_buff_mutex);

      if ( fill_th_data->ref_frame.size()<=BUF_LEN){
        pthread_cond_signal( &fill_th_data->fill_buff_cond );  
        fill_th_data->b_fill_buf = 1;
      }  
    pthread_mutex_unlock(&fill_th_data->fill_buff_mutex);
	               
    if( pFrame_old != NULL ){
      av_frame_unref (pFrame_old); 	
      av_frame_free(&pFrame_old);
    }	
    printf("Buffer length: %lu \n",fill_th_data->ref_frame.size());

    player_ptr->plane[0] = pFrame_ref->data[0];
    player_ptr->plane[1] = pFrame_ref->data[1];
    player_ptr->plane[2] = pFrame_ref->data[2];

    // Timer
    glutTimerFunc(1000/player_ptr->fps, timeFunc, 0);
    glutPostRedisplay();
}
static void key(unsigned char k, int x, int y)
{
  switch (k) {
  case 27:  /* Escape */
    exit(0);
    break;
  case 112:
    layer_id=1;
    break ;
  case 109:
    layer_id=0;
    break ; 
  default:
    return;
  }
  glutPostRedisplay();
}


//////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    AVFormatContext *pFormatCtx;
    AVCodecContext  *pCodecCtx;
    AVCodec         *pCodec;
    AVFrame         *pFrame; 
    AVFrame         *pFrame_ref;
    int             videoStream;

    unsigned char mv_array[1000000];
    
    int write_id=0;
    int read_id=2;
    int th_id = -1;
    pthread_t fill_buff_th;
    int wait_ret=-1;
    int rc, err;

    layer_id = 0;
    // Register all formats and codecs
    avcodec_register_all();
    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();
    // Open video file
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
        return -1; // Couldn't open file

    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx,NULL) < 0)
        return -1; // Couldn't find stream information

    // Find the first video stream
    videoStream = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
	AVCodecContext *cc = pFormatCtx->streams[i]->codec;
        if (cc->codec_type==AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1)
        return -1; // Didn't find a video stream

    // Get a pointer to the codec context for the video stream
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodecCtx->refcounted_frames = 1; // referenced frames are not resused by the decoder and can be kept for display
   
    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL)
        return -1; // Codec not found

    // Inform the codec that we can handle truncated bitstreams -- i.e.,
    // bitstreams where frame boundaries can fall in the middle of packets
    if (pCodec->capabilities & CODEC_CAP_TRUNCATED)
        pCodecCtx->flags |= CODEC_FLAG_TRUNCATED;

    // Open codec
    if (avcodec_open2(pCodecCtx, pCodec, NULL)<0)
        return -1; // Could not open codec

    // Allocate video frame
    pFrame =  av_frame_alloc();

    struct fill_th fill_struct;
    fill_th_data = (fill_th*)&fill_struct;
    fill_th_data->pFormatCtx = pFormatCtx;
    fill_th_data->pCodecCtx = pCodecCtx;
    fill_th_data->videoStream = videoStream;
    fill_th_data->pFrame = pFrame;
    fill_th_data->b_new_frame = false;
    fill_th_data->b_fill_buf = true;

    pthread_mutex_init(&fill_th_data->fill_buff_mutex,NULL);
    pthread_cond_init(&fill_th_data->fill_buff_cond,NULL);
    pthread_mutex_init(&fill_th_data->new_frame_mutex,NULL);
    pthread_cond_init(&fill_th_data->new_frame_cond,NULL);

    err = 0;
    while(err == 0)// decode first frame to get resolutions
      err = GetNextFrame(fill_th_data->pFormatCtx, fill_th_data->pCodecCtx, fill_th_data->videoStream, fill_th_data->pFrame, fill_th_data->key_time);
   
#if 1
    pFrame_ref = av_frame_alloc();            
    av_frame_ref (pFrame_ref,fill_th_data->pFrame);  // add a reference to the frame (for display)    
    pthread_mutex_lock(&fill_th_data->fill_buff_mutex);
      fill_th_data->ref_frame.push_front(pFrame_ref);	
      //fill_th_data->b_new_frame = true;                        
    pthread_mutex_unlock(&fill_th_data->fill_buff_mutex);
#endif             
    //////////////// openGL inits //////////////////
    //init  YUV420P_GL (home made c++ class)   
    YUV420P_GL player; 
    player_ptr = &player;
    player.fps = pCodecCtx->time_base.den/(pCodecCtx->time_base.num*pCodecCtx->ticks_per_frame);
    fill_th_data->frame_rate = player.fps;
    player.screen_w = 640; // display window width
    player.pixel_w = pFrame->linesize[0];
    player.screen_h = 480; // display window height
    player.pixel_h = pFrame->height;
  
    th_id = pthread_create(&fill_buff_th,NULL,fill_buffer,(void*)fill_th_data);

    ///////////// buffer init ///////////////////////////////////
    usleep(BUF_LEN/player.fps*1000000);
    ///////////////////////////////////////////////////////////////

    //Init GLUT
    glutInit(&argc, argv);  
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA /*| GLUT_STENCIL | GLUT_DEPTH*/);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(player.screen_w, player.screen_h);
    glutCreateWindow("Pseudo-scalable video player");

    printf("Version: %s\n", glGetString(GL_VERSION));
    GLenum l = glewInit();
    glutDisplayFunc(&display_callback);
    glutTimerFunc(1/player.fps, timeFunc, 0); 
    glutKeyboardFunc(key);

    glutMainLoop();

    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}
