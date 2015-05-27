#ifndef YUV420P_GL_H
#define YUV420P_GL_H

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glut.h>

class YUV420P_GL {

 public:
  YUV420P_GL();
  void display(char *string);
  void drawString(char *string);

 private:
  char *textFileRead(char * filename);
  
 public:
  float fps;
  int screen_w;
  int screen_h;
  int pixel_w;
  int pixel_h;
  unsigned char *plane[3];
  GLuint p;                
  GLuint id_y;
  GLuint id_u;
  GLuint id_v; // Texture id
  GLuint textureUniformY;
  GLuint textureUniformU;
  GLuint textureUniformV;
};


#endif
