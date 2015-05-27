#include <stdio.h>


#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "YUV420p_GL.h"


#define ATTRIB_VERTEX 3
#define ATTRIB_TEXTURE 4

YUV420P_GL::YUV420P_GL()
  :fps(0)
  ,screen_w(0)
  ,screen_h(0)
  ,pixel_w(0)
  ,pixel_h(0)
  ,p(0)
  ,id_y(0)
  ,id_u(0)
  ,id_v(0)
  ,textureUniformY(0)
  ,textureUniformU(0)
  ,textureUniformV(0)
{
}

const int pixel_w = 640, pixel_h = 480;
//Bit per Pixel
const int bpp=12;
unsigned char buffer[pixel_w*pixel_h*bpp/8];
unsigned char buffer_convert[pixel_w*pixel_h*3];

inline unsigned char CONVERT_ADJUST(double tmp)
{
	return (unsigned char)((tmp >= 0 && tmp <= 255)?tmp:(tmp < 0 ? 0 : 255));
}

//YUV420P to RGB24
void CONVERT_YUV420PtoRGB24(unsigned char* y,unsigned char*u,unsigned char*v,unsigned char* rgb_dst,int nWidth,int nHeight)
{
	unsigned char *tmpbuf=(unsigned char *)malloc(nWidth*nHeight*3);
	unsigned char Y,U,V,R,G,B;
	unsigned char* y_planar,*u_planar,*v_planar;
	int rgb_width , u_width;
	rgb_width = nWidth * 3;
	u_width = (nWidth >> 1);
	int ypSize = nWidth * nHeight;
	int upSize = (ypSize>>2);
	int offSet = 0;

	y_planar = y;//yuv_src;
	u_planar = u;//yuv_src + ypSize;
	v_planar = v;//u_planar + upSize;

	for(int i = 0; i < nHeight; i++)
	{
		for(int j = 0; j < nWidth; j ++)
		{
			// Get the Y value from the y planar
			Y = *(y_planar + nWidth * i + j);
			// Get the V value from the u planar
			offSet = (i>>1) * (u_width) + (j>>1);
			V = *(u_planar + offSet);
			// Get the U value from the v planar
			U = *(v_planar + offSet);

			// Cacular the R,G,B values
			// Method 1
			R = CONVERT_ADJUST((Y + (1.4075 * (V - 128))));
			G = CONVERT_ADJUST((Y - (0.3455 * (U - 128) - 0.7169 * (V - 128))));
			B = CONVERT_ADJUST((Y + (1.7790 * (U - 128))));

			offSet = rgb_width * i + j * 3;

			rgb_dst[offSet] = B;
			rgb_dst[offSet + 1] = G;
			rgb_dst[offSet + 2] = R;
		}
	}
	free(tmpbuf);
}

void YUV420P_GL::drawString(char *string)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();             
    glLoadIdentity();   
    int w = glutGet( GLUT_WINDOW_WIDTH );
    int h = glutGet( GLUT_WINDOW_HEIGHT );
    glOrtho( 0, w, 0, h, -1, 1 );

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable( GL_DEPTH_TEST ); 

    glDisable( GL_LIGHTING );
    glColor3f(1, 1, 1);

    glRasterPos2i(20, 20);
    void *font = GLUT_BITMAP_HELVETICA_18; 
    for (char* c=string; *c != '\0'; c++) 
    {
        glutBitmapCharacter(font, *c); 
    }

    glEnable( GL_LIGHTING );

    glEnable (GL_DEPTH_TEST);     

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();  
}

void YUV420P_GL::display(char *string){

	glRasterPos3f(-1.0f,1.0f,0);
	//Zoom, Flip
	glPixelZoom((float)screen_w/(float)pixel_w, -(float)screen_h/(float)pixel_h);

	CONVERT_YUV420PtoRGB24(plane[0],plane[1],plane[2],buffer_convert,pixel_w,pixel_h);
	glDrawPixels(pixel_w, pixel_h,GL_RGB, GL_UNSIGNED_BYTE, buffer_convert);

        drawString(string);

	//GLUT_DOUBLE
	glutSwapBuffers();

}


